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

extern "C"
{
#include <libavformat/avformat.h>
}

using namespace FFmpegInterop;
using namespace winrt;
using namespace winrt::Windows::Storage::Streams;

H264AVCSampleProvider::H264AVCSampleProvider(FFmpegReader& reader, const AVFormatContext* avFormatCtx, AVCodecContext* avCodecCtx) :
	MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
{
}

HRESULT H264AVCSampleProvider::WriteAVPacketToStream(const DataWriter& dataWriter, const AVPacket_ptr& packet)
{
	HRESULT hr = S_OK;

	// On a KeyFrame, write the SPS and PPS
	if (packet->flags & AV_PKT_FLAG_KEY)
	{
		hr = GetSPSAndPPSBuffer(dataWriter);
	}

	if (SUCCEEDED(hr))
	{
		// Convert the packet to NAL format
		hr = WriteNALPacket(dataWriter, packet);
	}

	// We have a complete frame
	return hr;
}

HRESULT H264AVCSampleProvider::GetSPSAndPPSBuffer(const DataWriter& dataWriter)
{
	HRESULT hr = S_OK;
	int spsLength = 0;
	int ppsLength = 0;

	if (!m_pAvCodecCtx)
	{
		return E_FAIL;
	}

	// Get the position of the SPS
	if (m_pAvCodecCtx->extradata == nullptr && m_pAvCodecCtx->extradata_size < 8)
	{
		// The data isn't present
		hr = E_FAIL;
	}

	if (SUCCEEDED(hr))
	{
		byte* spsPos = m_pAvCodecCtx->extradata + 8;
		spsLength = spsPos[-1];

		if (m_pAvCodecCtx->extradata_size < (8 + spsLength))
		{
			// We don't have a complete SPS
			hr = E_FAIL;
		}
		else
		{
			// Write the NAL unit for the SPS
			dataWriter.WriteByte(0);
			dataWriter.WriteByte(0);
			dataWriter.WriteByte(0);
			dataWriter.WriteByte(1);

			// Write the SPS
			dataWriter.WriteBytes(array_view<const byte>(spsPos, spsPos + spsLength));
		}
	}

	if (SUCCEEDED(hr))
	{
		if (m_pAvCodecCtx->extradata_size < (8 + spsLength + 3))
		{
			hr = E_FAIL;
		}

		if (SUCCEEDED(hr))
		{
			byte* ppsPos = m_pAvCodecCtx->extradata + 8 + spsLength + 3;
			ppsLength = ppsPos[-1];

			if (m_pAvCodecCtx->extradata_size < (8 + spsLength + 3 + ppsLength))
			{
				hr = E_FAIL;
			}
			else
			{
				// Write the NAL unit for the PPS
				dataWriter.WriteByte(0);
				dataWriter.WriteByte(0);
				dataWriter.WriteByte(0);
				dataWriter.WriteByte(1);

				// Write the PPS
				dataWriter.WriteBytes(array_view<const byte>(ppsPos, ppsPos + ppsLength));
			}
		}
	}

	return hr;
}

// Write out an H.264 packet converting stream offsets to start-codes
HRESULT H264AVCSampleProvider::WriteNALPacket(const DataWriter& dataWriter, const AVPacket_ptr& packet)
{
	HRESULT hr = S_OK;
	uint32_t index = 0;
	const uint32_t packetSize = (uint32_t) packet->size;

	do
	{
		// Make sure we have enough data
		if (packetSize < (index + 4))
		{
			hr = E_FAIL;
			break;
		}

		// Grab the size of the blob
		const uint32_t size = (packet->data[index] << 24) + (packet->data[index + 1] << 16) + (packet->data[index + 2] << 8) + packet->data[index + 3];
		index += 4;

		// Stop if index and size goes beyond packet size or overflow
		if (packetSize < (index + size) || (UINT32_MAX - index) < size)
		{
			hr = E_FAIL;
			break;
		}

		// Write the NAL unit to the stream
		dataWriter.WriteByte(0);
		dataWriter.WriteByte(0);
		dataWriter.WriteByte(0);
		dataWriter.WriteByte(1);

		// Write the rest of the packet to the stream
		dataWriter.WriteBytes(array_view<const byte>(&packet->data[index], &packet->data[index + size]));
		index += size;
	} while (index < packetSize);

	return hr;
}
