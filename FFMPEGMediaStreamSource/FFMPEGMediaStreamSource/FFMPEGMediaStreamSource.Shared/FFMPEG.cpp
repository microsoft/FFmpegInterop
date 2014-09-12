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
#include "FFMPEG.h"

extern "C"
{
	#include <libavutil/imgutils.h>
}

using namespace FFMPEGMediaStreamSource;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::MediaProperties;

FFMPEG::FFMPEG() :
	avFormatCtx(NULL),
	avAudioCodec(NULL),
	avVideoCodec(NULL),
	avAudioCodecCtx(NULL),
	avVideoCodecCtx(NULL),
	avFrame(NULL),
	audioStreamIndex(AVERROR_STREAM_NOT_FOUND),
	videoStreamIndex(AVERROR_STREAM_NOT_FOUND),
	audioStreamId(NULL),
	videoStreamId(NULL),
	audioPacketQueueHead(0),
	audioPacketQueueCount(0),
	videoPacketQueueHead(0),
	videoPacketQueueCount(0),
	compressedVideo(true)
{
}

FFMPEG::~FFMPEG()
{
}

void FFMPEG::Initialize()
{
	OutputDebugString(L"Initialize\n");
	av_register_all();
	av_log_set_callback(log_callback_help);
	avformat_network_init();
}

void FFMPEG::Close()
{
	// TODO: Stop all callbacks

	av_freep(videoBufferData);

	if (avFrame)
	{
		av_freep(avFrame);
	}

	avcodec_close(avAudioCodecCtx);
	avcodec_close(avVideoCodecCtx);
	avformat_close_input(&avFormatCtx);
}

MediaStreamSource^ FFMPEG::OpenFile(StorageFile^ file, AudioStreamDescriptor^ audioDesc, VideoStreamDescriptor^ videoDesc)
{
	// TODO: Destroy all previously allocated items
	//Close();

	OutputDebugString(L"SetInput\n");

	// Get filename, replace with get stream buffer later on
	std::wstring wStringPath(file->Path->Begin());
	std::string stringPath(wStringPath.begin(), wStringPath.end());
	
	if (avformat_open_input(&avFormatCtx, stringPath.c_str(), NULL, NULL) < 0)
	{
		return nullptr; // Error opening file
	}

	if (avformat_find_stream_info(avFormatCtx, NULL) < 0)
	{
		return nullptr; // Error finding info
	}

	// find the audio stream and its decoder
	audioStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &avAudioCodec, 0);
	if (audioStreamIndex != AVERROR_STREAM_NOT_FOUND) {
		avAudioCodecCtx = avFormatCtx->streams[audioStreamIndex]->codec;
		if (avcodec_open2(avAudioCodecCtx, avAudioCodec, NULL) < 0)
		{
			return nullptr; // Cannot open the audio codec
		}
	}

	// find the video stream and its decoder
	videoStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &avVideoCodec, 0);
	if (videoStreamIndex != AVERROR_STREAM_NOT_FOUND) {
		avVideoCodecCtx = avFormatCtx->streams[videoStreamIndex]->codec;
		if (avcodec_open2(avVideoCodecCtx, avVideoCodec, NULL) < 0)
		{
			return nullptr; // Cannot open the video codec
		}
	}

	if (av_image_alloc(videoBufferData, videoBufferLineSize, avVideoCodecCtx->width, avVideoCodecCtx->height, avVideoCodecCtx->pix_fmt, 1) < 0)
	{
		return nullptr; // Cannot open the video codec
	}

	avFrame = av_frame_alloc();
	if (!avFrame)
	{
		return nullptr; // mark not ready
	}

	VideoEncodingProperties^ videoProperties;
	if (compressedVideo)
	{
		videoProperties = VideoEncodingProperties::CreateH264();
		videoProperties->ProfileId = avVideoCodecCtx->profile;
		videoProperties->Height = avVideoCodecCtx->height;
		videoProperties->Width = avVideoCodecCtx->width;
	}
	else
	{
		videoProperties = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Yv12, avVideoCodecCtx->width, avVideoCodecCtx->height);
	}

	videoProperties->FrameRate->Numerator = avVideoCodecCtx->time_base.den;
	videoProperties->FrameRate->Denominator = avVideoCodecCtx->time_base.num;
	videoProperties->Bitrate = avVideoCodecCtx->bit_rate;
	videoDesc = ref new VideoStreamDescriptor(videoProperties);
	videoStreamId = videoDesc->GetHashCode(); // identify Video stream against Audio stream

	if (avAudioCodecCtx)
	{
		AudioEncodingProperties^ audioProperties = AudioEncodingProperties::CreateAac(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, avAudioCodecCtx->bit_rate);
		audioDesc = ref new AudioStreamDescriptor(audioProperties);
		audioStreamId = audioDesc->GetHashCode(); // identify Audio stream against Video stream
	}

	MediaStreamSource^ mss;
	mss = ref new MediaStreamSource(videoDesc, audioDesc);

	return mss;
}

