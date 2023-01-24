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

// Minimum duration for uncompressed audio samples (200 ms)
const LONGLONG MINAUDIOSAMPLEDURATION = 2000000;

UncompressedAudioSampleProvider::UncompressedAudioSampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx)
	: UncompressedSampleProvider(reader, avFormatCtx, avCodecCtx)
	, m_inputSampleFormat(avCodecCtx->sample_fmt)
	, m_inputChannelLayout(m_pAvCodecCtx->channel_layout ? m_pAvCodecCtx->channel_layout : av_get_default_channel_layout(m_pAvCodecCtx->channels))
	, m_inputSampleRate(avCodecCtx->sample_rate)
	, m_outputChannelLayout(m_pAvCodecCtx->channel_layout ? m_pAvCodecCtx->channel_layout : av_get_default_channel_layout(m_pAvCodecCtx->channels))
	, m_outputSampleRate(avCodecCtx->sample_rate)
	, m_pSwrCtx(nullptr)
{
}

HRESULT UncompressedAudioSampleProvider::AllocateResources()
{
	HRESULT hr = S_OK;
	hr = UncompressedSampleProvider::AllocateResources();
	if (FAILED(hr))
	{
		return hr;
	}
	
	if (m_inputSampleFormat != AV_SAMPLE_FMT_S16)
	{
		hr = InitResampler();
		if (FAILED(hr))
		{
			return hr;
		}
	}

	return hr;
}

UncompressedAudioSampleProvider::~UncompressedAudioSampleProvider()
{
	if (m_pAvFrame)
	{
		av_frame_free(&m_pAvFrame);
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
	HRESULT hr = S_OK;

	// Check if the format changed
	uint64_t frameChannelLayout = m_pAvFrame->channel_layout ? m_pAvFrame->channel_layout : av_get_default_channel_layout(m_pAvFrame->channels);
	if (m_pAvFrame->format != m_inputSampleFormat ||
		frameChannelLayout != m_inputChannelLayout ||
		m_pAvFrame->sample_rate != m_inputSampleRate)
	{
		m_inputSampleFormat = static_cast<AVSampleFormat>(m_pAvFrame->format);
		m_inputChannelLayout = frameChannelLayout;
		m_inputSampleRate = m_pAvFrame->sample_rate;

		// Check if resampler is needed
		if (m_inputSampleFormat != AV_SAMPLE_FMT_S16 ||
			m_inputChannelLayout != m_outputChannelLayout ||
			m_inputSampleRate != m_outputSampleRate)
		{
			hr = InitResampler();
			if (FAILED(hr))
			{
				return hr;
			}
		}
		else
		{
			swr_free(&m_pSwrCtx);
		}
	}

	Platform::Array<uint8_t>^ aBuffer = nullptr;
	if (m_pSwrCtx == nullptr)
	{
		aBuffer = ref new Platform::Array<uint8_t>(m_pAvFrame->buf[0]->data, m_pAvFrame->buf[0]->size);
	}
	else
	{
		// Resample uncompressed frame to AV_SAMPLE_FMT_S16 PCM format
		uint8_t *resampledData = nullptr;
		int aBufferSize = av_samples_alloc(&resampledData, NULL, av_get_channel_layout_nb_channels(m_outputChannelLayout), m_pAvFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
		if (aBufferSize < 0)
		{
			return E_FAIL;
		}

		int resampledDataSize = swr_convert(m_pSwrCtx, &resampledData, m_pAvFrame->nb_samples, (const uint8_t **)m_pAvFrame->extended_data, m_pAvFrame->nb_samples);
		if (resampledDataSize < 0)
		{
			av_freep(&resampledData);
			return E_FAIL;
		}

		aBuffer = ref new Platform::Array<uint8_t>(resampledData, min(aBufferSize, resampledDataSize * m_pAvFrame->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)));
		av_freep(&resampledData);
	}

	dataWriter->WriteBytes(aBuffer);
	av_frame_free(&m_pAvFrame);

	return S_OK;
}

HRESULT UncompressedAudioSampleProvider::InitResampler()
{
	// Set up resampler to convert any PCM format (e.g. AV_SAMPLE_FMT_FLTP) to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element.
	m_pSwrCtx = swr_alloc_set_opts(
		m_pSwrCtx,
		m_outputChannelLayout,
		AV_SAMPLE_FMT_S16,
		m_outputSampleRate,
		m_inputChannelLayout,
		m_inputSampleFormat,
		m_inputSampleRate,
		0,
		NULL);

	if (!m_pSwrCtx)
	{
		return E_OUTOFMEMORY;
	}

	if (swr_init(m_pSwrCtx) < 0)
	{
		return E_FAIL;
	}

	return S_OK;
}

MediaStreamSample^ UncompressedAudioSampleProvider::GetNextSample()
{
	// Similar to GetNextSample in MediaSampleProvider, 
	// but we concatenate samples until reaching a minimum duration
	DebugMessage(L"GetNextSample\n");

	HRESULT hr = S_OK;

	MediaStreamSample^ sample;
	DataWriter^ dataWriter = ref new DataWriter();

	LONGLONG finalPts = -1;
	LONGLONG finalDur = 0;
	bool isFirstPacket = true;
	bool isDiscontinuous;

	do
	{
		LONGLONG pts = 0;
		LONGLONG dur = 0;

		hr = GetNextPacket(dataWriter, pts, dur, isFirstPacket);
		if (isFirstPacket)
		{
			isDiscontinuous = m_isDiscontinuous;
		}
		isFirstPacket = false;

		if (SUCCEEDED(hr))
		{
			if (finalPts == -1)
			{
				finalPts = pts;
			}
			finalDur += dur;
		}

	} while (SUCCEEDED(hr) && finalDur < MINAUDIOSAMPLEDURATION);

	if (finalPts != -1)
	{
		sample = MediaStreamSample::CreateFromBuffer(dataWriter->DetachBuffer(), { finalPts });

		// Recalculate duration after appending samples
		// FFMPEG does not seem to always output correct duration for uncompressed
		const LONGLONG numerator = sample->Buffer->Length * 10000000LL;
		const LONGLONG denominator = m_pAvFormatCtx->streams[m_streamIndex]->codecpar->channels * m_pAvFormatCtx->streams[m_streamIndex]->codecpar->sample_rate * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
		if (denominator != 0)
		{
			sample->Duration = { numerator / denominator };
		}

		sample->Discontinuous = isDiscontinuous;
		if (SUCCEEDED(hr))
		{
			// only reset flag if last packet was read successfully
			m_isDiscontinuous = false;
		}
	}
	else
	{
		// flush stream and disable any further processing
		DebugMessage(L"Too many broken packets - disable stream\n");
		DisableStream();
	}

	return sample;
}
