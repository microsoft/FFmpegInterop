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
	UncompressedAudioSampleProvider::UncompressedAudioSampleProvider(_In_ const AVFormatContext* formatContext, _In_ AVStream* stream, _In_ Reader& reader, _In_ uint32_t allowedDecodeErrors) :
		UncompressedSampleProvider(formatContext, stream, reader, allowedDecodeErrors),
		m_minAudioSampleDur(ConvertToAVTime(MIN_AUDIO_SAMPLE_DUR_MS, MS_PER_SEC, m_stream->time_base))
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
		// We intentionally don't call SampleProvider::SetEncodingProperties() here as
		// it would set encoding properties with values for the compressed audio type.
	}

	void UncompressedAudioSampleProvider::Flush() noexcept
	{
		UncompressedSampleProvider::Flush();

		m_lastDecodeFailed = false;
	}

	tuple<IBuffer, int64_t, int64_t, vector<pair<GUID, IInspectable>>, vector<pair<GUID, IInspectable>>> UncompressedAudioSampleProvider::GetSampleData()
	{
		// Decode samples until we reach the minimum sample duration threshold or EOS
		IBuffer sampleBuf{ nullptr };
		int64_t pts{ -1 };
		int64_t dur{ 0 };
		bool firstDecodedSample{ true };
		uint32_t decodeErrors{ 0 };
		vector<uint8_t> compactedSampleBuf;

		// Check if we had a decode error on the last GetSampleData() call
		if (m_lastDecodeFailed)
		{
			decodeErrors++;
			m_isDiscontinuous = true;
			m_lastDecodeFailed = false;
		}

		while (true)
		{
			// Get the next decoded sample
			AVFrame_ptr frame;
			try
			{
				frame = GetFrame();
				decodeErrors = 0;
			}
			catch (...)
			{
				const hresult hr{ to_hresult() };
				switch (hr)
				{
				case MF_E_END_OF_STREAM:
					// We've reached EOF
					if (firstDecodedSample)
					{
						// Nothing more to do
						throw;
					}
					else
					{
						// Return the decoded sample data we have
						sampleBuf = make<FFmpegInteropBuffer>(move(compactedSampleBuf));
					}

					break;

				case E_OUTOFMEMORY:
					// Always treat as fatal error
					throw;

				default:
					// Unexpected decode error
					if (decodeErrors < m_allowedDecodeErrors)
					{
						decodeErrors++;
						TraceLoggingWrite(g_FFmpegInteropProvider, "AllowedDecodeError", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
							TraceLoggingValue(m_stream->index, "StreamId"),
							TraceLoggingValue(decodeErrors, "DecodeErrorCount"),
							TraceLoggingValue(m_allowedDecodeErrors, "DecodeErrorLimit"));

						if (firstDecodedSample)
						{
							m_isDiscontinuous = true;
						}
						else
						{
							m_lastDecodeFailed = true;

							// Return the decoded sample data we have
							sampleBuf = make<FFmpegInteropBuffer>(move(compactedSampleBuf));
						}
					}
					else
					{
						throw;
					}

					break;
				}

				if (sampleBuf != nullptr)
				{
					// Return the decoded sample data we have
					break;
				}
				else
				{
					// Try to decode a sample again
					continue;
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

				int resampledSamplesCount{ swr_convert(m_swrContext.get(), &buf, frame->nb_samples, const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples) };
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

			dur += ConvertToAVTime(curSampleCount, m_codecContext->sample_rate, m_stream->time_base);

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

		return { move(sampleBuf), pts, dur, { }, { } };
	}
}
