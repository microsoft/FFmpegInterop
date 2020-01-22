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
#include "H264SampleProvider.h"

using namespace winrt::FFmpegInterop::implementation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace
{
	uint32_t ReadNaluLength(_In_ uint8_t naluLengthSize, _In_reads_(dataSize) const uint8_t* data, _In_ int dataSize)
	{
		THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, naluLengthSize > dataSize);

		switch (naluLengthSize)
		{
		case 1:
			return data[0];

		case 2:
			return data[0] << 8 | data[1];

		case 3:
			return data[0] << 16 | data[1] << 8 | data[2];

		case 4:
			return data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];

		default:
			THROW_HR(MF_E_INVALID_FILE_FORMAT);
		}
	}
}

H264SampleProvider::H264SampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
	SampleProvider(stream, reader),
	m_avcCodecPrivate(m_stream->codecpar->extradata, m_stream->codecpar->extradata_size)
{
	// TODO: We currently only support the AVC bitstream format. Add support for Annex B
}

void H264SampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
{
	const AVCodecParameters* codecPar{ m_stream->codecpar };

	SampleProvider::SetEncodingProperties(encProp, setFormatUserData);

	VideoEncodingProperties videoEncProp{ encProp.as<VideoEncodingProperties>() };
	videoEncProp.ProfileId(codecPar->profile);

	MediaPropertySet videoProp{ videoEncProp.Properties() };
	videoProp.Insert(MF_MT_MPEG2_LEVEL, PropertyValue::CreateUInt32(static_cast<uint32_t>(codecPar->level)));
	videoProp.Insert(MF_NALU_LENGTH_SET, PropertyValue::CreateUInt32(static_cast<uint32_t>(true)));
	videoProp.Insert(MF_MT_MPEG_SEQUENCE_HEADER, PropertyValue::CreateUInt8Array({ m_avcCodecPrivate.GetSpsPpsData().data(), m_avcCodecPrivate.GetSpsPpsData().data() + m_avcCodecPrivate.GetSpsPpsData().size() }));

	// TODO: Set MF_MT_VIDEO_H264_NO_FMOASO
}

tuple<IBuffer, int64_t, int64_t, map<GUID, IInspectable>> H264SampleProvider::GetSampleData()
{
	// Get the next sample
	AVPacket_ptr packet{ GetPacket() };

	const int64_t pts{ packet->pts };
	const int64_t dur{ packet->duration };

	// Convert the sample to Annex B format by replacing NAL lengths with the NALU start code.
	// Prepend SPS/PPS data to the sample if this is a key frame.
	auto [buf, naluLengths] = TransformSample(move(packet));

	map<GUID, IInspectable> properties;

	// Set the NALU length info
	const uint8_t* naluLengthsBuf{ reinterpret_cast<const uint8_t*>(naluLengths.data()) };
	const size_t naluLengthsBufSize{ sizeof(decltype(naluLengths)::value_type) * naluLengths.size() };
	properties[MF_NALU_LENGTH_INFORMATION] = PropertyValue::CreateUInt8Array({ naluLengthsBuf, naluLengthsBuf + naluLengthsBufSize });

	return { move(buf), pts, dur, move(properties) };
}

