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

using namespace winrt::FFmpegInterop::implementation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

UncompressedAudioSampleProvider::UncompressedAudioSampleProvider(_In_ const AVStream* stream, _In_ FFmpegReader& reader) :
	UncompressedSampleProvider(stream, reader)
{
		
}

void UncompressedAudioSampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp)
{
	// We intentially don't call SampleProvider::SetEncodingProperties() here as
	// it would set encoding properties with values for the compressed audio type.
}

tuple<IBuffer, int64_t, int64_t, std::map<GUID, IInspectable>> UncompressedAudioSampleProvider::GetSampleData()
{
	// TODO: Sample compaction
	// Minimum duration for uncompressed audio samples (200 ms)
	// constexpr int64_t c_minAudioSampleDuration = 200 * (AV_TIME_BASE / MS_PER_SEC);

	// Get the next decoded sample
	AVFrame_ptr frame{ GetFrame() };

	InitResamplerIfNeeded(frame.get());

	IBuffer sampleData{ nullptr };
	int sampleCount{ 0 };

	if (m_swrContext == nullptr)
	{
		sampleData = make<FFmpegInteropBuffer>(frame->buf[0]);
		sampleCount = frame->nb_samples;
	}
	else
	{
		// Resample the frame to the desired output format
		int result{ swr_convert_frame(m_swrContext.get(), m_resampledFrame.get(), frame.get()) };
		if (result == AVERROR_INPUT_CHANGED)
		{
			// There was a format change. Reconfigure the resampler and try again.
			THROW_HR_IF_FFMPEG_FAILED(swr_config_frame(m_swrContext.get(), m_resampledFrame.get(), frame.get()));
			THROW_HR_IF_FFMPEG_FAILED(swr_init(m_swrContext.get()));
			THROW_HR_IF_FFMPEG_FAILED(swr_convert_frame(m_swrContext.get(), m_resampledFrame.get(), frame.get()));
		}
		else
		{
			THROW_HR_IF_FFMPEG_FAILED(result);
		}

		sampleData = make<FFmpegInteropBuffer>(m_resampledFrame->buf[0]);
		sampleCount = m_resampledFrame->nb_samples;

		av_frame_unref(m_resampledFrame.get());
	}

	// Calculate duration for the decoded sample
	int64_t dur = static_cast<int64_t>(sampleCount) * m_stream->time_base.den / (static_cast<int64_t>(m_codecContext->sample_rate) * m_stream->time_base.num);

	return { move(sampleData), frame->best_effort_timestamp, dur, { } };
}

void UncompressedAudioSampleProvider::InitResamplerIfNeeded(_In_ const AVFrame* frame)
{
	const bool resamplerNeeded{
		frame->format != AV_SAMPLE_FMT_S16 ||
		frame->sample_rate != m_codecContext->sample_rate ||
		frame->channels != m_codecContext->channels };

	if (resamplerNeeded)
	{
		if (m_swrContext == nullptr)
		{
			// Allocate a resampler and frame for the resampled output
			m_swrContext.reset(swr_alloc());

			m_resampledFrame.reset(av_frame_alloc());
			THROW_IF_NULL_ALLOC(m_resampledFrame);
			m_resampledFrame->format = AV_SAMPLE_FMT_S16;
			m_resampledFrame->sample_rate = m_codecContext->sample_rate;
			m_resampledFrame->channel_layout = (m_codecContext->channel_layout != 0) ? m_codecContext->channel_layout : av_get_default_channel_layout(m_codecContext->channels);
		}
	}
	else if (m_swrContext != nullptr)
	{
		m_swrContext.reset();
		m_resampledFrame.reset();
	}
}
	
/*
UncompressedAudioSampleProvider::UncompressedAudioSampleProvider(_In_ const AVStream* stream, _In_ FFmpegReader& reader) :
	UncompressedSampleProvider(stream, reader)
{
	// Set default channel layout when the value is unknown (0)
	int64_t inChannelLayout = m_codecContext->channel_layout ? m_codecContext->channel_layout : av_get_default_channel_layout(m_codecContext->channels);
	int64_t outChannelLayout = av_get_default_channel_layout(m_codecContext->channels);

	// Set up resampler to convert any PCM format (e.g. AV_SAMPLE_FMT_FLTP) to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element.
	// Additional logic can be added to avoid resampling PCM data that is already in AV_SAMPLE_FMT_S16_PCM.
	m_swrContext.reset(swr_alloc_set_opts(
		nullptr,
		outChannelLayout,
		AV_SAMPLE_FMT_S16,
		m_codecContext->sample_rate,
		inChannelLayout,
		m_codecContext->sample_fmt,
		m_codecContext->sample_rate,
		0,
		nullptr));
	THROW_IF_NULL_ALLOC(E_OUTOFMEMORY, m_swrContext);
	THROW_HR_IF_FFMPEG_FAILED(swr_init(m_swrContext.get()));
}

	
tuple<IBuffer, int64_t, int64_t> UncompressedAudioSampleProvider::GetSampleData()
{
	// TODO: Sample compaction
	// Minimum duration for uncompressed audio samples (200 ms)
	// constexpr int64_t c_minAudioSampleDuration = 200 * (AV_TIME_BASE / MS_PER_SEC);

	GetFrame();

	IBuffer sampleData{ nullptr };
	int sampleCount;

	if (m_swrContext == nullptr)
	{
		sampleData = make<FFmpegInteropBuffer>(frame->buf[0]);
		sampleCount = frame->nb_samples;
	}
	else
	{
		// Resample uncompressed frame to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element
		uint8_t* buf = nullptr;
		int bufSize = av_samples_alloc(&buf, nullptr, frame->channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 0);
		THROW_HR_IF_FFMPEG_FAILED(bufSize);

		int resampledSamplesCount = swr_convert(m_swrContext.get(), &buf, bufSize, frame->extended_data, frame->nb_samples);
		AVBlob_ptr resampledData{ exchange(buf, nullptr) };
		THROW_HR_IF_FFMPEG_FAILED(resampledSamplesCount);

		const uint32_t resampledDataSize = resampledSamplesCount * frame->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
		WINRT_ASSERT(resampledDataSize <= bufSize);

		sampleData = make<FFmpegInteropBuffer>(move(resampledData), resampledDataSize);
		sampleCount = resampledSamplesCount;
	}

	int64_t pts = frame->pkt_pts;
	int64_t dur = static_cast<int64_t>(sampleCount) * m_stream->time_base.den / (m_codecContext->sample_rate * m_stream->time_base.num);

	av_frame_unref(frame.get()); // We no longer need the current frame

	return { move(sampleData), pts, dur };
}

void UncompressedAudioSampleProvider::UpdateResamplerIfNeeded()
{
	if (m_swrContext == nullptr)
	{
		// Check if we need a resampler
		bool needResampler = 
			frame->channels != m_codecContext->channels ||
			frame->sample_rate != m_codecContext->sample_rate ||
			frame->format != AV_SAMPLE_FMT_S16;

		m_swrContext.reset(swr_alloc_set_opts(
			m_swrContext.release(),
			av_get_default_channel_layout(m_codecContext->channels),
			AV_SAMPLE_FMT_S16,
			m_codecContext->sample_rate,
			av_get_default_channel_layout(frame->channels),
			static_cast<AVSampleFormat>(frame->format),
			frame->sample_rate,
			0,
			nullptr));

	}
	else
	{

	}
}
*/
