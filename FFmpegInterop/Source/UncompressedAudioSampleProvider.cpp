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
	: UncompressedSampleProvider(reader, avFormatCtx, avCodecCtx)
	, m_pSwrCtx(nullptr)
{
}

HRESULT UncompressedAudioSampleProvider::AllocateResources()
{
	HRESULT hr = S_OK;
	hr = UncompressedSampleProvider::AllocateResources();
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

HRESULT UncompressedAudioSampleProvider::ProcessDecodedFrame(DataWriter^ dataWriter)
{
	// Resample uncompressed frame to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element
	uint8_t *resampledData = nullptr;
	unsigned int aBufferSize = av_samples_alloc(&resampledData, NULL, m_pAvFrame->channels, m_pAvFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
	int resampledDataSize = swr_convert(m_pSwrCtx, &resampledData, aBufferSize, (const uint8_t **)m_pAvFrame->extended_data, m_pAvFrame->nb_samples);
	auto aBuffer = ref new Platform::Array<uint8_t>(resampledData, min(aBufferSize, (unsigned int)(resampledDataSize * m_pAvFrame->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16))));
	dataWriter->WriteBytes(aBuffer);
	av_freep(&resampledData);
	av_frame_unref(m_pAvFrame);
	av_freep(m_pAvFrame);

	return S_OK;
}