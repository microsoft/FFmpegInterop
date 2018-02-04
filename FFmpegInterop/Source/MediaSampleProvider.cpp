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
#include "MediaSampleProvider.h"
#include "FFmpegInteropMSS.h"
#include "FFmpegReader.h"

using namespace FFmpegInterop;

MediaSampleProvider::MediaSampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx)
	: m_pReader(reader)
	, m_pAvFormatCtx(avFormatCtx)
	, m_pAvCodecCtx(avCodecCtx)
	, m_streamIndex(AVERROR_STREAM_NOT_FOUND)
	, m_startOffset(avFormatCtx->start_time * 10)
	, m_isEnabled(true)
{
	DebugMessage(L"MediaSampleProvider\n");
}

HRESULT MediaSampleProvider::AllocateResources()
{
	DebugMessage(L"AllocateResources\n");
	return S_OK;
}

MediaSampleProvider::~MediaSampleProvider()
{
	DebugMessage(L"~MediaSampleProvider\n");
}

void MediaSampleProvider::SetCurrentStreamIndex(int streamIndex)
{
	DebugMessage(L"SetCurrentStreamIndex\n");
	if (m_pAvCodecCtx != nullptr && m_pAvFormatCtx->nb_streams > (unsigned int)streamIndex)
	{
		m_streamIndex = streamIndex;
	}
	else
	{
		m_streamIndex = AVERROR_STREAM_NOT_FOUND;
	}
}

MediaStreamSample^ MediaSampleProvider::GetNextSample()
{
	DebugMessage(L"GetNextSample\n");

	HRESULT hr = S_OK;

	MediaStreamSample^ sample;
	if (m_isEnabled)
	{
		IBuffer^ buffer = nullptr;
		LONGLONG pts = 0;
		LONGLONG dur = 0;

		hr = CreateNextSampleBuffer(&buffer, pts, dur);

		if (hr == S_OK)
		{
			pts = LONGLONG(av_q2d(m_pAvFormatCtx->streams[m_streamIndex]->time_base) * 10000000 * pts) - m_startOffset;
			dur = LONGLONG(av_q2d(m_pAvFormatCtx->streams[m_streamIndex]->time_base) * 10000000 * dur);

			sample = MediaStreamSample::CreateFromBuffer(buffer, { pts });
			sample->Duration = { dur };
			sample->Discontinuous = m_isDiscontinuous;

			hr = SetSampleProperties(sample);

			m_isDiscontinuous = false;
		}
		else if (hr == S_FALSE)
		{
			DebugMessage(L"End of stream reached.\n");
			DisableStream();
		}
		else
		{
			DebugMessage(L"Error reading next packet.\n");
			DisableStream();
		}
	}

	return sample;
}

HRESULT MediaSampleProvider::GetNextPacket(AVPacket** avPacket, LONGLONG & packetPts, LONGLONG & packetDuration)
{
	HRESULT hr = S_OK;

	// Continue reading until there is an appropriate packet in the stream
	while (m_packetQueue.empty())
	{
		if (m_pReader->ReadPacket() < 0)
		{
			DebugMessage(L"GetNextSample reaching EOF\n");
			break;
		}
	}

	if (!m_packetQueue.empty())
	{
		// read next packet and set pts values
		auto packet = PopPacket();
		*avPacket = packet;

		packetDuration = packet->duration;
		if (packet->pts != AV_NOPTS_VALUE)
		{
			packetPts = packet->pts;
			// Set the PTS for the next sample if it doesn't one.
			m_nextPacketPts = packetPts + packetDuration;
		}
		else
		{
			packetPts = m_nextPacketPts;
			// Set the PTS for the next sample if it doesn't one.
			m_nextPacketPts += packetDuration;
		}
	}
	else
	{
		hr = S_FALSE;
	}

	return hr;
}


void MediaSampleProvider::QueuePacket(AVPacket *packet)
{
	DebugMessage(L" - QueuePacket\n");

	if (m_isEnabled)
	{
		m_packetQueue.push(packet);
	}
	else
	{
		av_packet_free(&packet);
	}
}

AVPacket* MediaSampleProvider::PopPacket()
{
	DebugMessage(L" - PopPacket\n");
	AVPacket* result = NULL;

	if (!m_packetQueue.empty())
	{
		result = m_packetQueue.front();
		m_packetQueue.pop();
	}

	return result;
}


void MediaSampleProvider::Flush()
{
	DebugMessage(L"Flush\n");
	while (!m_packetQueue.empty())
	{
		AVPacket *avPacket = PopPacket();
		av_packet_free(&avPacket);
	}
	m_isDiscontinuous = true;
}

void MediaSampleProvider::DisableStream()
{
	DebugMessage(L"DisableStream\n");
	Flush();
	m_isEnabled = false;
}

void free_buffer(void *lpVoid)
{
	auto buffer = (AVBufferRef *)lpVoid;
	av_buffer_unref(&buffer);
}