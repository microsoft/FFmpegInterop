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

extern "C"
{
#include <libavformat/avformat.h>
}

using namespace FFmpegInterop;
using namespace winrt::Windows::Storage::Streams;

void AVFrameDeleter::operator()(AVFrame* frame)
{
	av_frame_free(&frame);
}

UncompressedSampleProvider::UncompressedSampleProvider(FFmpegReader& reader, const AVFormatContext* avFormatCtx, AVCodecContext* avCodecCtx) :
	MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
{
}

HRESULT UncompressedSampleProvider::DecodeAVPacket(const winrt::Windows::Storage::Streams::DataWriter& dataWriter, const AVPacket_ptr& packet, int64_t& framePts, int64_t& frameDuration)
{
	HRESULT hr = S_OK;

	int sendPacketResult = avcodec_send_packet(m_pAvCodecCtx, packet.get());
	if (sendPacketResult == AVERROR(EAGAIN))
	{
		// The decoder should have been drained and always ready to access input
		_ASSERT(FALSE);
		hr = E_UNEXPECTED;
	}
	else if (sendPacketResult < 0)
	{
		// We failed to send the packet
		DebugMessage(L"Decoder failed on the sample\n");
		hr = E_FAIL;
	}

	bool fGotFrame  = false;
	while (SUCCEEDED(hr))
	{
		// Try to get a frame from the decoder.
		AVFrame_ptr frame(av_frame_alloc());
		int decodeFrame = avcodec_receive_frame(m_pAvCodecCtx, frame.get());

		// The decoder is empty, send a packet to it.
		if (decodeFrame == AVERROR(EAGAIN))
		{
			// The decoder doesn't have enough data to produce a frame.
			// If the decoder didn't give an initial frame we still need to feed it more frames.
			// Return S_FALSE to indicate a partial frame
			hr = fGotFrame ? S_OK : S_FALSE;
			break;
		}
		else if (decodeFrame < 0)
		{
			hr = E_FAIL;
			DebugMessage(L"Failed to get a frame from the decoder\n");
		}

		// Update the timestamp if the packet has one
		if (frame->pts != AV_NOPTS_VALUE)
		{
			framePts = frame->pts;
			frameDuration = frame->pkt_duration;
		}
		fGotFrame = true;

		hr = ProcessDecodedFrame(dataWriter);
	}

	return hr;
}