MediaStreamSample^ FFMPEG::FillSample(IMediaStreamDescriptor^ streamDesc)
{
	if (streamDesc->GetHashCode() == audioStreamId)
	{
		return FillAudioSample();
	}

	if (streamDesc->GetHashCode() == videoStreamId)
	{
		return FillVideoSample();
	}

	return nullptr;
}

MediaStreamSample^ FFMPEG::FillAudioSample()
{
	OutputDebugString(L"FillAudioSample\n");

	MediaStreamSample^ sample;
	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	while (audioPacketQueueCount <= 0)
	{
		if (ReadPacket() < 0)
		{
			OutputDebugString(L"FillAudioSample Reaching End Of File\n");
			break;
		}
	}

	if (audioPacketQueueCount > 0)
	{
		avPacket = PopAudioPacket();
	}

	DataWriter^ dataWriter = ref new DataWriter();
	auto aBuffer = ref new Platform::Array<uint8_t>(avPacket.data, avPacket.size);
	dataWriter->WriteBytes(aBuffer);

	Windows::Foundation::TimeSpan pts = { ULONGLONG(av_q2d(avAudioCodecCtx->time_base) * 10000000 * avPacket.pts) };
	Windows::Foundation::TimeSpan dur = { ULONGLONG(av_q2d(avAudioCodecCtx->time_base) * 10000000 * avPacket.duration) };

	sample = MediaStreamSample::CreateFromBuffer(dataWriter->DetachBuffer(), pts);
	sample->Duration = dur;

	return sample;
}

MediaStreamSample^ FFMPEG::FillVideoSample()
{
	OutputDebugString(L"FillVideoSample\n");

	MediaStreamSample^ sample;
	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	int frameComplete = 0;

	while (!frameComplete)
	{
		while (videoPacketQueueCount <= 0)
		{
			if (ReadPacket() < 0)
			{
				OutputDebugString(L"FillVideoSample Reaching End Of File\n");
				return sample;
			}
		}

		if (videoPacketQueueCount > 0)
		{			
			avPacket = PopVideoPacket();
			if (!compressedVideo)
			{
				if (avcodec_decode_video2(avVideoCodecCtx, avFrame, &frameComplete, &avPacket) < 0)
				{
					return sample;
				}
			}
			else
			{
				frameComplete = true;
			}
		}
	}
	
	DataWriter^ dataWriter = ref new DataWriter();
	if (!compressedVideo)
	{
		av_image_copy(videoBufferData, videoBufferLineSize, (const uint8_t **)(avFrame->data), avFrame->linesize,
			avVideoCodecCtx->pix_fmt, avVideoCodecCtx->width, avVideoCodecCtx->height);

		auto YBuffer = ref new Platform::Array<uint8_t>(videoBufferData[0], videoBufferLineSize[0] * avVideoCodecCtx->height);
		auto UBuffer = ref new Platform::Array<uint8_t>(videoBufferData[1], videoBufferLineSize[1] * avVideoCodecCtx->height / 2);
		auto VBuffer = ref new Platform::Array<uint8_t>(videoBufferData[2], videoBufferLineSize[2] * avVideoCodecCtx->height / 2);
		dataWriter->WriteBytes(YBuffer);
		dataWriter->WriteBytes(VBuffer);
		dataWriter->WriteBytes(UBuffer);
	}
	else
	{
		if (avPacket.flags & AV_PKT_FLAG_KEY)
		{
			GetSPSAndPPSBuffer(dataWriter);
		}
		WriteAnnexBPacket(dataWriter, avPacket);
	}

	Windows::Foundation::TimeSpan pts = { ULONGLONG(av_q2d(avVideoCodecCtx->pkt_timebase) * 10000000 * avPacket.pts) };
	Windows::Foundation::TimeSpan dur = { ULONGLONG(av_q2d(avVideoCodecCtx->pkt_timebase) * 10000000 * avPacket.duration) };

	auto buffer = dataWriter->DetachBuffer();
	sample = MediaStreamSample::CreateFromBuffer(buffer, pts);
	sample->Duration = dur;
	if (!compressedVideo)
	{
		sample->KeyFrame = avFrame->key_frame == 1;
	}
	else
	{
		sample->KeyFrame = (avPacket.flags & AV_PKT_FLAG_KEY) != 0;
	}
	timeOffset.Duration = timeOffset.Duration + avFrame->pkt_duration * 100;

	return sample;
}

