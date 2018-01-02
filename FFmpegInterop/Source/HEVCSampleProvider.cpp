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
#include "HEVCSampleProvider.h"

using namespace FFmpegInterop;

HEVCSampleProvider::HEVCSampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx)
	: MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
{
}

HEVCSampleProvider::~HEVCSampleProvider()
{
}

HRESULT HEVCSampleProvider::WriteAVPacketToStream(DataWriter^ dataWriter, AVPacket* avPacket)
{
	HRESULT hr = S_OK;
	// On first frame, write the SPS and PPS
	if (!m_bHasSentExtradata)
	{
		hr = GetSPSAndPPSBuffer(dataWriter, m_pAvCodecCtx->extradata, m_pAvCodecCtx->extradata_size);
		m_bHasSentExtradata = true;
	}

	if (SUCCEEDED(hr))
	{
		// Check for extradata changes during playback
		for (int i = 0; i < avPacket->side_data_elems; i++)
		{
			if (avPacket->side_data[i].type == AV_PKT_DATA_NEW_EXTRADATA)
			{
				hr = GetSPSAndPPSBuffer(dataWriter, avPacket->side_data[i].data, avPacket->side_data[i].size);
			}
		}

		if (m_bIsRawNalStream)
		{
			// Just write NAL stream to output
			auto data = Platform::ArrayReference<uint8_t>(avPacket->data, avPacket->size);
			dataWriter->WriteBytes(data);
		}
		else
		{
			// Convert the packet to NAL format
			hr = WriteNALPacket(dataWriter, avPacket);
		}
	}

	// We have a complete frame
	return hr;
}

HRESULT HEVCSampleProvider::GetSPSAndPPSBuffer(DataWriter^ dataWriter, byte* buf, int length)
{
	HRESULT hr = S_OK;
	int spsLength = 0;
	int ppsLength = 0;

	// Get the position of the SPS
	if (buf == nullptr || length < 4)
	{
		// The data isn't present
		hr = E_FAIL;
	}
	if (SUCCEEDED(hr))
	{
		if (length > 22 && (buf[0] || buf[1] || buf[2] > 1)) {
			/* Extradata is in hvcC format */
			int i, j, num_arrays;
			int pos = 21;

			m_nalLenSize = (buf[pos++] & 3) + 1;
			num_arrays = buf[pos++];

			/* Decode nal units from hvcC. */
			for (i = 0; i < num_arrays; i++) {
				int type = buf[pos++] & 0x3f;
				int cnt = ReadMultiByteValue(buf, pos, 2);
				pos += 2;

				for (j = 0; j < cnt; j++) {
					int nalsize = ReadMultiByteValue(buf, pos, 2);
					pos += 2;

					if (length - pos < nalsize) {
						return E_FAIL;
					}

					// Write the NAL unit to the stream
					dataWriter->WriteByte(0);
					dataWriter->WriteByte(0);
					dataWriter->WriteByte(0);
					dataWriter->WriteByte(1);

					auto data = Platform::ArrayReference<uint8_t>(buf + pos, nalsize);
					dataWriter->WriteBytes(data);

					pos += nalsize;
				}
			}
		}
		else 
		{
			/* The stream and extradata contains raw NAL packets. No decoding needed. */
			auto extra = Platform::ArrayReference<uint8_t>(buf, length);
			dataWriter->WriteBytes(extra);
			m_bIsRawNalStream = true;
		}
	}

	return hr;
}

// Write out an HEVC packet converting stream offsets to start-codes
HRESULT HEVCSampleProvider::WriteNALPacket(DataWriter^ dataWriter, AVPacket* avPacket)
{
	HRESULT hr = S_OK;
	uint32 index = 0;
	uint32 size = 0;
	uint32 packetSize = (uint32)avPacket->size;

	do
	{
		// Make sure we have enough data
		if (packetSize < (index + m_nalLenSize))
		{
			hr = E_FAIL;
			break;
		}

		// Grab the size of the blob
		size = ReadMultiByteValue(avPacket->data, index, m_nalLenSize);
		index += m_nalLenSize;

		// Write the NAL unit to the stream
		dataWriter->WriteByte(0);
		dataWriter->WriteByte(0);
		dataWriter->WriteByte(0);
		dataWriter->WriteByte(1);

		// Stop if index and size goes beyond packet size or overflow
		if (packetSize < (index + size) || (UINT32_MAX - index) < size)
		{
			hr = E_FAIL;
			break;
		}

		// Write the rest of the packet to the stream
		auto vBuffer = Platform::ArrayReference<uint8_t>(&(avPacket->data[index]), size);
		dataWriter->WriteBytes(vBuffer);
		index += size;
	} while (index < packetSize);

	return hr;
}

int HEVCSampleProvider::ReadMultiByteValue(byte* buffer, int index, int numBytes)
{
	if (numBytes == 4)
	{
		return (buffer[index] << 24) + (buffer[index + 1] << 16) + (buffer[index + 2] << 8) + buffer[index + 3];
	}
	if (numBytes == 3)
	{
		return (buffer[index] << 16) + (buffer[index + 1] << 8) + buffer[index + 2];
	}
	if (numBytes == 2)
	{
		return (buffer[index] << 8) + buffer[index + 1];
	}
	if (numBytes == 1)
	{
		return (buffer[index]);
	}
	return -1;
}
