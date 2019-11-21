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
#include "H264AVCSampleProvider.h"

using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	static constexpr size_t AVC_CONFIG_MIN_SIZE = 7;

	H264AVCSampleProvider::H264AVCSampleProvider(_In_ const AVStream* stream, _Inout_ FFmpegReader& reader) :
		MediaSampleProvider(stream, reader)
	{
		THROW_HR_IF_NULL(MF_E_INVALID_FILE_FORMAT, m_stream->codecpar->extradata);

		// Determine whether the bitstream format is AVC or Annex B
		m_isAVC = m_stream->codecpar->extradata_size < 1 || m_stream->codecpar->extradata[0] == 1;

		if (m_isAVC)
		{
			THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, m_stream->codecpar->extradata_size < AVC_CONFIG_MIN_SIZE);
			m_nalLenSize = m_stream->codecpar->extradata[4] & 0x03 + 1;
		}
		else
		{
			m_nalLenSize = 0;
		}
	}

	tuple<IBuffer, int64_t, int64_t> H264AVCSampleProvider::GetSampleData()
	{
		// DataWriter is only used if we need to append SPS/PPS data to the sample or perform Annex B conversions
		DataWriter dataWriter{ nullptr };

		AVPacket_ptr packet{ GetPacket() };

		// On a key frames write the SPS/PPS data
		if (packet->flags & AV_PKT_FLAG_KEY)
		{
			WriteSPSAndPPS(dataWriter, m_stream->codecpar->extradata, m_stream->codecpar->extradata_size);
		}

		// Check for new SPS/PPS data on the sample
		for (int i = 0; i < packet->side_data_elems; i++)
		{
			if (packet->side_data[i].type == AV_PKT_DATA_NEW_EXTRADATA)
			{
				WriteSPSAndPPS(dataWriter, packet->side_data[i].data, packet->side_data[i].size);
				break;
			}
		}

		if (m_isAVC || dataWriter != nullptr)
		{
			WriteNALPacket(dataWriter, packet->data, packet->size);

			return { dataWriter.DetachBuffer(), packet->pts, packet->duration };
		}
		else
		{
			return { make<FFmpegBuffer>(packet.get()), packet->pts, packet->duration };
		}
	}

	void H264AVCSampleProvider::WriteSPSAndPPS(
		_Inout_opt_ DataWriter& dataWriter,
		_In_reads_(codecPrivateDataSize) const uint8_t* codecPrivateData,
		_In_ uint32_t codecPrivateDataSize)
	{
		if (m_isAVC)
		{
			THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, codecPrivateDataSize < AVC_CONFIG_MIN_SIZE);

			// Convert SPS/PPS data to Annex B format
			uint32_t pos = 4;

			uint8_t spsCount = codecPrivateData[pos++] & 0x1F;
			pos += WriteParameterSetData(dataWriter, spsCount, codecPrivateData + pos, codecPrivateDataSize - pos);

			THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, pos + 1 > codecPrivateDataSize);
			uint8_t ppsCount = codecPrivateData[pos++];
			WriteParameterSetData(dataWriter, ppsCount, codecPrivateData + pos, codecPrivateDataSize - pos);
		}
		else
		{
			// Copy SPS/PPS data as is
			dataWriter.WriteBytes({ codecPrivateData, codecPrivateData + codecPrivateDataSize });
		}
	}

	uint32_t H264AVCSampleProvider::WriteParameterSetData(
		_Inout_opt_ DataWriter& dataWriter,
		_In_ uint8_t parameterSetCount,
		_In_reads_(parameterSetDataSize) const uint8_t* parameterSetData,
		_In_ uint32_t parameterSetDataSize)
	{
		// Convert the parameter set data to Annex B format
		uint32_t pos = 0;
		for (uint8_t i = 0; i < parameterSetCount; i++)
		{
			// Get the NAL length
			THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, pos + 2 > parameterSetDataSize);
			uint16_t nalLen = (parameterSetData[pos] << 8) + parameterSetData[pos + 1];
			pos += 2;

			THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, pos + nalLen > parameterSetDataSize);

			// Write the NAL unit start code
			dataWriter.WriteBytes({ 0x00, 0x00, 0x00, 0x01 });

			// Write the NAL unit data
			dataWriter.WriteBytes({ parameterSetData + pos, parameterSetData + pos + nalLen });
			pos += nalLen;
		}

		return pos; // Return the number of bytes read
	}

	void H264AVCSampleProvider::WriteNALPacket(
		_Inout_opt_ DataWriter& dataWriter,
		_In_reads_(packetDataSize) const uint8_t* packetData,
		_In_ uint32_t packetDataSize)
	{
		if (m_isAVC)
		{
			// Convert packet data to Annex B format
			uint64_t pos = 0;

			do
			{
				// Get the NAL length
				uint32_t nalLen = 0;

				THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, pos + m_nalLenSize > packetDataSize);
				for (uint8_t i = 0; i < m_nalLenSize; i++)
				{
					nalLen <<= 8;
					nalLen += packetData[pos++];
				}

				THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, pos + nalLen > packetDataSize);

				// Write the NAL unit start code
				dataWriter.WriteBytes({ 0x00, 0x00, 0x00, 0x01 });

				// Write the NAL unit data
				dataWriter.WriteBytes({ packetData + pos, packetData + pos + nalLen });
				pos += nalLen;
			} while (pos < packetDataSize);
		}
		else
		{
			// Copy packet data as is
			dataWriter.WriteBytes({ packetData, packetData + packetDataSize });
		}
	}
}
