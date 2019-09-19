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

extern "C"
{
#include <libavformat/avformat.h>
}

using namespace FFmpegInterop;
using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Storage::Streams;

void AVPacketDeleter::operator()(AVPacket* packet)
{
	av_packet_free(&packet);
}

MediaSampleProvider::MediaSampleProvider(FFmpegReader& reader, const AVFormatContext* avFormatCtx, const AVCodecContext* avCodecCtx) :
	m_reader(reader),
	m_pAvFormatCtx(avFormatCtx),
	m_pAvCodecCtx(avCodecCtx),
	m_streamIndex(AVERROR_STREAM_NOT_FOUND),
	m_isEnabled(false),
	m_isDiscontinuous(true),
	m_nextFramePts(AV_NOPTS_VALUE)
{
	DebugMessage(L"MediaSampleProvider\n");
	
	m_startOffset = (m_pAvFormatCtx->start_time != AV_NOPTS_VALUE) ? static_cast<int64_t>(m_pAvFormatCtx->start_time * 10000000 / double(AV_TIME_BASE)) : 0;
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

	if (m_pAvFormatCtx->nb_streams > (unsigned int)streamIndex)
	{
		m_streamIndex = streamIndex;
	}
	else
	{
		m_streamIndex = AVERROR_STREAM_NOT_FOUND;
	}
}

MediaStreamSample MediaSampleProvider::GetNextSample()
{
	DebugMessage(L"GetNextSample\n");

	MediaStreamSample sample{ nullptr };

	if (m_isEnabled)
	{
		DataWriter dataWriter;
		LONGLONG pts = 0;
		LONGLONG dur = 0;

		HRESULT hr = GetNextPacket(dataWriter, pts, dur, true);

		if (hr == S_OK)
		{
			sample = MediaStreamSample::CreateFromBuffer(dataWriter.DetachBuffer(), TimeSpan{ pts });
			sample.Duration(TimeSpan{ dur });
			sample.Discontinuous(m_isDiscontinuous);
			m_isDiscontinuous = false;
		}
		else if (hr != MF_E_END_OF_STREAM)
		{
			DebugMessage(L"Too many broken packets - disable stream\n");
			DisableStream();
		}
	}

	return sample;
}

HRESULT MediaSampleProvider::WriteAVPacketToStream(const DataWriter& dataWriter, const AVPacket_ptr& packet)
{
	// This is the simplest form of transfer. Copy the packet directly to the stream
	// This works for most compressed formats
	dataWriter.WriteBytes(array_view<const byte>(packet->data, packet->data + packet->size));

	return S_OK;
}

HRESULT MediaSampleProvider::DecodeAVPacket(const DataWriter& dataWriter, const AVPacket_ptr& packet, int64_t& framePts, int64_t& frameDuration)
{
	// For the simple case of compressed samples, each packet is a sample
	frameDuration = packet->duration;
	if (packet->pts != AV_NOPTS_VALUE)
	{
		framePts = packet->pts;
		// Set the PTS for the next sample if it doesn't one.
		m_nextFramePts = framePts + frameDuration;
	}
	else
	{
		if (m_nextFramePts == AV_NOPTS_VALUE)
		{
			m_nextFramePts = (m_pAvFormatCtx->streams[m_streamIndex]->start_time != AV_NOPTS_VALUE) ? m_pAvFormatCtx->streams[m_streamIndex]->start_time : 0;
		}

		framePts = m_nextFramePts;
		// Set the PTS for the next sample if it doesn't one.
		m_nextFramePts += frameDuration;
	}

	return S_OK;
}

void MediaSampleProvider::QueuePacket(AVPacket_ptr packet)
{
	DebugMessage(L" - QueuePacket\n");

	if (m_isEnabled)
	{
		m_packetQueue.push_back(std::move(packet));
	}
}

HRESULT FFmpegInterop::MediaSampleProvider::GetNextPacket(const DataWriter& dataWriter, LONGLONG& pts, LONGLONG& dur, bool allowSkip)
{
	HRESULT hr = S_OK;

	AVPacket_ptr packet;
	bool frameComplete = false;
	bool decodeSuccess = true;
	int64_t framePts = 0;
	int64_t frameDuration = 0;
	int errorCount = 0;

	while (SUCCEEDED(hr) && !frameComplete)
	{
		// Continue reading until there is an appropriate packet in the stream
		while (m_packetQueue.empty())
		{
			int readResult = m_reader.ReadPacket();
			if (readResult == AVERROR_EOF)
			{
				DebugMessage(L"GetNextSample reaching EOF\n");
				hr = MF_E_END_OF_STREAM;
				break;
			}
			else if (readResult < 0)
			{
				hr = E_FAIL;
				break;
			}
		}

		if (!m_packetQueue.empty())
		{
			// Pick the packets from the queue one at a time
			packet = std::move(m_packetQueue.front());
			m_packetQueue.pop_front();

			// Decode the packet if necessary, it will update the presentation time if necessary
			hr = DecodeAVPacket(dataWriter, packet, framePts, frameDuration);
			frameComplete = (hr == S_OK);

			if (!frameComplete)
			{
				m_isDiscontinuous = true;

				if (allowSkip && errorCount++ < 10)
				{
					// skip a few broken packets (maybe make this configurable later)
					DebugMessage(L"Skipping broken packet\n");
					hr = S_OK;
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		// Write the packet out
		hr = WriteAVPacketToStream(dataWriter, packet);

		pts = LONGLONG(av_q2d(m_pAvFormatCtx->streams[m_streamIndex]->time_base) * 10000000 * framePts) - m_startOffset;
		dur = LONGLONG(av_q2d(m_pAvFormatCtx->streams[m_streamIndex]->time_base) * 10000000 * frameDuration);
	}

	return hr;
}

void MediaSampleProvider::Flush()
{
	DebugMessage(L"Flush\n");

	m_packetQueue.clear();
	m_isDiscontinuous = true;
}

void MediaSampleProvider::DisableStream()
{
	DebugMessage(L"DisableStream\n");
	Flush();
	m_isEnabled = false;
}

void MediaSampleProvider::EnableStream()
{
	DebugMessage(L"EnableStream\n");
	m_isEnabled = true;
}

