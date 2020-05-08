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
	tuple<AVSampleFormat, GUID> UncompressedAudioSampleProvider::GetOutputFormat(_In_ AVSampleFormat format) noexcept
	{
		AVSampleFormat ffmpegOutputFormat{ av_get_packed_sample_fmt(format) };
		GUID mfOutputFormat{ GUID_NULL };

		switch (ffmpegOutputFormat)
		{
		default:
			ffmpegOutputFormat = AV_SAMPLE_FMT_S16;
			__fallthrough;

		case AV_SAMPLE_FMT_U8:
		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S32:
			mfOutputFormat = MFAudioFormat_PCM;
			break;

		case AV_SAMPLE_FMT_FLT:
			mfOutputFormat = MFAudioFormat_Float;
			break;
		}

		WINRT_ASSERT(!av_sample_fmt_is_planar(ffmpegOutputFormat)); // We need the output format to be packed

		return { ffmpegOutputFormat, mfOutputFormat };
	}

	UncompressedAudioSampleProvider::UncompressedAudioSampleProvider(_In_ const AVFormatContext* formatContext, _In_ AVStream* stream, _In_ Reader& reader, _In_ uint32_t allowedDecodeErrors) :
		UncompressedSampleProvider(formatContext, stream, reader, allowedDecodeErrors, &InitCodecContext),
		m_minAudioSampleDur(ConvertToAVTime(MIN_AUDIO_SAMPLE_DUR_MS, MS_PER_SEC, m_stream->time_base))
	{
		m_outputFormat = GetFFmpegOutputFormat(m_codecContext->sample_fmt);
		if (m_codecContext->sample_fmt != m_outputFormat)
		{
			// Set up resampler to convert to the desired output format
			int64_t inChannelLayout{ m_codecContext->channel_layout != 0 ? static_cast<int64_t>(m_codecContext->channel_layout) : av_get_default_channel_layout(m_codecContext->channels) };
			int64_t outChannelLayout{ av_get_default_channel_layout(m_codecContext->channels) };

			m_swrContext.reset(swr_alloc_set_opts(
				m_swrContext.release(),
				outChannelLayout,
				m_outputFormat,
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

	void UncompressedAudioSampleProvider::InitCodecContext(_In_ AVCodecContext* codecContext) noexcept
	{
		codecContext->request_sample_fmt = av_get_packed_sample_fmt(codecContext->sample_fmt); // Try to request packed formats to avoid resampling
	}

	void UncompressedAudioSampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
	{
		// We intentionally don't call SampleProvider::SetEncodingProperties() here as
		// it would set encoding properties with values for the compressed audio type.

		IAudioEncodingProperties audioEncProp{ encProp.as<IAudioEncodingProperties>() };
		audioEncProp.Subtype(to_hstring(GetMFOutputFormat(m_codecContext->sample_fmt)));
		audioEncProp.SampleRate(m_codecContext->sample_rate);
		audioEncProp.ChannelCount(m_codecContext->channels);

		int bitsPerSample{ av_get_bytes_per_sample(m_outputFormat) * BITS_PER_BYTE };
		audioEncProp.Bitrate(static_cast<uint32_t>(m_codecContext->sample_rate * m_codecContext->channels * bitsPerSample));
		audioEncProp.BitsPerSample(static_cast<uint32_t>(bitsPerSample));

		MediaPropertySet properties{ audioEncProp.Properties() };
		properties.Insert(MF_MT_ALL_SAMPLES_INDEPENDENT, PropertyValue::CreateUInt32(false));
		properties.Insert(MF_MT_COMPRESSED, PropertyValue::CreateUInt32(true));
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

			if (!IsUsingResampler())
			{
				// Uncompressed frame is already in the desired output format
				WINRT_ASSERT(!av_sample_fmt_is_planar(static_cast<AVSampleFormat>(frame->format)));
				curSampleBuf = make<FFmpegInteropBuffer>(frame->buf[0]);
				curSampleCount = frame->nb_samples;
			}
			else
			{
				// We need to resample the uncompressed frame to the desired output format.
				// We use the compacted sample buffer as the output buffer for resampling to minimize copying.
				// Allocate buffer space for the resampled data.
				int requiredBufferSize{ av_samples_get_buffer_size(nullptr, frame->channels, frame->nb_samples, m_outputFormat, 0) };
				THROW_HR_IF_FFMPEG_FAILED(requiredBufferSize);

				if (firstDecodedSample)
				{
					// Reserve buffer space to minimize reallocations
					int minSampleCount{ MIN_AUDIO_SAMPLE_DUR_MS * m_codecContext->sample_rate / MS_PER_SEC };
					int minBufferSize{ av_samples_get_buffer_size(nullptr, frame->channels, minSampleCount, m_outputFormat, 0) };
					compactedSampleBuf.reserve(max(minBufferSize, requiredBufferSize));
				}

				compactedSampleBuf.resize(compactedSampleBuf.size() + requiredBufferSize);

				uint8_t* resampledData[AV_NUM_DATA_POINTERS]{ nullptr };
				THROW_HR_IF_FFMPEG_FAILED(av_samples_fill_arrays(resampledData, nullptr, &compactedSampleBuf[compactedSampleBuf.size() - requiredBufferSize], frame->channels, frame->nb_samples, m_outputFormat, 0));

				// Convert to the output format
				int resampledSamplesCount{ swr_convert(m_swrContext.get(), resampledData, frame->nb_samples, const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples) };
				THROW_HR_IF_FFMPEG_FAILED(resampledSamplesCount);

				if (resampledSamplesCount < frame->nb_samples)
				{
					// We got less samples than we expected. We need to shrink the buffer.
					int resampledDataSize{ av_samples_get_buffer_size(nullptr, frame->channels, resampledSamplesCount, m_outputFormat, 0) };
					THROW_HR_IF_FFMPEG_FAILED(resampledDataSize);
					compactedSampleBuf.resize(compactedSampleBuf.size() - (requiredBufferSize - resampledDataSize));
				}
				else if (resampledSamplesCount > frame->nb_samples)
				{
					// There may have been a buffer overflow... This should never happen.
					WINRT_ASSERT(false);
					THROW_HR(E_UNEXPECTED);
				}

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
			if (!IsUsingResampler() && (!firstDecodedSample || !minSampleDurMet))
			{
				if (firstDecodedSample)
				{
					// Reserve buffer space to minimize reallocations
					int minSampleCount{ MIN_AUDIO_SAMPLE_DUR_MS * m_codecContext->sample_rate / MS_PER_SEC };
					int minBufferSize{ av_samples_get_buffer_size(nullptr, frame->channels, minSampleCount, m_outputFormat, 0) };
					compactedSampleBuf.reserve(minBufferSize);
				}

				compactedSampleBuf.insert(compactedSampleBuf.end(), curSampleBuf.data(), curSampleBuf.data() + curSampleBuf.Length());
			}

			// Check if we've reached the minimum sample duration threshold
			if (minSampleDurMet)
			{
				if (!IsUsingResampler() && firstDecodedSample)
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
