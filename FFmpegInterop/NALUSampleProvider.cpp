//*****************************************************************************
//
//	Copyright 2015 Microsoft Corporation
//
//	Licensed under the Apache License, Version 2.0 (the "License");
//	you may not use this file except in compliance with the License.
//	You may obtain a copy of the License at
//
//	http ://www.apache.org/licenses/LICENSE-2.0
//
//	Unless required by applicable law or agreed to in writing, software
//	distributed under the License is distributed on an "AS IS" BASIS,
//	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//	See the License for the specific language governing permissions and
//	limitations under the License.
//
//*****************************************************************************

#include "pch.h"
#include "NALUSampleProvider.h"

using namespace winrt::FFmpegInterop::implementation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

uint32_t winrt::FFmpegInterop::implementation::GetAnnexBNaluLength(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize)
{
	// Make sure data starts with a NALU start code
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, dataSize < sizeof(NALU_START_CODE));
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, equal(begin(NALU_START_CODE), end(NALU_START_CODE), data));

	// Scan for the next NALU start code
	for (uint32_t i{ 2 * sizeof(NALU_START_CODE) }; i < dataSize; i++)
	{
		if (equal(begin(NALU_START_CODE), end(NALU_START_CODE), data + i - sizeof(NALU_START_CODE)))
		{
			// Found next NALU start code
			return i;
		}
	}

	// No more NALU start codes found
	return dataSize;
}

uint32_t winrt::FFmpegInterop::implementation::GetAVCNaluLength(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize, _In_ uint8_t naluLengthSize)
{
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, naluLengthSize > dataSize);

	uint32_t naluLength{ 0 };
	switch (naluLengthSize)
	{
	case 1:
		naluLength = data[0];
		break;

	case 2:
		naluLength = _byteswap_ushort(*reinterpret_cast<const uint16_t*>(data));
		break;

	case 3:
		naluLength = data[0] << 16 | data[1] << 8 | data[2];
		break;

	case 4:
		naluLength = _byteswap_ulong(*reinterpret_cast<const uint32_t*>(data));
		break;

	default:
		THROW_HR(MF_E_INVALID_FILE_FORMAT);
	}

	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, naluLengthSize + naluLength > dataSize);

	return naluLength;
}

NALUSampleProvider::NALUSampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
	SampleProvider(stream, reader)
{
	
}

void NALUSampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
{
	SampleProvider::SetEncodingProperties(encProp, setFormatUserData);

	VideoEncodingProperties videoEncProp{ encProp.as<VideoEncodingProperties>() };
	videoEncProp.ProfileId(m_stream->codecpar->profile);

	MediaPropertySet videoProp{ videoEncProp.Properties() };
	videoProp.Insert(MF_MT_MPEG2_LEVEL, PropertyValue::CreateUInt32(static_cast<uint32_t>(m_stream->codecpar->level)));
	videoProp.Insert(MF_NALU_LENGTH_SET, PropertyValue::CreateUInt32(static_cast<uint32_t>(true)));

	videoProp.Insert(MF_MT_MPEG_SEQUENCE_HEADER, PropertyValue::CreateUInt8Array({ m_spsPpsData.data(), m_spsPpsData.data() + m_spsPpsData.size() }));
}

tuple<IBuffer, int64_t, int64_t, map<GUID, IInspectable>> NALUSampleProvider::GetSampleData()
{
	// Get the next sample
	AVPacket_ptr packet{ GetPacket() };

	const int64_t pts{ packet->pts };
	const int64_t dur{ packet->duration };

	// Transform the sample into the format expected by the decoder
	auto [buf, naluLengths] = TransformSample(move(packet));

	map<GUID, IInspectable> properties;

	// Set the NALU length info
	const uint8_t* naluLengthsBuf{ reinterpret_cast<const uint8_t*>(naluLengths.data()) };
	const size_t naluLengthsBufSize{ sizeof(decltype(naluLengths)::value_type) * naluLengths.size() };
	properties[MF_NALU_LENGTH_INFORMATION] = PropertyValue::CreateUInt8Array({ naluLengthsBuf, naluLengthsBuf + naluLengthsBufSize });

	return { move(buf), pts, dur, move(properties) };
}

