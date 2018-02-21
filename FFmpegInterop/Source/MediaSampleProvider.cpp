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
	, m_startOffset(AV_NOPTS_VALUE)
	, m_nextFramePts(0)
	, m_isEnabled(true)
	, m_isDiscontinuous(false)
{
	DebugMessage(L"MediaSampleProvider\n");
}

HRESULT MediaSampleProvider::AllocateResources()
{
	DebugMessage(L"AllocateResources\n");
	m_startOffset = AV_NOPTS_VALUE;
	m_nextFramePts = 0;
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

MediaStreamSample^ MediaSampleProvider::GetNextSample()
{
	DebugMessage(L"GetNextSample\n");

	HRESULT hr = S_OK;

	MediaStreamSample^ sample;
	if (m_isEnabled)
	{
		DataWriter^ dataWriter = ref new DataWriter();

		LONGLONG pts = 0;
		LONGLONG dur = 0;

		hr = GetNextPacket(dataWriter, pts, dur, true);

		if (hr == S_OK)
		{
			sample = MediaStreamSample::CreateFromBuffer(dataWriter->DetachBuffer(), { pts });
			sample->Duration = { dur };
			sample->Discontinuous = m_isDiscontinuous;
			m_isDiscontinuous = false;
		}
		else
		{
			DebugMessage(L"Too many broken packets - disable stream\n");
			DisableStream();
		}
	}

	return sample;
}

HRESULT MediaSampleProvider::WriteAVPacketToStream(DataWriter^ dataWriter, AVPacket* avPacket)
{
	// This is the simplest form of transfer. Copy the packet directly to the stream
	// This works for most compressed formats
	auto aBuffer = ref new Platform::Array<uint8_t>(avPacket->data, avPacket->size);
	dataWriter->WriteBytes(aBuffer);
	return S_OK;
}

HRESULT MediaSampleProvider::DecodeAVPacket(DataWriter^ dataWriter, AVPacket *avPacket, int64_t &framePts, int64_t &frameDuration)
{
	// For the simple case of compressed samples, each packet is a sample
	if (avPacket != nullptr)
	{
		frameDuration = avPacket->duration;
		if (avPacket->pts != AV_NOPTS_VALUE)
		{
			framePts = avPacket->pts;
			// Set the PTS for the next sample if it doesn't one.
			m_nextFramePts = framePts + frameDuration;
		}
		else
		{
			framePts = m_nextFramePts;
			// Set the PTS for the next sample if it doesn't one.
			m_nextFramePts += frameDuration;
		}
	}
	return S_OK;
}

void MediaSampleProvider::QueuePacket(AVPacket packet)
{
	DebugMessage(L" - QueuePacket\n");

	if (m_isEnabled)
	{
		m_packetQueue.push_back(packet);
	}
	else
	{
		av_packet_unref(&packet);
	}
}

AVPacket MediaSampleProvider::PopPacket()
{
	DebugMessage(L" - PopPacket\n");

	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	if (!m_packetQueue.empty())
	{
		avPacket = m_packetQueue.front();
		m_packetQueue.erase(m_packetQueue.begin());
	}

	return avPacket;
}

HRESULT FFmpegInterop::MediaSampleProvider::GetNextPacket(DataWriter ^ writer, LONGLONG & pts, LONGLONG & dur, bool allowSkip)
{
	HRESULT hr = S_OK;

	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	bool frameComplete = false;
	bool decodeSuccess = true;
	int64_t framePts = 0, frameDuration = 0;
	int errorCount = 0;

	while (SUCCEEDED(hr) && !frameComplete)
	{
		// Continue reading until there is an appropriate packet in the stream
		while (m_packetQueue.empty())
		{
			if (m_pReader->ReadPacket() < 0)
			{
				DebugMessage(L"GetNextSample reaching EOF\n");
				hr = E_FAIL;
				break;
			}
		}

		if (!m_packetQueue.empty())
		{
			// Pick the packets from the queue one at a time
			avPacket = PopPacket();
			framePts = avPacket.pts;
			frameDuration = avPacket.duration;

			// Decode the packet if necessary, it will update the presentation time if necessary
			hr = DecodeAVPacket(writer, &avPacket, framePts, frameDuration);
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
		hr = WriteAVPacketToStream(writer, &avPacket);

		if (m_startOffset == AV_NOPTS_VALUE)
		{
			//if we havent set m_startOffset already
			DebugMessage(L"Saving m_startOffset\n");

			//in some real-time streams framePts is less than 0 so we need to make sure m_startOffset is never negative
			m_startOffset = framePts < 0 ? 0 : framePts;
		}

		pts = LONGLONG(av_q2d(m_pAvFormatCtx->streams[m_streamIndex]->time_base) * 10000000 * (framePts - m_startOffset));

		dur = LONGLONG(av_q2d(m_pAvFormatCtx->streams[m_streamIndex]->time_base) * 10000000 * frameDuration);
	}

	av_packet_unref(&avPacket);

	return hr;
}

void MediaSampleProvider::Flush()
{
	DebugMessage(L"Flush\n");
	while (!m_packetQueue.empty())
	{
		av_packet_unref(&PopPacket());
	}
	m_isDiscontinuous = true;
}

void MediaSampleProvider::DisableStream()
{
	DebugMessage(L"DisableStream\n");
	Flush();
	m_isEnabled = false;
}