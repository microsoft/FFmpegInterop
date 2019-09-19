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
#include "FFmpegReader.h"
#include "MediaSampleProvider.h"

extern "C"
{
#include <libavformat/avformat.h>
}

using namespace FFmpegInterop;

FFmpegReader::FFmpegReader(AVFormatContext* avFormatCtx) :
	m_pAvFormatCtx(avFormatCtx),
	m_audioSampleProvider(nullptr),
	m_audioStreamIndex(AVERROR_STREAM_NOT_FOUND),
	m_videoSampleProvider(nullptr),
	m_videoStreamIndex(AVERROR_STREAM_NOT_FOUND),
	m_subtitleSampleProvider(nullptr),
	m_subtitleStreamIndex(AVERROR_STREAM_NOT_FOUND)
{
}

// Read the next packet from the stream and push it into the appropriate sample provider
int FFmpegReader::ReadPacket()
{
	AVPacket_ptr packet(av_packet_alloc());
	if (packet == nullptr)
	{
		return AVERROR(ENOMEM);
	}

	int ret = av_read_frame(m_pAvFormatCtx, packet.get());
	if (ret < 0)
	{
		return ret;
	}

	// Push the packet to the appropriate stream or drop the packet if the stream is not being used
	if (packet->stream_index == m_audioStreamIndex && m_audioSampleProvider != nullptr)
	{
		m_audioSampleProvider->QueuePacket(std::move(packet));
	}
	else if (packet->stream_index == m_videoStreamIndex && m_videoSampleProvider != nullptr)
	{
		m_videoSampleProvider->QueuePacket(std::move(packet));
	}
	else if (packet->stream_index == m_subtitleStreamIndex && m_subtitleSampleProvider != nullptr)
	{
		m_subtitleSampleProvider->QueuePacket(std::move(packet));
	}

	return ret;
}

void FFmpegReader::SetAudioStream(int audioStreamIndex, MediaSampleProvider* audioSampleProvider)
{
	m_audioStreamIndex = audioStreamIndex;
	m_audioSampleProvider = audioSampleProvider;
	if (audioSampleProvider != nullptr)
	{
		audioSampleProvider->SetCurrentStreamIndex(m_audioStreamIndex);
	}
}

void FFmpegReader::SetVideoStream(int videoStreamIndex, MediaSampleProvider* videoSampleProvider)
{
	m_videoStreamIndex = videoStreamIndex;
	m_videoSampleProvider = videoSampleProvider;
	if (videoSampleProvider != nullptr)
	{
		videoSampleProvider->SetCurrentStreamIndex(m_videoStreamIndex);
	}
}

void FFmpegReader::SetSubtitleStream(int subtitleStreamIndex, MediaSampleProvider* subtitleSampleProvider)
{
	m_subtitleStreamIndex = subtitleStreamIndex;
	m_subtitleSampleProvider = subtitleSampleProvider;
	if (subtitleSampleProvider != nullptr)
	{
		subtitleSampleProvider->SetCurrentStreamIndex(m_subtitleStreamIndex);
	}
}