tuple<IBuffer, vector<uint32_t>> NALUSampleProvider::TransformSample(_Inout_ AVPacket_ptr packet)
{
	const bool isKeyFrame{ (packet->flags & AV_PKT_FLAG_KEY) != 0 };
	bool copySpsPpsData{ isKeyFrame && !m_spsPpsData.empty() };
	const bool writeToBuf{ copySpsPpsData || (!m_isBitstreamAnnexB && m_naluLengthSize != sizeof(NALU_START_CODE)) };

	vector<uint8_t> buf;
	if (writeToBuf)
	{
		size_t sampleSize{ static_cast<uint32_t>(packet->size) };

		if (isKeyFrame)
		{
			// Reserve space for SPS/PPS data
			sampleSize += m_spsPpsData.size();
		}

		if (!m_isBitstreamAnnexB && m_naluLengthSize > sizeof(NALU_START_CODE))
		{
			// Reserve additional space since NALU start code is longer than NALU length size
			sampleSize += MAX_NALU_NUM_SUPPORTED * (sizeof(NALU_START_CODE) - m_naluLengthSize);
		}

		buf.reserve(sampleSize);
	}

	vector<uint32_t> naluLengths;
	naluLengths.reserve(MAX_NALU_NUM_SUPPORTED);

	const uint8_t naluPrefixLength{ m_isBitstreamAnnexB ? sizeof(NALU_START_CODE) : m_naluLengthSize };
	for (uint32_t i{ 0 }; i + naluPrefixLength <= static_cast<uint32_t>(packet->size);)
	{
		// Get the length of the next NALU
		uint32_t naluLength{ 0 };
		if (m_isBitstreamAnnexB)
		{
			naluLength = GetAnnexBNaluLength(packet->data + i, packet->size - i);
		}
		else
		{
			naluLength = GetAVCNaluLength(packet->data + i, packet->size - i, m_naluLengthSize);
		}

		if (copySpsPpsData)
		{
			// Check if this NALU is an AUD (access unit delimiter). The AUD must be the first NALU in the sample.
			if (naluLength < naluPrefixLength ||  packet->data[naluPrefixLength] != NALU_TYPE_AUD)
			{
				// Copy the SPS/PPS data
				buf.insert(buf.end(), m_spsPpsData.data(), m_spsPpsData.data() + m_spsPpsData.size());

				// Save the SPS/PPS NALU lengths
				naluLengths.insert(naluLengths.end(), m_spsPpsNaluLengths.begin(), m_spsPpsNaluLengths.end());

				copySpsPpsData = false;
			}
		}
		
		if (writeToBuf)
		{
			// Write the NALU start code and data
			const uint8_t* naluData{ packet->data + i + naluPrefixLength };
			buf.insert(buf.end(), begin(NALU_START_CODE), end(NALU_START_CODE));
			buf.insert(buf.end(), naluData, naluData + naluLength);
		}
		else if (!m_isBitstreamAnnexB)
		{
			// Replace the NALU length with the NALU start code
			copy(begin(NALU_START_CODE), end(NALU_START_CODE), packet->data + i);
		}

		// Save the NALU length
		naluLengths.push_back(sizeof(NALU_START_CODE) + naluLength);

		i += naluPrefixLength + naluLength;
	}

	if (copySpsPpsData)
	{
		// Copy the SPS/PPS data
		buf.insert(buf.end(), m_spsPpsData.data(), m_spsPpsData.data() + m_spsPpsData.size());

		// Save the SPS/PPS NALU lengths
		naluLengths.insert(naluLengths.end(), m_spsPpsNaluLengths.begin(), m_spsPpsNaluLengths.end());

		copySpsPpsData = false;
	}

	if (writeToBuf)
	{
		return { make<FFmpegInteropBuffer>(move(buf)), move(naluLengths) };
	}
	else
	{
		return { make<FFmpegInteropBuffer>(move(packet)), move(naluLengths) };
	}
}

AnnexBParser::AnnexBParser(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize) :
	m_data(data),
	m_dataSize(dataSize)
{
	// Validate parameters
	WINRT_ASSERT(m_data != nullptr);
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, m_dataSize < sizeof(NALU_START_CODE));
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, !equal(begin(NALU_START_CODE), end(NALU_START_CODE), m_data));
}

tuple<vector<uint8_t>, vector<uint32_t>> AnnexBParser::GetSpsPpsData() const
{
	vector<uint8_t> spsPpsData{ m_data, m_data + m_dataSize };
	vector<uint32_t> spsPpsNaluLengths;

	for (uint32_t i{ 0 }; i < m_dataSize;)
	{
		uint32_t naluLength{ GetAnnexBNaluLength(m_data + i, m_dataSize - i) };

		// Save the NALU length
		spsPpsNaluLengths.push_back(naluLength);

		i += naluLength;
	}

	return { spsPpsData, spsPpsNaluLengths };
}
