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
#include "shcore.h"

extern "C"
{
#include <libavutil/imgutils.h>
}

using namespace concurrency;
using namespace FFMPEGMediaStreamSource;
using namespace Platform;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::MediaProperties;

// Static function to read file stream and pass data to FFMPEG
static int FileStreamRead(void* ptr, uint8_t* buf, int buf_size)
{
	IStream* pStream = reinterpret_cast<IStream*>(ptr);
	ULONG bytesRead = 0;
	HRESULT hr = pStream->Read(buf, buf_size, &bytesRead);
	if (hr == S_FALSE)
		return AVERROR_EOF;  // Let FFmpeg know that we have reached eof
	if (FAILED(hr))
		return -1;
	return bytesRead;
}

// Static function to seek in file stream
static int64_t FileStreamSeek(void* ptr, int64_t pos, int whence)
{
	IStream* pStream = reinterpret_cast<IStream*>(ptr);

	// Seek:
	LARGE_INTEGER in = { pos };
	ULARGE_INTEGER out = { 0 };
	if (FAILED(pStream->Seek(in, whence, &out)))
		return -1;

	// Return the new position:
	return out.QuadPart;
}

FFMPEG::FFMPEG(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode) :
	avIOCtx(NULL),
	avFormatCtx(NULL),
	avAudioCodecCtx(NULL),
	avVideoCodecCtx(NULL),
	swrCtx(NULL),
	avFrame(NULL),
	audioStreamIndex(AVERROR_STREAM_NOT_FOUND),
	videoStreamIndex(AVERROR_STREAM_NOT_FOUND),
	fileStreamData(NULL),
	fileStreamBuffer(NULL),
	audioPacketQueueHead(0),
	audioPacketQueueCount(0),
	videoPacketQueueHead(0),
	videoPacketQueueCount(0),
	generateUncompressedAudio(false),
	generateUncompressedVideo(false)
{
	av_register_all();
	mss = CreateMediaStreamSource(stream, forceAudioDecode, forceVideoDecode);
}

FFMPEG::~FFMPEG()
{
	Close();
}

MediaStreamSource^ FFMPEG::GetMSS()
{
	return mss;
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
	av_free(avIOCtx);
	swr_free(&swrCtx);
}

void FFMPEG::OnStarting(MediaStreamSource ^sender, MediaStreamSourceStartingEventArgs ^args)
{
	MediaStreamSourceStartingRequest^ request = args->Request;

	// Perform seek operation when MediaStreamSource received seek event from MediaElement
	if (request->StartPosition && request->StartPosition->Value.Duration <= mediaDuration.Duration)
	{
		// Select the first valid stream either from video or audio.
		int streamIndex = audioStreamIndex >= 0 ? audioStreamIndex : videoStreamIndex > 0 ? videoStreamIndex : -1;
		int64_t seekTarget = request->StartPosition->Value.Duration;
		request->SetActualStartPosition(request->StartPosition->Value);

		if (streamIndex >= 0)
		{
			// Convert TimeSpan unit to AV_TIME_BASE
			seekTarget = seekTarget / (av_q2d(avAudioCodecCtx->pkt_timebase) * 10000000);

			if (av_seek_frame(avFormatCtx, streamIndex, seekTarget, 0) < 0)
			{
				OutputDebugString(L" - ### Error while seeking\n");
			}

			// TODO: Change circular queue to PacketList linked list

			// Flush all audio packet in queue and internal buffer
			if (audioStreamIndex >= 0)
			{
				while (audioPacketQueueCount > 0)
				{
					av_free_packet(&PopAudioPacket());
				}
			}

			// Flush all video packet in queue and internal buffer
			if (videoStreamIndex >= 0)
			{
				while (videoPacketQueueCount > 0)
				{
					av_free_packet(&PopVideoPacket());
				}
			}
		}
	}
}

void FFMPEG::OnSampleRequested(Windows::Media::Core::MediaStreamSource ^sender, MediaStreamSourceSampleRequestedEventArgs ^args)
{
	if (args->Request->StreamDescriptor == audioStreamDescriptor)
	{
		args->Request->Sample = FillAudioSample();
	}
	else if (args->Request->StreamDescriptor == videoStreamDescriptor)
	{
		args->Request->Sample = FillVideoSample();
	}
	else
	{
		args->Request->Sample = nullptr;
	}
}

