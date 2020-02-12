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
#include "SampleProvider.h"
#include "Reader.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::MediaProperties;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	SampleProvider::SampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
		m_stream(stream),
		m_reader(reader)
	{
		if (m_stream->start_time != AV_NOPTS_VALUE)
		{
			m_startOffset = m_stream->start_time;
			m_nextSamplePts = m_stream->start_time;
		}
	}

	void SampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
	{
		const AVCodecParameters* codecPar{ m_stream->codecpar };

		switch (codecPar->codec_type)
		{
		case AVMEDIA_TYPE_AUDIO:
		{
			IAudioEncodingProperties audioEncProp{ encProp.as<IAudioEncodingProperties>() };
			audioEncProp.SampleRate(codecPar->sample_rate);
			audioEncProp.ChannelCount(codecPar->channels);
			audioEncProp.Bitrate(static_cast<uint32_t>(codecPar->bit_rate));
			audioEncProp.BitsPerSample(static_cast<uint32_t>(max(codecPar->bits_per_coded_sample, codecPar->bits_per_raw_sample)));

			if (setFormatUserData && codecPar->extradata != nullptr && codecPar->extradata_size > 0)
			{
				// Set the format user data
				IAudioEncodingPropertiesWithFormatUserData audioEncPropWithFormatUserData{ encProp.as<IAudioEncodingPropertiesWithFormatUserData>() };
				audioEncPropWithFormatUserData.SetFormatUserData({ codecPar->extradata, codecPar->extradata + codecPar->extradata_size });
			}

			MediaPropertySet properties{ audioEncProp.Properties() };

			if (GUID subtype{ unbox_value<GUID>(properties.Lookup(MF_MT_SUBTYPE)) }; subtype == MFAudioFormat_PCM || subtype == MFAudioFormat_Float)
			{
				properties.Insert(MF_MT_ALL_SAMPLES_INDEPENDENT, PropertyValue::CreateUInt32(true));
				properties.Insert(MF_MT_COMPRESSED, PropertyValue::CreateUInt32(false));
			}
			else
			{
				properties.Insert(MF_MT_ALL_SAMPLES_INDEPENDENT, PropertyValue::CreateUInt32(false));
				properties.Insert(MF_MT_COMPRESSED, PropertyValue::CreateUInt32(true));
			}
			
			break;
		}

		case AVMEDIA_TYPE_VIDEO:
		{
			IVideoEncodingProperties videoEncProp{ encProp.as<IVideoEncodingProperties>() };
			videoEncProp.Height(codecPar->height);
			videoEncProp.Width(codecPar->width);
			videoEncProp.Bitrate(static_cast<uint32_t>(codecPar->bit_rate));

			if (codecPar->sample_aspect_ratio.num > 0 && codecPar->sample_aspect_ratio.den != 0)
			{
				MediaRatio pixelAspectRatio{ videoEncProp.PixelAspectRatio() };
				pixelAspectRatio.Numerator(codecPar->sample_aspect_ratio.num);
				pixelAspectRatio.Denominator(codecPar->sample_aspect_ratio.den);
			}

			if (m_stream->avg_frame_rate.num != 0 || m_stream->avg_frame_rate.den != 0)
			{
				MediaRatio frameRate{ videoEncProp.FrameRate() };
				frameRate.Numerator(m_stream->avg_frame_rate.num);
				frameRate.Denominator(m_stream->avg_frame_rate.den);
			}

			if (setFormatUserData && codecPar->extradata != nullptr && codecPar->extradata_size > 0)
			{
				// Set the format user data
				IVideoEncodingProperties2 videoEncProp2{ encProp.as<IVideoEncodingProperties2>() };
				videoEncProp2.SetFormatUserData({ codecPar->extradata, codecPar->extradata + codecPar->extradata_size });
			}

			MediaPropertySet properties{ videoEncProp.Properties() };

			properties.Insert(MF_MT_ALL_SAMPLES_INDEPENDENT, PropertyValue::CreateUInt32(false));
			properties.Insert(MF_MT_COMPRESSED, PropertyValue::CreateUInt32(true));
			properties.Insert(MF_MT_PIXEL_ASPECT_RATIO, PropertyValue::CreateUInt64(Pack2UINT32AsUINT64(1, 1)));

			if (const AVDictionaryEntry* rotateTag{ av_dict_get(m_stream->metadata, "rotate", nullptr, 0) }; rotateTag != nullptr)
			{
				properties.Insert(MF_MT_VIDEO_ROTATION, PropertyValue::CreateUInt32(atoi(rotateTag->value)));
			}

			if (codecPar->color_primaries != AVCOL_PRI_UNSPECIFIED)
			{
				MFVideoPrimaries videoPrimaries{ MFVideoPrimaries_Unknown };
				switch (codecPar->color_primaries)
				{
				case AVCOL_PRI_RESERVED0:
				case AVCOL_PRI_RESERVED:
					videoPrimaries = MFVideoPrimaries_reserved;
					break;

				case AVCOL_PRI_BT709:
					videoPrimaries = MFVideoPrimaries_BT709;
					break;

				case  AVCOL_PRI_BT470M:
					videoPrimaries = MFVideoPrimaries_BT470_2_SysM;
					break;

				case AVCOL_PRI_BT470BG:
					videoPrimaries = MFVideoPrimaries_BT470_2_SysBG;
					break;

				case AVCOL_PRI_SMPTE170M:
					videoPrimaries = MFVideoPrimaries_SMPTE170M;
					break;

				case AVCOL_PRI_SMPTE240M:
					videoPrimaries = MFVideoPrimaries_SMPTE240M;
					break;

				case AVCOL_PRI_FILM:
					videoPrimaries = MFVideoPrimaries_SMPTE_C;
					break;

				case AVCOL_PRI_BT2020:
					videoPrimaries = MFVideoPrimaries_BT2020;
					break;

				default:
					break;
				}

				properties.Insert(MF_MT_VIDEO_PRIMARIES, PropertyValue::CreateUInt32(videoPrimaries));
			}

			if (codecPar->color_trc != AVCOL_TRC_UNSPECIFIED)
			{
				MFVideoTransferFunction videoTransferFunc{ MFVideoTransFunc_Unknown };
				switch (codecPar->color_trc)
				{
				case AVCOL_TRC_BT709:
				case AVCOL_TRC_GAMMA22:
				case AVCOL_TRC_SMPTE170M:
					videoTransferFunc = MFVideoTransFunc_22;
					break;

				case AVCOL_TRC_GAMMA28:
					videoTransferFunc = MFVideoTransFunc_28;
					break;

				case AVCOL_TRC_SMPTE240M:
					videoTransferFunc = MFVideoTransFunc_240M;
					break;

				case AVCOL_TRC_LINEAR:
					videoTransferFunc = MFVideoTransFunc_10;
					break;

				case AVCOL_TRC_LOG:
					videoTransferFunc = MFVideoTransFunc_Log_100;
					break;

				case AVCOL_TRC_LOG_SQRT:
					videoTransferFunc = MFVideoTransFunc_Log_316;
					break;

				case AVCOL_TRC_BT1361_ECG:
					videoTransferFunc = MFVideoTransFunc_709;
					break;

				case AVCOL_TRC_BT2020_10:
				case AVCOL_TRC_BT2020_12:
					videoTransferFunc = MFVideoTransFunc_2020;
					break;

				case AVCOL_TRC_SMPTEST2084:
					videoTransferFunc = MFVideoTransFunc_2084;
					break;

				case AVCOL_TRC_ARIB_STD_B67:
					videoTransferFunc = MFVideoTransFunc_HLG;
					break;

				default:
					break;  
				}

				properties.Insert(MF_MT_TRANSFER_FUNCTION, PropertyValue::CreateUInt32(videoTransferFunc));
			}

			if (codecPar->color_range != AVCOL_RANGE_UNSPECIFIED)
			{
				MFNominalRange nominalRange{ codecPar->color_range == AVCOL_RANGE_JPEG ? MFNominalRange_0_255 : MFNominalRange_16_235 };
				properties.Insert(MF_MT_VIDEO_NOMINAL_RANGE, PropertyValue::CreateUInt32(nominalRange));
			}

			if (AVContentLightMetadata* contentLightMetadata{ reinterpret_cast<AVContentLightMetadata*>(av_stream_get_side_data(m_stream, AV_PKT_DATA_CONTENT_LIGHT_LEVEL, nullptr)) }; contentLightMetadata != nullptr)
			{
				properties.Insert(MF_MT_MAX_LUMINANCE_LEVEL, PropertyValue::CreateUInt32(contentLightMetadata->MaxCLL));
				properties.Insert(MF_MT_MAX_FRAME_AVERAGE_LUMINANCE_LEVEL, PropertyValue::CreateUInt32(contentLightMetadata->MaxFALL));
			}

			if (AVMasteringDisplayMetadata* masteringDisplayMetadata{ reinterpret_cast<AVMasteringDisplayMetadata*>(av_stream_get_side_data(m_stream, AV_PKT_DATA_MASTERING_DISPLAY_METADATA, nullptr)) }; masteringDisplayMetadata != nullptr)
			{
				if (masteringDisplayMetadata->has_luminance)
				{
					constexpr uint32_t MASTERING_DISP_LUMINANCE_SCALE{ 10000 };
					properties.Insert(MF_MT_MIN_MASTERING_LUMINANCE, PropertyValue::CreateUInt32(static_cast<uint32_t>(MASTERING_DISP_LUMINANCE_SCALE * av_q2d(masteringDisplayMetadata->min_luminance))));
					properties.Insert(MF_MT_MAX_MASTERING_LUMINANCE, PropertyValue::CreateUInt32(static_cast<uint32_t>(av_q2d(masteringDisplayMetadata->max_luminance))));
				}

				if (masteringDisplayMetadata->has_primaries)
				{
					MT_CUSTOM_VIDEO_PRIMARIES customVideoPrimaries
					{
						static_cast<float>(av_q2d(masteringDisplayMetadata->display_primaries[0][0])),
						static_cast<float>(av_q2d(masteringDisplayMetadata->display_primaries[0][1])),
						static_cast<float>(av_q2d(masteringDisplayMetadata->display_primaries[1][0])),
						static_cast<float>(av_q2d(masteringDisplayMetadata->display_primaries[1][1])),
						static_cast<float>(av_q2d(masteringDisplayMetadata->display_primaries[2][0])),
						static_cast<float>(av_q2d(masteringDisplayMetadata->display_primaries[2][1])),
						static_cast<float>(av_q2d(masteringDisplayMetadata->white_point[0])),
						static_cast<float>(av_q2d(masteringDisplayMetadata->white_point[1]))
					};
					properties.Insert(MF_MT_CUSTOM_VIDEO_PRIMARIES, PropertyValue::CreateUInt8Array({ reinterpret_cast<uint8_t*>(&customVideoPrimaries), reinterpret_cast<uint8_t*>(&customVideoPrimaries) + sizeof(customVideoPrimaries) }));
				}
			}

			break;
		}

		case AVMEDIA_TYPE_SUBTITLE:
		{
			ITimedMetadataEncodingProperties subtitleEncProp{ encProp.as<ITimedMetadataEncodingProperties>() };

			if (setFormatUserData && codecPar->extradata != nullptr && codecPar->extradata_size > 0)
			{
				// Set the format user data
				subtitleEncProp.SetFormatUserData({ codecPar->extradata, codecPar->extradata + codecPar->extradata_size });
			}

			MediaPropertySet properties{ encProp.Properties() };

			properties.Insert(MF_MT_ALL_SAMPLES_INDEPENDENT, PropertyValue::CreateUInt32(true));
			properties.Insert(MF_MT_COMPRESSED, PropertyValue::CreateUInt32(false));

			break;
		}

		default:
			WINRT_ASSERT(false);
			THROW_HR(E_UNEXPECTED);
		}
	}

	void SampleProvider::Select() noexcept
	{
		TraceLoggingWrite(g_FFmpegInteropProvider, "StreamSelected", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
			TraceLoggingValue(m_stream->index, "StreamId"));

		WINRT_ASSERT(!m_isSelected);
		m_isSelected = true;
	}

	void SampleProvider::Deselect() noexcept
	{
		TraceLoggingWrite(g_FFmpegInteropProvider, "StreamDeselected", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
			TraceLoggingValue(m_stream->index, "StreamId"));

		WINRT_ASSERT(m_isSelected);
		m_isSelected = false;
		Flush();
	}

	void SampleProvider::OnSeek(_In_ int64_t seekTime) noexcept
	{
		m_nextSamplePts = seekTime;
		m_isEOS = false;
		Flush();
	}

	void SampleProvider::NotifyEOF() noexcept
	{
		// We've reached EOF so no more packets will be read.
		// If there's no packets in the queue this stream is now EOS.
		if (!m_isEOS && m_packetQueue.empty())
		{
			m_isEOS = true;

			if (m_isSelected)
			{
				TraceLoggingWrite(g_FFmpegInteropProvider, "EndOfStream", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
					TraceLoggingValue(m_stream->index, "StreamId"));
			}
		}
	}

	void SampleProvider::Flush() noexcept
	{
		m_packetQueue.clear();
		m_isDiscontinuous = true;
	}

	void SampleProvider::QueuePacket(_In_ AVPacket_ptr packet)
	{
		if (m_isSelected)
		{
			m_packetQueue.push_back(move(packet));
		}
	}

	void SampleProvider::GetSample(_Inout_ const MediaStreamSourceSampleRequest& request)
	{
		TraceLoggingWrite(g_FFmpegInteropProvider, "SampleRequested", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
			TraceLoggingValue(m_stream->index, "StreamId"));

		// Make sure this stream is selected
		THROW_HR_IF(MF_E_INVALIDREQUEST, !m_isSelected);

		// Get the sample data, timestamp, duration, and properties
		auto [buf, pts, dur, properties] = GetSampleData();

		// Make sure the PTS is set
		if (pts == AV_NOPTS_VALUE)
		{
			pts = m_nextSamplePts;
		}

		// Calculate the PTS for the next sample
		m_nextSamplePts = pts + dur;

		// Convert time base from FFmpeg to MF
		pts = ConvertFromAVTime(pts - m_startOffset, m_stream->time_base, HNS_PER_SEC);
		dur = ConvertFromAVTime(dur, m_stream->time_base, HNS_PER_SEC);

		// Create the sample
		MediaStreamSample sample{ MediaStreamSample::CreateFromBuffer(buf, static_cast<TimeSpan>(pts)) };
		sample.Duration(TimeSpan{ dur });
		sample.Discontinuous(m_isDiscontinuous);

		MediaStreamSamplePropertySet extendedProperties{ sample.ExtendedProperties() };
		for (const auto& [key, value] : properties)
		{
			extendedProperties.Insert(key, value);
		}

		m_isDiscontinuous = false;

		request.Sample(sample);

		TraceLoggingWrite(g_FFmpegInteropProvider, "SampleRequestFilled", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
			TraceLoggingValue(m_stream->index, "StreamId"),
			TraceLoggingValue(sample.Timestamp().count(), "TimestampHNS"),
			TraceLoggingValue(sample.Duration().count(), "DurHNS"));
	}

	tuple<IBuffer, int64_t, int64_t, map<GUID, IInspectable>> SampleProvider::GetSampleData()
	{
		AVPacket_ptr packet{ GetPacket() };

		const int64_t pts{ packet->pts };
		const int64_t dur{ packet->duration };

		// Set sample properties
		map<GUID, IInspectable> properties;

		if ((packet->flags & AV_PKT_FLAG_KEY) != 0)
		{
			properties[MFSampleExtension_CleanPoint] = PropertyValue::CreateUInt32(true);
		}

		return { make<FFmpegInteropBuffer>(move(packet)), pts, dur, move(properties) };
	}

	AVPacket_ptr SampleProvider::GetPacket()
	{
		// Continue reading until there is an appropriate packet in the stream
		while (m_packetQueue.empty())
		{
			m_reader.ReadPacket();
		}

		AVPacket_ptr packet{ move(m_packetQueue.front()) };
		m_packetQueue.pop_front();

		return packet;
	}
}
