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
{
}

HRESULT MediaSampleProvider::AllocateResources()
{
	return S_OK;
}

MediaSampleProvider::~MediaSampleProvider()
{
}

void MediaSampleProvider::SetCurrentStreamIndex(int streamIndex)
{
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
	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;
	DataWriter^ dataWriter = ref new DataWriter();

	bool frameComplete = false;
	bool decodeSuccess = true;
	int64_t framePts = 0, frameDuration = 0;

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
			hr = DecodeAVPacket(dataWriter, &avPacket, framePts, frameDuration);
			frameComplete = (hr == S_OK);
		}
	}

	if (SUCCEEDED(hr))
	{
		// Write the packet out
		hr = WriteAVPacketToStream(dataWriter, &avPacket);

		Windows::Foundation::TimeSpan pts = { LONGLONG(av_q2d(m_pAvFormatCtx->streams[m_streamIndex]->time_base) * 10000000 * framePts) };
		Windows::Foundation::TimeSpan dur = { LONGLONG(av_q2d(m_pAvFormatCtx->streams[m_streamIndex]->time_base) * 10000000 * frameDuration) };

		sample = MediaStreamSample::CreateFromBuffer(dataWriter->DetachBuffer(), pts);
		sample->Duration = dur;
	}

	av_packet_unref(&avPacket);

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
	if (avPacket != nullptr && avPacket->pts != AV_NOPTS_VALUE)
	{
		framePts = avPacket->pts;
		frameDuration = avPacket->duration;
	}
	return S_OK;
}

void MediaSampleProvider::QueuePacket(AVPacket packet)
{
	DebugMessage(L" - QueuePacket\n");

	m_packetQueue.push_back(packet);
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

void MediaSampleProvider::Flush()
{
	while (!m_packetQueue.empty())
	{
		av_packet_unref(&PopPacket());
	}
}
