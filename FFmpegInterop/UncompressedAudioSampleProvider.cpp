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
		m_minAudioSampleDur(ConvertToAVTime(MIN_AUDIO_SAMPLE_DUR_MS, MS_PER_SEC, m_stream->time_base)),
		m_inputSampleFormat(m_codecContext->sample_fmt),
		m_channelLayout(m_codecContext->ch_layout),
		m_sampleRate(m_codecContext->sample_rate)
	{
		if (m_sampleRate != AV_SAMPLE_FMT_S16)
		{
			InitResampler();
		}
	}

	void UncompressedAudioSampleProvider::InitResampler()
	{
		SwrContext* swrContext{ m_swrContext.release() };
		THROW_HR_IF_FFMPEG_FAILED(swr_alloc_set_opts2(
			&swrContext,
			&m_channelLayout,
			AV_SAMPLE_FMT_S16,
			m_sampleRate,
			&m_channelLayout,
			m_inputSampleFormat,
			m_sampleRate,
			0,
			nullptr));
		m_swrContext.reset(exchange(swrContext, nullptr));

		THROW_HR_IF_FFMPEG_FAILED(swr_init(m_swrContext.get()));
	}

	void UncompressedAudioSampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
	{
		// We intentionally don't call SampleProvider::SetEncodingProperties() here as
		// it would set encoding properties with values for the compressed audio type.

		MediaPropertySet properties{ encProp.Properties() };
		if (m_channelLayout.order == AV_CHANNEL_ORDER_NATIVE)
		{
			properties.Insert(MF_MT_AUDIO_CHANNEL_MASK, PropertyValue::CreateUInt32(static_cast<uint32_t>(m_channelLayout.u.mask)));
		}
		else if (m_channelLayout.order != AV_CHANNEL_ORDER_UNSPEC)
		{
			// We don't currently support other channel orders
			THROW_HR(MF_E_INVALIDMEDIATYPE);
		}
	}

	void UncompressedAudioSampleProvider::Flush() noexcept
	{
		UncompressedSampleProvider::Flush();

		m_lastDecodeFailed = false;
		m_formatChangeFrame.reset();
	}

	tuple<IBuffer, int64_t, int64_t, vector<pair<GUID, Windows::Foundation::IInspectable>>, vector<pair<GUID, Windows::Foundation::IInspectable>>> UncompressedAudioSampleProvider::GetSampleData()
	{
		// Decode samples until we reach the minimum sample duration threshold or EOS
		IBuffer sampleBuf{ nullptr };
		int64_t pts{ -1 };
		int64_t dur{ 0 };
		vector<pair<GUID, Windows::Foundation::IInspectable>> formatChanges;
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
				frame = m_formatChangeFrame != nullptr ? move(m_formatChangeFrame) : GetFrame();
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
						TraceLoggingProviderWrite(FFmpegInteropProvider, "AllowedDecodeError", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
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

			// Check for a dynamic format change
			if (m_inputSampleFormat != frame->format ||
				av_channel_layout_compare(&m_channelLayout, &frame->ch_layout) != 0 ||
				m_sampleRate != frame->sample_rate)
			{
				if (firstDecodedSample)
				{
					// Get the list of format changes
					if (m_inputSampleFormat != frame->format)
					{
						m_inputSampleFormat = static_cast<AVSampleFormat>(frame->format);
					}

					if (av_channel_layout_compare(&m_channelLayout, &frame->ch_layout) != 0)
					{
						if (frame->ch_layout.order == AV_CHANNEL_ORDER_NATIVE)
						{
							formatChanges.emplace_back(MF_MT_AUDIO_CHANNEL_MASK, PropertyValue::CreateUInt32(static_cast<uint32_t>(frame->ch_layout.u.mask)));
						}
						else if (frame->ch_layout.order != AV_CHANNEL_ORDER_UNSPEC)
						{
							// We don't currently support other channel orders
							THROW_HR(MF_E_INVALIDMEDIATYPE);
						}

						if (m_channelLayout.nb_channels != frame->ch_layout.nb_channels)
						{
							formatChanges.emplace_back(MF_MT_AUDIO_NUM_CHANNELS, PropertyValue::CreateUInt32(frame->ch_layout.nb_channels));
							formatChanges.emplace_back(MF_MT_AUDIO_BLOCK_ALIGNMENT, 
								PropertyValue::CreateUInt32(frame->ch_layout.nb_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)));
							formatChanges.emplace_back(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 
								PropertyValue::CreateUInt32(frame->sample_rate * frame->ch_layout.nb_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)));
						}
						
						m_channelLayout = frame->ch_layout;
					}

					if (m_sampleRate != frame->sample_rate)
					{
						formatChanges.emplace_back(MF_MT_AUDIO_SAMPLES_PER_SECOND, PropertyValue::CreateUInt32(frame->sample_rate));
						if (none_of(formatChanges.begin(), formatChanges.end(), 
									[](const auto& val) { return val.first == MF_MT_AUDIO_SAMPLES_PER_SECOND; }))
						{
							formatChanges.emplace_back(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
								PropertyValue::CreateUInt32(frame->sample_rate * frame->ch_layout.nb_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)));
						}

						m_sampleRate = frame->sample_rate;
					}

					//  Check if the resampler is needed
					if (m_inputSampleFormat != AV_SAMPLE_FMT_S16)
					{
						InitResampler();
					}
					else
					{
						m_swrContext.reset();
					}
				}
				else
				{
					// We already have compacted samples in the old format. Return the decoded sample data
					// we have and wait until the next sample request to trigger the format change.
					m_formatChangeFrame = move(frame);
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
				int bufSize{ av_samples_alloc(&buf, nullptr, frame->ch_layout.nb_channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 0) };
				THROW_HR_IF_FFMPEG_FAILED(bufSize);

				int resampledSamplesCount{ swr_convert(m_swrContext.get(), &buf, frame->nb_samples, const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples) };
				AVBlob_ptr resampledData{ exchange(buf, nullptr) };
				THROW_HR_IF_FFMPEG_FAILED(resampledSamplesCount);

				const uint32_t resampledDataSize{ static_cast<uint32_t>(resampledSamplesCount) * frame->ch_layout.nb_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) };
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
				TraceLoggingProviderWrite(FFmpegInteropProvider, "CompactingUncompressedAudioSamples", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
					TraceLoggingValue(m_stream->index, "StreamId"),
					TraceLoggingValue(dur, "CompactedDur"));

				firstDecodedSample = false;
			}
		}

		return { move(sampleBuf), pts, dur, { }, move(formatChanges) };
	}
}
