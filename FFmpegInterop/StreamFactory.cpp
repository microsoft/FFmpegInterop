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
#include "StreamFactory.h"
#include "FFmpegInteropMSSConfig.h"
#include "SampleProvider.h"
#include "ACMSampleProvider.h"
#include "AV1SampleProvider.h"
#include "FLACSampleProvider.h"
#include "H264SampleProvider.h"
#include "HEVCSampleProvider.h"
#include "MPEGSampleProvider.h"
#include "SubtitleSampleProvider.h"
#include "UncompressedAudioSampleProvider.h"
#include "UncompressedVideoSampleProvider.h"
#include "VFWSampleProvider.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::MediaProperties;
using namespace std;

namespace
{
	template<class T>
	T CreateEncProp(_In_ const hstring& subtype)
	{
		T encProp;
		encProp.Subtype(subtype);
		return encProp;
	}

	template<class T>
	T CreateEncProp(_In_ const GUID& subtype)
	{
		return CreateEncProp<T>(to_hstring(subtype));
	}

	AudioEncodingProperties CreateAudioEncProp(_In_ const GUID& subtype)
	{
		return CreateEncProp<AudioEncodingProperties>(subtype);
	}

	VideoEncodingProperties CreateVideoEncProp(_In_ const GUID& subtype)
	{
		return CreateEncProp<VideoEncodingProperties>(subtype);
	}

	TimedMetadataEncodingProperties CreateSubtitleEncProp(_In_ const GUID& subtype)
	{
		return CreateEncProp<TimedMetadataEncodingProperties>(subtype);
	}

	void SetStreamDescriptorProperties(_In_ const AVStream* stream, _Inout_ const IMediaStreamDescriptor& streamDescriptor)
	{
		wstring_convert<codecvt_utf8<wchar_t>> conv;

		const AVDictionaryEntry* titleTag{ av_dict_get(stream->metadata, "title", nullptr, 0) };
		if (titleTag != nullptr)
		{
			streamDescriptor.Name(conv.from_bytes(titleTag->value));
		}

		const AVDictionaryEntry* languageTag{ av_dict_get(stream->metadata, "language", nullptr, 0) };
		if (languageTag != nullptr)
		{
			streamDescriptor.Language(conv.from_bytes(languageTag->value));
		}
	}
}