int FFMPEG::ReadPacket()
{
	int ret;
	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	ret = av_read_frame(avFormatCtx, &avPacket);
	if (ret < 0)
	{
		return ret;
	}

	if (avPacket.stream_index == audioStreamIndex)
	{
		PushAudioPacket(avPacket);
	}
	else if (avPacket.stream_index == videoStreamIndex)
	{
		PushVideoPacket(avPacket);
	}
	else
	{
		OutputDebugString(L"Unidentified stream type !!!!!\n");
	}

	return ret;
}

void FFMPEG::WriteAnnexBPacket(DataWriter^ dataWriter, AVPacket avPacket)
{
	uint32 index = 0;
	uint32 size;
	
	do
	{
		// Grab the size of the blob
		size = (avPacket.data[index] << 24) + (avPacket.data[index + 1] << 16) + (avPacket.data[index + 2] << 8) + avPacket.data[index + 3];

		dataWriter->WriteByte(0);
		dataWriter->WriteByte(0);
		dataWriter->WriteByte(0);
		dataWriter->WriteByte(1);
		index += 4;

		auto vBuffer = ref new Platform::Array<uint8_t>(&(avPacket.data[index]), size);
		dataWriter->WriteBytes(vBuffer);
		index += size;
	} while (index < avPacket.size);
}

void FFMPEG::GetSPSAndPPSBuffer(DataWriter^ dataWriter)
{
	byte* spsPos = avVideoCodecCtx->extradata + 8;
	byte spsLength = spsPos[-1];
	auto vSPS = ref new Platform::Array<uint8_t>(spsPos, spsLength);
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(1);
	dataWriter->WriteBytes(vSPS);

	byte* ppsPos = avVideoCodecCtx->extradata + 8 + spsLength + 3;
	byte ppsLength = ppsPos[-1];
	auto vPPS = ref new Platform::Array<uint8_t>(ppsPos, ppsLength);
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(1);
	dataWriter->WriteBytes(vPPS);
}

void FFMPEG::PushAudioPacket(AVPacket packet)
{
	OutputDebugString(L" - PushAudio\n");

	if (audioPacketQueueCount < AUDIOPKTBUFFERSZ)
	{
		audioPacketQueue[(audioPacketQueueHead + audioPacketQueueCount) % AUDIOPKTBUFFERSZ] = packet;
		audioPacketQueueCount++;
	}
	else
	{
		OutputDebugString(L" - ### PushAudio too much\n"); // remove later
	}
}

AVPacket FFMPEG::PopAudioPacket()
{
	OutputDebugString(L" - PopAudio\n");

	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	if (audioPacketQueueCount > 0)
	{
		avPacket = audioPacketQueue[audioPacketQueueHead];
		audioPacketQueueHead = (audioPacketQueueHead + 1) % AUDIOPKTBUFFERSZ;
		audioPacketQueueCount--;
	}

	return avPacket;
}

void FFMPEG::PushVideoPacket(AVPacket packet)
{
	OutputDebugString(L" - PushVideo\n");

	if (videoPacketQueueCount < VIDEOPKTBUFFERSZ)
	{
		videoPacketQueue[(videoPacketQueueHead + videoPacketQueueCount) % VIDEOPKTBUFFERSZ] = packet;
		videoPacketQueueCount++;
	}
	else
	{
		OutputDebugString(L" - ### PushVideo too much\n"); // remove later
	}
}

AVPacket FFMPEG::PopVideoPacket()
{
	OutputDebugString(L" - PopVideo\n");

	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	if (videoPacketQueueCount > 0)
	{
		avPacket = videoPacketQueue[videoPacketQueueHead];
		videoPacketQueueHead = (videoPacketQueueHead + 1) % VIDEOPKTBUFFERSZ;
		videoPacketQueueCount--;
	}

	return avPacket;
}