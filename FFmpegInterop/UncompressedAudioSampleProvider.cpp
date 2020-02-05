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

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	UncompressedAudioSampleProvider::UncompressedAudioSampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
		UncompressedSampleProvider(stream, reader),
		m_minAudioSampleDur(ToAVTime(MIN_AUDIO_SAMPLE_DUR_MS, MS_PER_SEC, m_stream->time_base))
	{
		if (m_codecContext->sample_fmt != AV_SAMPLE_FMT_S16)
		{
			// Set up resampler to convert to AV_SAMPLE_FMT_S16 PCM format
			int64_t inChannelLayout{ m_codecContext->channel_layout != 0 ? static_cast<int64_t>(m_codecContext->channel_layout) : av_get_default_channel_layout(m_codecContext->channels) };
			int64_t outChannelLayout{ av_get_default_channel_layout(m_codecContext->channels) };

			m_swrContext.reset(swr_alloc_set_opts(
				m_swrContext.release(),
				outChannelLayout,
				AV_SAMPLE_FMT_S16,
				m_codecContext->sample_rate,
				inChannelLayout,
				m_codecContext->sample_fmt,
				m_codecContext->sample_rate,
				0,
				nullptr));
			THROW_IF_NULL_ALLOC(m_swrContext);
			THROW_HR_IF_FFMPEG_FAILED(swr_init(m_swrContext.get()));
		}
	}

	void UncompressedAudioSampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
	{
		// We intentially don't call SampleProvider::SetEncodingProperties() here as
		// it would set encoding properties with values for the compressed audio type.
	}

	tuple<IBuffer, int64_t, int64_t, std::map<GUID, IInspectable>> UncompressedAudioSampleProvider::GetSampleData()
	{
		// Decode samples until we reach the minimum sample duration threshold or EOS
		IBuffer sampleBuf{ nullptr };
		int64_t pts{ -1 };
		int64_t dur{ 0 };
		bool firstDecodedSample{ true };
		vector<uint8_t> compactedSampleBuf;

		while (true)
		{
			// Get the next decoded sample
			AVFrame_ptr frame;
			try
			{
				frame = GetFrame();
			}
			catch (...)
			{
				const hresult hr{ to_hresult() };
				if (hr != MF_E_END_OF_STREAM)
				{
					// Unexpected error. Rethrow.
					throw;
				}
				else if (firstDecodedSample)
				{
					// No decoded samples. Rethrow.
					throw;
				}
				else
				{
					// Return the decoded sample data we have
					sampleBuf = make<FFmpegInteropBuffer>(move(compactedSampleBuf));
					break;
				}
			}

			IBuffer curSampleBuf{ nullptr };
			int curSampleCount{ 0 };

			if (m_swrContext == nullptr)
			{
				// Uncompressed frame is already in the desired output format
				curSampleBuf = make<FFmpegInteropBuffer>(frame->buf[0]);
				curSampleCount = frame->nb_samples;
			}
			else
			{
				// Resample uncompressed frame to desired output format
				uint8_t* buf{ nullptr };
				int bufSize{ av_samples_alloc(&buf, nullptr, frame->channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 0) };
				THROW_HR_IF_FFMPEG_FAILED(bufSize);

				int resampledSamplesCount{ swr_convert(m_swrContext.get(), &buf, bufSize, const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples) };
				AVBlob_ptr resampledData{ exchange(buf, nullptr) };
				THROW_HR_IF_FFMPEG_FAILED(resampledSamplesCount);

				const uint32_t resampledDataSize{ static_cast<uint32_t>(resampledSamplesCount) * frame->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) };
				WINRT_ASSERT(resampledDataSize <= static_cast<uint32_t>(bufSize));

				curSampleBuf = make<FFmpegInteropBuffer>(move(resampledData), resampledDataSize);
				curSampleCount = resampledSamplesCount;
			}

			// Update pts and dur
			if (firstDecodedSample)
			{
				pts = frame->pts;
			}

			dur += ToAVTime(curSampleCount, m_codecContext->sample_rate, m_stream->time_base);

			const bool minSampleDurMet{ dur >= m_minAudioSampleDur };

			// Copy the current sample data to the compacted sample buffer if needed
			if (!firstDecodedSample || !minSampleDurMet)
			{
				compactedSampleBuf.insert(compactedSampleBuf.end(), curSampleBuf.data(), curSampleBuf.data() + curSampleBuf.Length());
			}

			// Check if we've reached the minimum sample duration threshold
			if (minSampleDurMet)
			{
				if (firstDecodedSample)
				{
					sampleBuf = move(curSampleBuf);
				}
				else
				{
					sampleBuf = make<FFmpegInteropBuffer>(move(compactedSampleBuf));
				}

				break;
			}
			else
			{
				// Continue compacting samples
				TraceLoggingWrite(g_FFmpegInteropProvider, "CompactingUncompressedAudioSamples", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
					TraceLoggingValue(m_stream->index, "StreamId"),
					TraceLoggingValue(dur, "CompactedDur"));

				firstDecodedSample = false;
			}
		}

		return { move(sampleBuf), pts, dur, { } };
	}
}
