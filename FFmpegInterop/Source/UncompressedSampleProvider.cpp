//*****************************************************************************
//
//	Copyright 2016 Microsoft Corporation
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
#include "UncompressedSampleProvider.h"

using namespace FFmpegInterop;

UncompressedSampleProvider::UncompressedSampleProvider(FFmpegReader^ reader, AVFormatContext* avFormatCtx, AVCodecContext* avCodecCtx)
	: MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
	, m_pAvFrame(nullptr)
{
}

HRESULT UncompressedSampleProvider::ProcessDecodedFrame(DataWriter^ dataWriter)
{
	return S_OK;
}

// Return S_FALSE for an incomplete frame
HRESULT UncompressedSampleProvider::GetFrameFromFFmpegDecoder(AVPacket* avPacket, bool& consumed)
{
	HRESULT hr = S_OK;
	consumed = true;
	int decodeFrame = 0;

	int sendPacketResult = avcodec_send_packet(m_pAvCodecCtx, avPacket);
	if (sendPacketResult == AVERROR(EAGAIN))
	{
		// We couldn't feed the packet to the decoder.
		// Keep it for next time
		consumed = false;
	}
	else if (sendPacketResult < 0)
	{
		// We failed to send the packet
		hr = E_FAIL;
		DebugMessage(L"Decoder failed on the sample\n");
	}

	if(SUCCEEDED(hr))
	{
		// Try to get a frame from the decoder.
		decodeFrame = avcodec_receive_frame(m_pAvCodecCtx, m_pAvFrame);

		// The decoder is empty, send a packet to it.
		if (decodeFrame == AVERROR(EAGAIN))
		{
			// The decoder doesn't have enough data to produce a frame,
			// return S_FALSE to indicate a partial frame
			hr = S_FALSE;
		}
		else if (decodeFrame < 0)
		{
			hr = E_FAIL;
			DebugMessage(L"Failed to get a frame from the decoder\n");
		}
	}

	return hr;
}

HRESULT UncompressedSampleProvider::DecodeAVPacket(DataWriter^ dataWriter, AVPacket* avPacket, int64_t& framePts, int64_t& frameDuration)
{
	HRESULT hr = S_OK;

	bool consumed = false;
	hr = GetFrameFromFFmpegDecoder(avPacket, consumed);
	// Update the timestamp if the packet has one
	if (m_pAvFrame->pts != AV_NOPTS_VALUE)
	{
		framePts = m_pAvFrame->pts;
		frameDuration = m_pAvFrame->pkt_duration;
	}

	// If we didn't consume the packet, put it back in the queue
	if (!consumed)
	{
		HeadQueuePacket(*avPacket);
	}

	// If we couldn't get a complete frame (S_FALSE), don't process the frame yet
	if (SUCCEEDED(hr) && hr != S_FALSE)
	{
		hr = ProcessDecodedFrame(dataWriter);
	}

	return hr;
}