tuple<IBuffer, vector<uint32_t>> H264SampleProvider::TransformSample(_Inout_ AVPacket_ptr packet)
{
	// Check if we'll need to write the transformed sample to a new buffer
	const bool isKeyFrame{ (packet->flags & AV_PKT_FLAG_KEY) != 0 };
	const bool canDoInplaceTransform{ m_avcCodecPrivate.GetNaluLengthSize() == sizeof(NALU_START_CODE) };
	const bool writeToBuf{ isKeyFrame || canDoInplaceTransform };

	vector<uint8_t> buf;
	if (writeToBuf)
	{
		// Reserve space for the transformed sample now to avoid reallocations
		size_t sampleSize{ static_cast<uint32_t>(packet->size) };

		if (isKeyFrame)
		{
			// Add space for SPS/PPS data
			sampleSize += m_avcCodecPrivate.GetSpsPpsData().size();
		}

		if (!canDoInplaceTransform)
		{
			// Add additional space since NALU start code is longer than NAL length size
			sampleSize += 64 * (sizeof(NALU_START_CODE) - m_avcCodecPrivate.GetNaluLengthSize());
		}

		buf.reserve(sampleSize);
	}

	// Convert the sample to Annex B format by replacing NAL lengths with the NALU start code. Prepend SPS/PPS data if this is a key frame.
	vector<uint32_t> naluLengths;
	bool needToCopySpsPpsData{ isKeyFrame };

	for (int pos = 0; pos < packet->size;)
	{
		uint32_t naluLength{ ReadNaluLength(m_avcCodecPrivate.GetNaluLengthSize(), packet->data + pos, packet->size - pos) };
		THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, pos + m_avcCodecPrivate.GetNaluLengthSize() + naluLength > static_cast<uint32_t>(packet->size));

		if (needToCopySpsPpsData)
		{
			// Check if the first NALU is an AUD (access unit delimiter). The AUD must be the first NALU in the sample.
			if (packet->data[m_avcCodecPrivate.GetNaluLengthSize()] != NALU_TYPE_AUD)
			{
				// Copy the SPS/PPS data
				const auto& spsPpsData{ m_avcCodecPrivate.GetSpsPpsData() };
				buf.insert(buf.end(), spsPpsData.begin(), spsPpsData.end());

				const auto& spsPpsNaluLengths{ m_avcCodecPrivate.GetSpsPpsNaluLengths() };
				naluLengths.insert(naluLengths.end(), spsPpsNaluLengths.begin(), spsPpsNaluLengths.end());

				needToCopySpsPpsData = false;
			}
		}

		if (writeToBuf)
		{
			// Write the NALU start code and data
			const uint8_t* naluData{ packet->data + pos + m_avcCodecPrivate.GetNaluLengthSize() };
			buf.insert(buf.end(), begin(NALU_START_CODE), end(NALU_START_CODE));
			buf.insert(buf.end(), naluData, naluData + naluLength);
		}
		else
		{
			// Replace the NAL length with the NALU start code
			copy(begin(NALU_START_CODE), end(NALU_START_CODE), packet->data + pos);
		}

		// Save the NALU length
		naluLengths.push_back(sizeof(NALU_START_CODE) + naluLength);

		if (needToCopySpsPpsData)
		{
			// Copy the SPS/PPS data
			const auto& spsPpsData{ m_avcCodecPrivate.GetSpsPpsData() };
			buf.insert(buf.end(), spsPpsData.begin(), spsPpsData.end());

			const auto& spsPpsNaluLengths{ m_avcCodecPrivate.GetSpsPpsNaluLengths() };
			naluLengths.insert(naluLengths.end(), spsPpsNaluLengths.begin(), spsPpsNaluLengths.end());

			needToCopySpsPpsData = false;
		}

		pos += m_avcCodecPrivate.GetNaluLengthSize() + naluLength;
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

H264SampleProvider::AVCCodecPrivate::AVCCodecPrivate(_In_reads_(codecPrivateDataSize) const uint8_t* codecPrivateData, _In_ int codecPrivateDataSize)
{
	// Make sure the bitstream format is AVC
	THROW_HR_IF_NULL(MF_E_INVALID_FILE_FORMAT, codecPrivateData);
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, codecPrivateDataSize < MIN_SIZE);
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, codecPrivateData[0] != 1);

	// Parse profile, level, and NALU length size
	m_profile = codecPrivateData[1];
	m_level = codecPrivateData[3];
	m_naluLengthSize = codecPrivateData[4] & 0x03 + 1;

	// Parse SPS/PPS data
	uint32_t pos = 5;

	uint8_t spsCount = codecPrivateData[pos++] & 0x1F;
	pos += ParseParameterSets(spsCount, codecPrivateData + pos, codecPrivateDataSize - pos);

	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, pos + 1 > static_cast<uint32_t>(codecPrivateDataSize));
	uint8_t ppsCount = codecPrivateData[pos++];
	ParseParameterSets(ppsCount, codecPrivateData + pos, codecPrivateDataSize - pos);
}

uint32_t H264SampleProvider::AVCCodecPrivate::ParseParameterSets(
	_In_ uint8_t parameterSetCount,
	_In_reads_(codecPrivateDataSize) const uint8_t* codecPrivateData,
	_In_ uint32_t codecPrivateDataSize)
{
	// Reserve estimated space now to minimize reallocations
	m_spsPpsNaluLengths.reserve(m_spsPpsNaluLengths.size() + parameterSetCount);
	m_spsPpsData.reserve(m_spsPpsData.size() + codecPrivateDataSize + 2 * static_cast<size_t>(parameterSetCount));

	// Parse the parameter sets and convert the SPS/PPS data to Annex B format
	uint32_t pos = 0;
	for (uint8_t i = 0; i < parameterSetCount; i++)
	{
		// Get the NALU length
		uint32_t naluLength{ ReadNaluLength(2, codecPrivateData + pos, codecPrivateDataSize - pos) };
		pos += 2;

		THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, pos + naluLength > codecPrivateDataSize);
		m_spsPpsNaluLengths.push_back(sizeof(NALU_START_CODE) + naluLength);

		// Write the NALU start code and SPS/PPS data to the buffer
		m_spsPpsData.insert(m_spsPpsData.end(), begin(NALU_START_CODE), end(NALU_START_CODE));
		m_spsPpsData.insert(m_spsPpsData.end(), codecPrivateData + pos, codecPrivateData + pos + naluLength);

		pos += naluLength;
	}

	return pos; // Return the number of bytes read
}

H264SampleProvider::AVCSequenceParameterSet::AVCSequenceParameterSet(_In_reads_(dataSize) const uint8_t* data, _In_ int dataSize)
{
	THROW_HR_IF_NULL(MF_E_INVALID_FILE_FORMAT, data);
	THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, dataSize < 3);

	m_profile = data[1];
		
	uint8_t profileCompatibility{ data[2] };
	m_constraintSet1 = (profileCompatibility & (1 << 6)) != 0;
}

H264SampleProvider::AVCPictureParamterSet::AVCPictureParamterSet(_In_reads_(dataSize) const uint8_t* data, _In_ int dataSize)
{
	// TODO: Implement
	m_numSliceGroups = 0;
}