MediaStreamSource^ FFMPEG::CreateMediaStreamSource(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode)
{
	if (!stream)
	{
		return nullptr;
	}

	// Convert asynchronous IRandomAccessStream to synchronous IStream. This API requires shcore.h and shcore.lib
	CreateStreamOverRandomAccessStream(reinterpret_cast<IUnknown*>(stream), IID_PPV_ARGS(&fileStreamData));

	// Setup FFMPEG custom IO to access file as stream. This is necessary when accessing any file outside of app installation directory and appdata folder.
	fileStreamBuffer = (unsigned char*)av_malloc(FILESTREAMBUFFERSZ);
	avIOCtx = avio_alloc_context(fileStreamBuffer, FILESTREAMBUFFERSZ, 0, fileStreamData, FileStreamRead, 0, FileStreamSeek);

	avFormatCtx = avformat_alloc_context();
	avFormatCtx->pb = avIOCtx;
	avFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

	if (avformat_open_input(&avFormatCtx, "", NULL, NULL) < 0)
	{
		return nullptr; // Error opening file
	}

	if (avformat_find_stream_info(avFormatCtx, NULL) < 0)
	{
		return nullptr; // Error finding info
	}

	// find the audio stream and its decoder
	AVCodec* avAudioCodec = NULL;
	audioStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &avAudioCodec, 0);
	if (audioStreamIndex != AVERROR_STREAM_NOT_FOUND && avAudioCodec) {
		avAudioCodecCtx = avFormatCtx->streams[audioStreamIndex]->codec;
		if (avcodec_open2(avAudioCodecCtx, avAudioCodec, NULL) < 0)
		{
			avAudioCodecCtx = NULL;
			return nullptr; // Cannot open the audio codec
		}

		// Detect audio format and create audio stream descriptor accordingly
		CreateAudioStreamDescriptor(forceAudioDecode);
	}

	// find the video stream and its decoder
	AVCodec* avVideoCodec = NULL;
	videoStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &avVideoCodec, 0);
	if (videoStreamIndex != AVERROR_STREAM_NOT_FOUND && avVideoCodec) {
		avVideoCodecCtx = avFormatCtx->streams[videoStreamIndex]->codec;
		if (avcodec_open2(avVideoCodecCtx, avVideoCodec, NULL) < 0)
		{
			avVideoCodecCtx = NULL;
			return nullptr; // Cannot open the video codec
		}

		if (av_image_alloc(videoBufferData, videoBufferLineSize, avVideoCodecCtx->width, avVideoCodecCtx->height, avVideoCodecCtx->pix_fmt, 1) < 0)
		{
			return nullptr; // Cannot open the video codec
		}

		// Detect video format and create video stream descriptor accordingly
		CreateVideoStreamDescriptor(forceVideoDecode);
	}

	avFrame = av_frame_alloc();
	if (!avFrame)
	{
		return nullptr; // mark not ready
	}

	// Convert media duration from AV_TIME_BASE to TimeSpan unit
	mediaDuration = { ULONGLONG(avFormatCtx->duration * 10000000 / double(AV_TIME_BASE)) };

	MediaStreamSource^ mss;

	if (audioStreamDescriptor)
	{
		if (videoStreamDescriptor)
		{
			mss = ref new MediaStreamSource(videoStreamDescriptor, audioStreamDescriptor);
		}
		else
		{
			mss = ref new MediaStreamSource(audioStreamDescriptor);
		}
	}
	else if (videoStreamDescriptor)
	{
		mss = ref new MediaStreamSource(videoStreamDescriptor);
	}

	if (mss)
	{
		mss->Duration = mediaDuration;
		mss->CanSeek = true;
		mss->Starting += ref new TypedEventHandler<MediaStreamSource ^, MediaStreamSourceStartingEventArgs ^>(this, &FFMPEG::OnStarting);
		mss->SampleRequested += ref new TypedEventHandler<MediaStreamSource ^, MediaStreamSourceSampleRequestedEventArgs ^>(this, &FFMPEG::OnSampleRequested);
	}

	return mss;
}

void FFMPEG::CreateAudioStreamDescriptor(bool forceAudioDecode)
{
	if (avAudioCodecCtx->codec_id == AV_CODEC_ID_AAC && !forceAudioDecode)
	{
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreateAac(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, avAudioCodecCtx->bit_rate));
	}
	else if (avAudioCodecCtx->codec_id == AV_CODEC_ID_MP3 && !forceAudioDecode)
	{
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreateMp3(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, avAudioCodecCtx->bit_rate));
	}
	else if (avAudioCodecCtx->sample_fmt == AV_SAMPLE_FMT_FLTP)
	{
		unsigned int bitsPerSample = avAudioCodecCtx->bits_per_coded_sample ? avAudioCodecCtx->bits_per_coded_sample : 16;
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreatePcm(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, bitsPerSample));
		generateUncompressedAudio = true;

		// Set up resampler to convert AV_SAMPLE_FMT_FLTP format to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element
		swrCtx = swr_alloc_set_opts(
			NULL,
			avAudioCodecCtx->channel_layout,
			AV_SAMPLE_FMT_S16,
			avAudioCodecCtx->sample_rate,
			avAudioCodecCtx->channel_layout,
			avAudioCodecCtx->sample_fmt,
			avAudioCodecCtx->sample_rate,
			0,
			NULL);

		swr_init(swrCtx);
	}
	else
	{
		avAudioCodecCtx->request_sample_fmt = AV_SAMPLE_FMT_S16;
		unsigned int bitsPerSample = avAudioCodecCtx->bits_per_coded_sample ? avAudioCodecCtx->bits_per_coded_sample : 16;
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreatePcm(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, bitsPerSample));
		generateUncompressedAudio = true;
	}
}

