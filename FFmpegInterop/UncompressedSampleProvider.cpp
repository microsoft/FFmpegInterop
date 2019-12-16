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

using namespace winrt::FFmpegInterop::implementation;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

UncompressedSampleProvider::UncompressedSampleProvider(_In_ const AVStream* stream, _In_ FFmpegReader& reader) :
	SampleProvider(stream, reader)
{
	// Create a new decoding context
	AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
	THROW_HR_IF_NULL(MF_E_INVALIDMEDIATYPE, codec);

	m_codecContext.reset(avcodec_alloc_context3(codec));
	THROW_IF_NULL_ALLOC(m_codecContext);
	THROW_IF_FFMPEG_FAILED(avcodec_parameters_to_context(m_codecContext.get(), stream->codecpar));

	unsigned int threadCount = std::thread::hardware_concurrency();
	if (threadCount > 0)
	{
		m_codecContext->thread_count = threadCount;
		m_codecContext->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
	}

	THROW_IF_FFMPEG_FAILED(avcodec_open2(m_codecContext.get(), codec, nullptr));
}

void UncompressedSampleProvider::Flush() noexcept
{
	SampleProvider::Flush();

	avcodec_flush_buffers(m_codecContext.get());
}

AVFrame_ptr UncompressedSampleProvider::GetFrame()
{
	// Allocate a frame
	AVFrame_ptr frame{ av_frame_alloc() };
	THROW_IF_NULL_ALLOC(frame);

	while (true)
	{
		// Send a packet to the decoder and see if it can produce a frame
		AVPacket_ptr packet{ GetPacket() };

		THROW_IF_FFMPEG_FAILED(avcodec_send_packet(m_codecContext.get(), packet.get()));

		int decodeResult = avcodec_receive_frame(m_codecContext.get(), frame.get());
		if (decodeResult == AVERROR(EAGAIN))
		{
			// The decoder needs more data to produce a frame
			continue;
		}
		THROW_IF_FFMPEG_FAILED(decodeResult);

		return frame;
	}
}