namespace winrt::FFmpegInterop::implementation
{
	tuple<unique_ptr<SampleProvider>, AudioStreamDescriptor> StreamFactory::CreateAudioStream(_In_ const AVStream* stream, _In_ Reader& reader, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		// Create the sample provider and encoding properties
		unique_ptr<SampleProvider> audioSampleProvider;
		AudioEncodingProperties audioEncProp{ nullptr };
		bool setFormatUserData{ false };

		AVCodecID codecId{ stream->codecpar->codec_id };
		TraceLoggingWrite(g_FFmpegInteropProvider, "CreateAudioStream", TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
			TraceLoggingValue(stream->index, "StreamId"),
			TraceLoggingInt32(codecId, "AVCodecID"));

		if (config != nullptr && config.ForceAudioDecode())
		{
			TraceLoggingWrite(g_FFmpegInteropProvider, "ForceAudioDecode", TraceLoggingLevel(TRACE_LEVEL_VERBOSE));
			codecId = AV_CODEC_ID_NONE;
		}

		switch (codecId)
		{
		case AV_CODEC_ID_AAC:
			if (stream->codecpar->extradata_size == 0)
			{
				audioEncProp = AudioEncodingProperties::CreateAacAdts(stream->codecpar->sample_rate, stream->codecpar->channels, static_cast<uint32_t>(stream->codecpar->bit_rate));
			}
			else
			{
				audioEncProp = AudioEncodingProperties::CreateAac(stream->codecpar->sample_rate, stream->codecpar->channels, static_cast<uint32_t>(stream->codecpar->bit_rate));
			}

			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_AC3:
			audioEncProp = CreateAudioEncProp(MFAudioFormat_Dolby_AC3);
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_ALAC:
			audioEncProp = CreateAudioEncProp(MFAudioFormat_ALAC);
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			setFormatUserData = true;
			break;

		case AV_CODEC_ID_DTS:
			audioEncProp = CreateAudioEncProp(MFAudioFormat_DTS_HD);
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_EAC3:
			audioEncProp = CreateAudioEncProp(MFAudioFormat_Dolby_DDPlus);
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_FLAC:
			audioEncProp = CreateAudioEncProp(MFAudioFormat_FLAC);
			audioSampleProvider = make_unique<FLACSampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_MP1:
		case AV_CODEC_ID_MP2:
			audioEncProp = CreateAudioEncProp(MFAudioFormat_MPEG);
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_MP3:
			audioEncProp = AudioEncodingProperties::CreateMp3(stream->codecpar->sample_rate, stream->codecpar->channels, static_cast<uint32_t>(stream->codecpar->bit_rate));
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_OPUS:
			audioEncProp = CreateAudioEncProp(MFAudioFormat_Opus);
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			setFormatUserData = true;
			break;

		case AV_CODEC_ID_PCM_F32LE:
		case AV_CODEC_ID_PCM_F64LE:
			audioEncProp = CreateAudioEncProp(MFAudioFormat_Float);
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_PCM_S16BE:
		case AV_CODEC_ID_PCM_S16LE:
		case AV_CODEC_ID_PCM_S24BE:
		case AV_CODEC_ID_PCM_S24LE:
		case AV_CODEC_ID_PCM_S32BE:
		case AV_CODEC_ID_PCM_S32LE:
		case AV_CODEC_ID_PCM_U8:
			audioEncProp = CreateAudioEncProp(MFAudioFormat_PCM);
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_TRUEHD:
			audioEncProp = CreateAudioEncProp(MEDIASUBTYPE_DOLBY_TRUEHD);
			audioSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_PCM_MULAW:
		case AV_CODEC_ID_WMALOSSLESS:
		case AV_CODEC_ID_WMAPRO:
		case AV_CODEC_ID_WMAV1:
		case AV_CODEC_ID_WMAV2:
		case AV_CODEC_ID_WMAVOICE:
			audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
			audioSampleProvider = make_unique<ACMSampleProvider>(stream, reader);
			break;

		default:
			constexpr uint32_t bitsPerSample{ 16 };
			audioEncProp = AudioEncodingProperties::CreatePcm(stream->codecpar->sample_rate, stream->codecpar->channels, bitsPerSample);
			audioSampleProvider = make_unique<UncompressedAudioSampleProvider>(stream, reader);
			break;
		}

		audioSampleProvider->SetEncodingProperties(audioEncProp, setFormatUserData);

		// Create the stream descriptor
		AudioStreamDescriptor audioStreamDescriptor{ audioEncProp };
		SetStreamDescriptorProperties(stream, audioStreamDescriptor);

		return { move(audioSampleProvider), move(audioStreamDescriptor) };
	}