void FFMPEG::CreateVideoStreamDescriptor(bool forceVideoDecode)
{
	VideoEncodingProperties^ videoProperties;

	if (avVideoCodecCtx->codec_id == AV_CODEC_ID_H264 && !forceVideoDecode)
	{
		videoProperties = VideoEncodingProperties::CreateH264();
		videoProperties->ProfileId = avVideoCodecCtx->profile;
		videoProperties->Height = avVideoCodecCtx->height;
		videoProperties->Width = avVideoCodecCtx->width;
	}
	else
	{
		videoProperties = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Yv12, avVideoCodecCtx->width, avVideoCodecCtx->height);
		generateUncompressedVideo = true;
	}

	videoProperties->FrameRate->Numerator = avVideoCodecCtx->time_base.den;
	videoProperties->FrameRate->Denominator = avVideoCodecCtx->time_base.num;
	videoProperties->Bitrate = avVideoCodecCtx->bit_rate;
	videoStreamDescriptor = ref new VideoStreamDescriptor(videoProperties);
}

MediaStreamSample^ FFMPEG::FillAudioSample()
{
	OutputDebugString(L"FillAudioSample\n");

	MediaStreamSample^ sample;
	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	int frameComplete = 0;

	while (!frameComplete)
	{
		// Continue reading until there is an audio packet in the stream. All video packet encountered will be queued.
		while (audioPacketQueueCount <= 0)
		{
			if (ReadPacket() < 0)
			{
				OutputDebugString(L"FillAudioSample Reaching End Of File\n");
				return sample;
			}
		}

		if (audioPacketQueueCount > 0)
		{
			avPacket = PopAudioPacket();
			if (generateUncompressedAudio)
			{
				int packetSize = avPacket.size;
				int decodedBytes = avcodec_decode_audio4(avAudioCodecCtx, avFrame, &frameComplete, &avPacket);

				if (decodedBytes < 0)
				{
					return sample;
				}

				if (decodedBytes != packetSize) {
					// Trap this condition to be handled
					throw - 1;
				}
			}
			else
			{
				frameComplete = true;
			}
		}
	}

	uint8_t *resampledData;

	DataWriter^ dataWriter = ref new DataWriter();
	if (generateUncompressedAudio && swrCtx)
	{
		// Resample frame to AV_SAMPLE_FMT_S16 format
		unsigned int aBufferSize = av_samples_alloc(&resampledData, NULL, avFrame->channels, avFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
		int resampledDataSize = swr_convert(swrCtx, &resampledData, aBufferSize, (const uint8_t **)avFrame->extended_data, avFrame->nb_samples);
		auto aBuffer = ref new Platform::Array<uint8_t>(resampledData, aBufferSize);
		dataWriter->WriteBytes(aBuffer);
		av_freep(&resampledData);
		av_frame_unref(avFrame);
	}
	else if (generateUncompressedAudio)
	{
		unsigned int aBufferSize = avFrame->linesize[0];
		auto aBuffer = ref new Platform::Array<uint8_t>(*avFrame->extended_data, aBufferSize);
		dataWriter->WriteBytes(aBuffer);
		av_frame_unref(avFrame);
	}
	else
	{
		auto aBuffer = ref new Platform::Array<uint8_t>(avPacket.data, avPacket.size);
		dataWriter->WriteBytes(aBuffer);
	}

	Windows::Foundation::TimeSpan pts = { ULONGLONG(av_q2d(avAudioCodecCtx->pkt_timebase) * 10000000 * avPacket.pts) };
	Windows::Foundation::TimeSpan dur = { ULONGLONG(av_q2d(avAudioCodecCtx->pkt_timebase) * 10000000 * avPacket.duration) };

	sample = MediaStreamSample::CreateFromBuffer(dataWriter->DetachBuffer(), pts);
	sample->Duration = dur;

	av_free_packet(&avPacket);

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
		// Continue reading until there is a video packet in the stream. All audio packet encountered will be queued.
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
			if (generateUncompressedVideo)
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
	if (generateUncompressedVideo)
	{
		av_image_copy(videoBufferData, videoBufferLineSize, (const uint8_t **)(avFrame->data), avFrame->linesize,
			avVideoCodecCtx->pix_fmt, avVideoCodecCtx->width, avVideoCodecCtx->height);

		auto YBuffer = ref new Platform::Array<uint8_t>(videoBufferData[0], videoBufferLineSize[0] * avVideoCodecCtx->height);
		auto UBuffer = ref new Platform::Array<uint8_t>(videoBufferData[1], videoBufferLineSize[1] * avVideoCodecCtx->height / 2);
		auto VBuffer = ref new Platform::Array<uint8_t>(videoBufferData[2], videoBufferLineSize[2] * avVideoCodecCtx->height / 2);
		dataWriter->WriteBytes(YBuffer);
		dataWriter->WriteBytes(VBuffer);
		dataWriter->WriteBytes(UBuffer);
		av_frame_unref(avFrame);
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
	if (generateUncompressedVideo)
	{
		sample->KeyFrame = avFrame->key_frame == 1;
	}
	else
	{
		sample->KeyFrame = (avPacket.flags & AV_PKT_FLAG_KEY) != 0;
	}

	av_free_packet(&avPacket);

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