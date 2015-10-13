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
#include "UncompressedAudioSampleProvider.h"

using namespace FFmpegInterop;

UncompressedAudioSampleProvider::UncompressedAudioSampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx)
	: MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
	, m_pAvFrame(nullptr)
	, m_pSwrCtx(nullptr)
{
}

HRESULT UncompressedAudioSampleProvider::AllocateResources()
{
	HRESULT hr = S_OK;
	hr = MediaSampleProvider::AllocateResources();
	if (SUCCEEDED(hr))
	{
		// Set default channel layout when the value is unknown (0)
		int64 inChannelLayout = m_pAvCodecCtx->channel_layout ? m_pAvCodecCtx->channel_layout : av_get_default_channel_layout(m_pAvCodecCtx->channels);
		int64 outChannelLayout = av_get_default_channel_layout(m_pAvCodecCtx->channels);

		// Set up resampler to convert any PCM format (e.g. AV_SAMPLE_FMT_FLTP) to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element.
		// Additional logic can be added to avoid resampling PCM data that is already in AV_SAMPLE_FMT_S16_PCM.
		m_pSwrCtx = swr_alloc_set_opts(
			NULL,
			outChannelLayout,
			AV_SAMPLE_FMT_S16,
			m_pAvCodecCtx->sample_rate,
			inChannelLayout,
			m_pAvCodecCtx->sample_fmt,
			m_pAvCodecCtx->sample_rate,
			0,
			NULL);

		if (!m_pSwrCtx)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		if (swr_init(m_pSwrCtx) < 0)
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		m_pAvFrame = av_frame_alloc();
		if (!m_pAvFrame)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	return hr;
}

UncompressedAudioSampleProvider::~UncompressedAudioSampleProvider()
{
	if (m_pAvFrame)
	{
		av_freep(m_pAvFrame);
	}

	// Free 
	swr_free(&m_pSwrCtx);
}

HRESULT UncompressedAudioSampleProvider::WriteAVPacketToStream(DataWriter^ dataWriter, AVPacket* avPacket)
{
	// Because each packet can contain multiple frames, we have already written the packet to the stream
	// during the decode stage.
	return S_OK;
}

HRESULT UncompressedAudioSampleProvider::DecodeAVPacket(DataWriter^ dataWriter, AVPacket* avPacket)
{
	HRESULT hr = S_OK;
	int frameComplete = 0;
	// Each audio packet may contain multiple frames which requires calling avcodec_decode_audio4 for each frame. Loop through the entire packet data
	while (avPacket->size > 0)
	{
		frameComplete = 0;
		int decodedBytes = avcodec_decode_audio4(m_pAvCodecCtx, m_pAvFrame, &frameComplete, avPacket);

		if (decodedBytes < 0)
		{
			DebugMessage(L"Fail To Decode!\n");
			hr = E_FAIL;
			break; // Skip broken frame
		}

		if (SUCCEEDED(hr) && frameComplete)
		{
			// Resample uncompressed frame to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element
			uint8_t *resampledData = nullptr;
			unsigned int aBufferSize = av_samples_alloc(&resampledData, NULL, m_pAvFrame->channels, m_pAvFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
			int resampledDataSize = swr_convert(m_pSwrCtx, &resampledData, aBufferSize, (const uint8_t **)m_pAvFrame->extended_data, m_pAvFrame->nb_samples);
			auto aBuffer = ref new Platform::Array<uint8_t>(resampledData, min(aBufferSize, (unsigned int)(resampledDataSize * m_pAvFrame->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16))));
			dataWriter->WriteBytes(aBuffer);
			av_freep(&resampledData);
			av_frame_unref(m_pAvFrame);
		}

		if (SUCCEEDED(hr))
		{
			// Advance to the next frame data that have not been decoded if any
			avPacket->size -= decodedBytes;
			avPacket->data += decodedBytes;
		}
	}

	if (SUCCEEDED(hr))
	{
		// We've completed the packet. Return S_FALSE to indicate an incomplete frame
		hr = (frameComplete != 0) ? S_OK : S_FALSE;
	}

	return hr;
}
