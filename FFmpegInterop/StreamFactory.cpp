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
#include "AV1SampleProvider.h"
#include "FLACSampleProvider.h"
#include "H264SampleProvider.h"
#include "HEVCSampleProvider.h"
#include "MPEGSampleProvider.h"
#include "SubtitleSampleProvider.h"
#include "UncompressedAudioSampleProvider.h"
#include "UncompressedVideoSampleProvider.h"

using namespace winrt::FFmpegInterop::implementation;
using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::MediaProperties;
using namespace std;

tuple<unique_ptr<SampleProvider>, AudioStreamDescriptor> StreamFactory::CreateAudioStream(_In_ const AVStream* stream, _In_ Reader& reader, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
{
	// Create the sample provider and encoding properties
	unique_ptr<SampleProvider> audioSampleProvider;
	AudioEncodingProperties audioEncProp{ nullptr };
	bool setFormatUserData{ false };

	AVCodecID codecId{ stream->codecpar->codec_id };

	if (config != nullptr && config.ForceAudioDecode())
	{
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
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MFAudioFormat_Dolby_AC3));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_ALAC:
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MFAudioFormat_ALAC));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
		setFormatUserData = true;
		break;

	case AV_CODEC_ID_DTS:
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MFAudioFormat_DTS_HD));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_EAC3:
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MFAudioFormat_Dolby_DDPlus));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_FLAC:
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MFAudioFormat_FLAC));
		audioSampleProvider = make_unique<FLACSampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_MP1:
	case AV_CODEC_ID_MP2:
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MFAudioFormat_MPEG));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_MP3:
		audioEncProp = AudioEncodingProperties::CreateMp3(stream->codecpar->sample_rate, stream->codecpar->channels, static_cast<uint32_t>(stream->codecpar->bit_rate));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_OPUS:
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MFAudioFormat_Opus));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
		setFormatUserData = true;
		break;

	case AV_CODEC_ID_PCM_F32LE:
	case AV_CODEC_ID_PCM_F64LE:
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MFAudioFormat_Float));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_PCM_S16BE:
	case AV_CODEC_ID_PCM_S16LE:
	case AV_CODEC_ID_PCM_S24BE:
	case AV_CODEC_ID_PCM_S24LE:
	case AV_CODEC_ID_PCM_S32BE:
	case AV_CODEC_ID_PCM_S32LE:
	case AV_CODEC_ID_PCM_U8:
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MFAudioFormat_PCM));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_TRUEHD:
		audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
		audioEncProp.Subtype(to_hstring(MEDIASUBTYPE_DOLBY_TRUEHD));
		audioSampleProvider = make_unique<SampleProvider>(stream, reader);
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

	if (config != nullptr && config.ForceVideoDecode())
	{
		codecId = AV_CODEC_ID_NONE;
	}

	switch (codecId)
	{
	case AV_CODEC_ID_AV1:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_AV1));
		videoSampleProvider = make_unique<AV1SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_H264:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_H264));
		videoSampleProvider = make_unique<H264SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_HEVC:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_HEVC));
		videoSampleProvider = make_unique<HEVCSampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_MJPEG:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_MJPG));
		videoSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_MPEG1VIDEO:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_MPG1));
		videoSampleProvider = make_unique<MPEGSampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_MPEG2VIDEO:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_MPEG2));
		videoSampleProvider = make_unique<MPEGSampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_MPEG4:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_MP4V));
		videoSampleProvider = make_unique<SampleProvider>(stream, reader);
		setFormatUserData = true;
		break;

	case AV_CODEC_ID_MSMPEG4V3:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_MP43));
		videoSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_VP8:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_VP80));
		videoSampleProvider = make_unique<SampleProvider>(stream, reader);
		break;

	case AV_CODEC_ID_VP9:
		videoEncProp = VideoEncodingProperties::VideoEncodingProperties();
		videoEncProp.Subtype(to_hstring(MFVideoFormat_VP90));
		videoSampleProvider = make_unique<SampleProvider>(stream, reader);
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
	TimedMetadataEncodingProperties subtitleEncProp;
	bool setFormatUserData{ false };

	switch (stream->codecpar->codec_id)
	{
	case AV_CODEC_ID_ASS:
	case AV_CODEC_ID_SSA:
		subtitleSampleProvider = make_unique<SubtitleSampleProvider>(stream, reader);
		subtitleEncProp.Subtype(L"SSA");
		setFormatUserData = true;
		break;

	case AV_CODEC_ID_DVD_SUBTITLE:
		subtitleSampleProvider = make_unique<SubtitleSampleProvider>(stream, reader);
		subtitleEncProp.Subtype(L"VobSub");
		setFormatUserData = true;
		break;

	case AV_CODEC_ID_HDMV_PGS_SUBTITLE:
		subtitleSampleProvider = make_unique<SubtitleSampleProvider>(stream, reader);
		subtitleEncProp.Subtype(L"PGS");
		break;

	case AV_CODEC_ID_SUBRIP:
	case AV_CODEC_ID_TEXT:
		subtitleSampleProvider = make_unique<SubtitleSampleProvider>(stream, reader);
		subtitleEncProp.Subtype(L"SRT");
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

void StreamFactory::SetStreamDescriptorProperties(_In_ const AVStream* stream, _Inout_ const IMediaStreamDescriptor& streamDescriptor)
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