	tuple<unique_ptr<SampleProvider>, VideoStreamDescriptor> StreamFactory::CreateVideoStream(_In_ const AVStream* stream, _In_ Reader& reader, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		// Create the sample provider and encoding properties
		unique_ptr<SampleProvider> videoSampleProvider;
		VideoEncodingProperties videoEncProp{ nullptr };
		bool setFormatUserData{ false };

		AVCodecID codecId{ stream->codecpar->codec_id };
		TraceLoggingWrite(g_FFmpegInteropProvider, "CreateVideoStream", TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
			TraceLoggingValue(stream->index, "StreamId"),
			TraceLoggingInt32(codecId, "AVCodecID"));

		if (config != nullptr && config.ForceVideoDecode())
		{
			TraceLoggingWrite(g_FFmpegInteropProvider, "ForceVideoDecode", TraceLoggingLevel(TRACE_LEVEL_VERBOSE));
			codecId = AV_CODEC_ID_NONE;
		}

		switch (codecId)
		{
		case AV_CODEC_ID_AV1:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_AV1);
			videoSampleProvider = make_unique<AV1SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_H264:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_H264);
			videoSampleProvider = make_unique<H264SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_HEVC:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_HEVC);
			videoSampleProvider = make_unique<HEVCSampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_MJPEG:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_MJPG);
			videoSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_MPEG1VIDEO:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_MPG1);
			videoSampleProvider = make_unique<MPEGSampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_MPEG2VIDEO:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_MPEG2);
			videoSampleProvider = make_unique<MPEGSampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_MPEG4:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_MP4V);
			videoSampleProvider = make_unique<SampleProvider>(stream, reader);
			setFormatUserData = true;
			break;

		case AV_CODEC_ID_MSMPEG4V3:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_MP43);
			videoSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_VP8:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_VP80);
			videoSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_VP9:
			videoEncProp = CreateVideoEncProp(MFVideoFormat_VP90);
			videoSampleProvider = make_unique<SampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_AYUV:
		case AV_CODEC_ID_DVVIDEO:
		case AV_CODEC_ID_H263:
		case AV_CODEC_ID_MSMPEG4V1:
		case AV_CODEC_ID_MSMPEG4V2:
		case AV_CODEC_ID_VC1:
		case AV_CODEC_ID_WMV1:
		case AV_CODEC_ID_WMV2:
		case AV_CODEC_ID_WMV3:
			videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
			videoSampleProvider = make_unique<VFWSampleProvider>(stream, reader);
			setFormatUserData = true;
			break;

		default:
			videoEncProp = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Nv12(), stream->codecpar->width, stream->codecpar->height);
			videoSampleProvider = make_unique<UncompressedVideoSampleProvider>(stream, reader);
			break;
		}

		videoSampleProvider->SetEncodingProperties(videoEncProp, setFormatUserData);

		// Create the stream descriptor
		VideoStreamDescriptor videoStreamDescriptor{ videoEncProp };
		SetStreamDescriptorProperties(stream, videoStreamDescriptor);

		return { move(videoSampleProvider), move(videoStreamDescriptor) };
	}

	tuple<unique_ptr<SampleProvider>, TimedMetadataStreamDescriptor> StreamFactory::CreateSubtitleStream(_In_ const AVStream* stream, _In_ Reader& reader)
	{
		// Create the sample provider and encoding properties
		unique_ptr<SubtitleSampleProvider> subtitleSampleProvider;
		TimedMetadataEncodingProperties subtitleEncProp{ nullptr };
		bool setFormatUserData{ false };

		AVCodecID codecId{ stream->codecpar->codec_id };
		TraceLoggingWrite(g_FFmpegInteropProvider, "CreateSubtitleStream", TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
			TraceLoggingValue(stream->index, "StreamId"),
			TraceLoggingInt32(codecId, "AVCodecID"));

		switch (codecId)
		{
		case AV_CODEC_ID_ASS:
		case AV_CODEC_ID_SSA:
			subtitleEncProp = CreateSubtitleEncProp(MFSubtitleFormat_SSA);
			subtitleSampleProvider = make_unique<SubtitleSampleProvider>(stream, reader);
			setFormatUserData = true;
			break;

		case AV_CODEC_ID_DVD_SUBTITLE:
			subtitleEncProp = CreateEncProp<TimedMetadataEncodingProperties>(L"VobSub"); // MFSubtitleFormat_VobSub
			subtitleSampleProvider = make_unique<SubtitleSampleProvider>(stream, reader);
			setFormatUserData = true;
			break;

		case AV_CODEC_ID_HDMV_PGS_SUBTITLE:
			subtitleEncProp = CreateEncProp<TimedMetadataEncodingProperties>(L"PGS"); // MFSubtitleFormat_PGS
			subtitleSampleProvider = make_unique<SubtitleSampleProvider>(stream, reader);
			break;

		case AV_CODEC_ID_SUBRIP:
		case AV_CODEC_ID_TEXT:
			subtitleEncProp = CreateSubtitleEncProp(MFSubtitleFormat_SRT);
			subtitleSampleProvider = make_unique<SubtitleSampleProvider>(stream, reader);
			break;

		default:
			// We don't support this subtitle codec
			THROW_HR(MF_E_INVALIDMEDIATYPE);
		}

		subtitleSampleProvider->SetEncodingProperties(subtitleEncProp, setFormatUserData);

		// Create the stream descriptor
		TimedMetadataStreamDescriptor subtitleStreamDescriptor{ subtitleEncProp };
		SetStreamDescriptorProperties(stream, subtitleStreamDescriptor);

		return { move(subtitleSampleProvider), move(subtitleStreamDescriptor) };
	}
}
