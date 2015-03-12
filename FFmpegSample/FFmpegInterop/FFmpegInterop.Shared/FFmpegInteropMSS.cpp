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
#include "FFmpegInteropMSS.h"
#include "shcore.h"

extern "C"
{
#include <libavutil/imgutils.h>
}

using namespace concurrency;
using namespace FFmpegInterop;
using namespace Platform;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::MediaProperties;

// Size of the buffer when reading a stream
const int FILESTREAMBUFFERSZ = 16384;

// Static functions passed to FFmpeg for stream interop
static int FileStreamRead(void* ptr, uint8_t* buf, int bufSize);
static int64_t FileStreamSeek(void* ptr, int64_t pos, int whence);

// Initialize an FFmpeg
FFmpegInteropMSS::FFmpegInteropMSS(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode) :
avIOCtx(NULL),
avFormatCtx(NULL),
avAudioCodecCtx(NULL),
avVideoCodecCtx(NULL),
swrCtx(NULL),
swsCtx(NULL),
avFrame(NULL),
audioStreamIndex(AVERROR_STREAM_NOT_FOUND),
videoStreamIndex(AVERROR_STREAM_NOT_FOUND),
fileStreamData(NULL),
fileStreamBuffer(NULL),
generateUncompressedAudio(false),
generateUncompressedVideo(false)
{
	av_register_all();
	mss = CreateMediaStreamSource(stream, forceAudioDecode, forceVideoDecode);
}

FFmpegInteropMSS::~FFmpegInteropMSS()
{
	if (mss)
	{
		mss->Starting -= startingRequestedToken;
		mss->SampleRequested -= sampleRequestedToken;
		mss = nullptr;
	}

	if (videoBufferData)
	{
		av_freep(videoBufferData);
	}

	if (avFrame)
	{
		av_freep(avFrame);
	}

	if (fileStreamBuffer)
	{
		av_free(fileStreamBuffer);
	}

	swr_free(&swrCtx);
	sws_freeContext(swsCtx);
	avcodec_close(avAudioCodecCtx);
	avcodec_close(avVideoCodecCtx);
	avformat_close_input(&avFormatCtx);
	av_free(avIOCtx);
}

MediaStreamSource^ FFmpegInteropMSS::GetMediaStreamSource()
{
	return mss;
}

MediaStreamSource^ FFmpegInteropMSS::CreateMediaStreamSource(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode)
{
	if (!stream)
	{
		return nullptr;
	}

	// Convert asynchronous IRandomAccessStream to synchronous IStream. This API requires shcore.h and shcore.lib
	CreateStreamOverRandomAccessStream(reinterpret_cast<IUnknown*>(stream), IID_PPV_ARGS(&fileStreamData));

	// Setup FFmpeg custom IO to access file as stream. This is necessary when accessing any file outside of app installation directory and appdata folder.
	// Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
	fileStreamBuffer = (unsigned char*)av_malloc(FILESTREAMBUFFERSZ);
	avIOCtx = avio_alloc_context(fileStreamBuffer, FILESTREAMBUFFERSZ, 0, fileStreamData, FileStreamRead, 0, FileStreamSeek);

	avFormatCtx = avformat_alloc_context();
	avFormatCtx->pb = avIOCtx;
	avFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

	// Open media file using custom IO setup above instead of using file name. Opening a file using file name will invoke fopen C API call that only have
	// access within the app installation directory and appdata folder. Custom IO allows access to file selected using FilePicker dialog.
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
		startingRequestedToken = mss->Starting += ref new TypedEventHandler<MediaStreamSource ^, MediaStreamSourceStartingEventArgs ^>(this, &FFmpegInteropMSS::OnStarting);
		sampleRequestedToken = mss->SampleRequested += ref new TypedEventHandler<MediaStreamSource ^, MediaStreamSourceSampleRequestedEventArgs ^>(this, &FFmpegInteropMSS::OnSampleRequested);
	}

	return mss;
}

void FFmpegInteropMSS::CreateAudioStreamDescriptor(bool forceAudioDecode)
{
	if (avAudioCodecCtx->codec_id == AV_CODEC_ID_AAC && !forceAudioDecode)
	{
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreateAac(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, avAudioCodecCtx->bit_rate));
	}
	else if (avAudioCodecCtx->codec_id == AV_CODEC_ID_MP3 && !forceAudioDecode)
	{
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreateMp3(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, avAudioCodecCtx->bit_rate));
	}
	else
	{
		// Set default 16 bits when bits per sample value is unknown (0)
		unsigned int bitsPerSample = avAudioCodecCtx->bits_per_coded_sample ? avAudioCodecCtx->bits_per_coded_sample : 16;
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreatePcm(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, bitsPerSample));
		generateUncompressedAudio = true;

		// Set default channel layout when the value is unknown (0)
		int64 inChannelLayout = avAudioCodecCtx->channel_layout ? avAudioCodecCtx->channel_layout : av_get_default_channel_layout(avAudioCodecCtx->channels);
		int64 outChannelLayout = av_get_default_channel_layout(avAudioCodecCtx->channels);

		// Set up resampler to convert any PCM format (e.g. AV_SAMPLE_FMT_FLTP) to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element.
		// Additional logic can be added to avoid resampling PCM data that is already in AV_SAMPLE_FMT_S16_PCM.
		swrCtx = swr_alloc_set_opts(
			NULL,
			outChannelLayout,
			AV_SAMPLE_FMT_S16,
			avAudioCodecCtx->sample_rate,
			inChannelLayout,
			avAudioCodecCtx->sample_fmt,
			avAudioCodecCtx->sample_rate,
			0,
			NULL);

		swr_init(swrCtx);
	}
}

void FFmpegInteropMSS::CreateVideoStreamDescriptor(bool forceVideoDecode)
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
		videoProperties = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Nv12, avVideoCodecCtx->width, avVideoCodecCtx->height);
		generateUncompressedVideo = true;

		// Setup software scaler to convert any decoder pixel format (e.g. YUV420P) to NV12 that is supported in Windows & Windows Phone MediaElement
		swsCtx = sws_getContext(
			avVideoCodecCtx->width,
			avVideoCodecCtx->height,
			avVideoCodecCtx->pix_fmt,
			avVideoCodecCtx->width,
			avVideoCodecCtx->height,
			AV_PIX_FMT_NV12,
			SWS_BICUBIC,
			NULL,
			NULL,
			NULL);

		if (!swsCtx || av_image_alloc(videoBufferData, videoBufferLineSize, avVideoCodecCtx->width, avVideoCodecCtx->height, AV_PIX_FMT_NV12, 1) < 0)
		{
			return; // Don't generate video stream descriptor if scaler or video buffer cannot be initialized
		}
	}

	videoProperties->FrameRate->Numerator = avVideoCodecCtx->framerate.num;
	videoProperties->FrameRate->Denominator = avVideoCodecCtx->framerate.den;
	videoProperties->Bitrate = avVideoCodecCtx->bit_rate;
	videoStreamDescriptor = ref new VideoStreamDescriptor(videoProperties);
}

void FFmpegInteropMSS::OnStarting(MediaStreamSource ^sender, MediaStreamSourceStartingEventArgs ^args)
{
	MediaStreamSourceStartingRequest^ request = args->Request;

	// Perform seek operation when MediaStreamSource received seek event from MediaElement
	if (request->StartPosition && request->StartPosition->Value.Duration <= mediaDuration.Duration)
	{
		// Select the first valid stream either from video or audio
		int streamIndex = videoStreamIndex >= 0 ? videoStreamIndex : audioStreamIndex >= 0 ? audioStreamIndex : -1;

		if (streamIndex >= 0)
		{
			// Convert TimeSpan unit to AV_TIME_BASE
			int64_t seekTarget = static_cast<int64_t>(request->StartPosition->Value.Duration / (av_q2d(avFormatCtx->streams[streamIndex]->time_base) * 10000000));

			if (av_seek_frame(avFormatCtx, streamIndex, seekTarget, 0) < 0)
			{
				DebugMessage(L" - ### Error while seeking\n");
			}
			else
			{
				// Add deferral

				// Flush all audio packet in queue and internal buffer
				if (audioStreamIndex >= 0)
				{
					while (!audioPacketQueue.empty())
					{
						av_free_packet(&PopAudioPacket());
					}
					avcodec_flush_buffers(avAudioCodecCtx);
				}

				// Flush all video packet in queue and internal buffer
				if (videoStreamIndex >= 0)
				{
					while (!videoPacketQueue.empty())
					{
						av_free_packet(&PopVideoPacket());
					}
					avcodec_flush_buffers(avVideoCodecCtx);
				}
			}
		}

		request->SetActualStartPosition(request->StartPosition->Value);
	}
}

void FFmpegInteropMSS::OnSampleRequested(Windows::Media::Core::MediaStreamSource ^sender, MediaStreamSourceSampleRequestedEventArgs ^args)
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

MediaStreamSample^ FFmpegInteropMSS::FillAudioSample()
{
	DebugMessage(L"FillAudioSample\n");

	MediaStreamSample^ sample;
	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;
	DataWriter^ dataWriter = ref new DataWriter();

	int frameComplete = 0;

	while (!frameComplete)
	{
		// Continue reading until there is an audio packet in the stream. All video packet encountered will be queued.
		while (audioPacketQueue.empty())
		{
			if (ReadPacket() < 0)
			{
				DebugMessage(L"FillAudioSample Reaching End Of File\n");
				return sample;
			}
		}

		if (!audioPacketQueue.empty())
		{
			// Pick audio packet from the queue one at a time
			avPacket = PopAudioPacket();

			if (generateUncompressedAudio)
			{
				// Each audio packet may contain multiple frames which requires calling avcodec_decode_audio4 for each frame. Loop through the entire packet data
				while (avPacket.size > 0)
				{
					frameComplete = 0;
					int decodedBytes = avcodec_decode_audio4(avAudioCodecCtx, avFrame, &frameComplete, &avPacket);

					if (decodedBytes < 0)
					{
						DebugMessage(L"Fail To Decode!\n");
						break; // Skip broken frame
					}

					if (frameComplete)
					{
						// Resample uncompressed frame to AV_SAMPLE_FMT_S16 PCM format that is expected by Media Element
						uint8_t *resampledData;
						unsigned int aBufferSize = av_samples_alloc(&resampledData, NULL, avFrame->channels, avFrame->nb_samples, AV_SAMPLE_FMT_S16, 0);
						int resampledDataSize = swr_convert(swrCtx, &resampledData, aBufferSize, (const uint8_t **)avFrame->extended_data, avFrame->nb_samples);
						auto aBuffer = ref new Platform::Array<uint8_t>(resampledData, aBufferSize);
						dataWriter->WriteBytes(aBuffer);
						av_freep(&resampledData);
						av_frame_unref(avFrame);
					}

					// Advance to the next frame data that have not been decoded if any
					avPacket.size -= decodedBytes;
					avPacket.data += decodedBytes;
				}
			}
			else
			{
				// Pass the compressed audio packet as is without decoding to Media Element
				auto aBuffer = ref new Platform::Array<uint8_t>(avPacket.data, avPacket.size);
				dataWriter->WriteBytes(aBuffer);
				frameComplete = true;
			}
		}
	}

	Windows::Foundation::TimeSpan pts = { ULONGLONG(av_q2d(avFormatCtx->streams[audioStreamIndex]->time_base) * 10000000 * avPacket.pts) };
	Windows::Foundation::TimeSpan dur = { ULONGLONG(av_q2d(avFormatCtx->streams[audioStreamIndex]->time_base) * 10000000 * avPacket.duration) };

	sample = MediaStreamSample::CreateFromBuffer(dataWriter->DetachBuffer(), pts);
	sample->Duration = dur;

	av_free_packet(&avPacket);

	return sample;
}

MediaStreamSample^ FFmpegInteropMSS::FillVideoSample()
{
	DebugMessage(L"FillVideoSample\n");

	MediaStreamSample^ sample;
	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	int frameComplete = 0;

	while (!frameComplete)
	{
		// Continue reading until there is a video packet in the stream. All audio packet encountered will be queued.
		while (videoPacketQueue.empty())
		{
			if (ReadPacket() < 0)
			{
				DebugMessage(L"FillVideoSample Reaching End Of File\n");
				return sample;
			}
		}

		if (!videoPacketQueue.empty())
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
		// Convert decoded video pixel format to NV12 using FFmpeg software scaler
		sws_scale(swsCtx, (const uint8_t **)(avFrame->data), avFrame->linesize, 0, avVideoCodecCtx->height, videoBufferData, videoBufferLineSize);

		auto YBuffer = ref new Platform::Array<uint8_t>(videoBufferData[0], videoBufferLineSize[0] * avVideoCodecCtx->height);
		auto UVBuffer = ref new Platform::Array<uint8_t>(videoBufferData[1], videoBufferLineSize[1] * avVideoCodecCtx->height / 2);
		dataWriter->WriteBytes(YBuffer);
		dataWriter->WriteBytes(UVBuffer);
	}
	else
	{
	// This should only be H.264
		if (avPacket.flags & AV_PKT_FLAG_KEY)
		{
			GetSPSAndPPSBuffer(dataWriter);
		}
		WriteAnnexBPacket(dataWriter, avPacket);
	}

	// Use decoding timestamp if presentation timestamp is not valid
	if (avPacket.pts == AV_NOPTS_VALUE && avPacket.dts != AV_NOPTS_VALUE)
	{
		avPacket.pts = avPacket.dts;
	}

	Windows::Foundation::TimeSpan pts = { ULONGLONG(av_q2d(avFormatCtx->streams[videoStreamIndex]->time_base) * 10000000 * avPacket.pts) };
	Windows::Foundation::TimeSpan dur = { ULONGLONG(av_q2d(avFormatCtx->streams[videoStreamIndex]->time_base) * 10000000 * avPacket.duration) };

	auto buffer = dataWriter->DetachBuffer();
	sample = MediaStreamSample::CreateFromBuffer(buffer, pts);
	sample->Duration = dur;
	if (generateUncompressedVideo)
	{
		sample->KeyFrame = avFrame->key_frame == 1;
		av_frame_unref(avFrame);
	}
	else
	{
		sample->KeyFrame = (avPacket.flags & AV_PKT_FLAG_KEY) != 0;
	}

	av_free_packet(&avPacket);

	return sample;
}

int FFmpegInteropMSS::ReadPacket()
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
		DebugMessage(L"Ignoring unused stream\n");
	}

	return ret;
}

void FFmpegInteropMSS::GetSPSAndPPSBuffer(DataWriter^ dataWriter)
{
	byte* spsPos = avVideoCodecCtx->extradata + 8;
	byte spsLength = spsPos[-1];
	auto vSPS = ref new Platform::Array<uint8_t>(spsPos, spsLength);

	// Write the NAL unit for the SPS
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(1);

	// Write the SPS
	dataWriter->WriteBytes(vSPS);

	byte* ppsPos = avVideoCodecCtx->extradata + 8 + spsLength + 3;
	byte ppsLength = ppsPos[-1];
	auto vPPS = ref new Platform::Array<uint8_t>(ppsPos, ppsLength);

	// Write the NAL unit for the PPS
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(0);
	dataWriter->WriteByte(1);

	// Write the PPS
	dataWriter->WriteBytes(vPPS);
}

void FFmpegInteropMSS::WriteAnnexBPacket(DataWriter^ dataWriter, AVPacket avPacket)
{
	int32 index = 0;
	uint32 size;

	do
	{
		// Grab the size of the blob
		size = (avPacket.data[index] << 24) + (avPacket.data[index + 1] << 16) + (avPacket.data[index + 2] << 8) + avPacket.data[index + 3];

		// Write the NAL unit to the stream
		dataWriter->WriteByte(0);
		dataWriter->WriteByte(0);
		dataWriter->WriteByte(1);
		index += 4;

		// Write the rest of the packet to the stream
		auto vBuffer = ref new Platform::Array<uint8_t>(&(avPacket.data[index]), size);
		dataWriter->WriteBytes(vBuffer);
		index += size;
	} while (index < avPacket.size);
}

void FFmpegInteropMSS::PushAudioPacket(AVPacket packet)
{
	DebugMessage(L" - PushAudio\n");

	audioPacketQueue.push(packet);
}

AVPacket FFmpegInteropMSS::PopAudioPacket()
{
	DebugMessage(L" - PopAudio\n");

	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	if (!audioPacketQueue.empty())
	{
		avPacket = audioPacketQueue.front();
		audioPacketQueue.pop();
	}

	return avPacket;
}

void FFmpegInteropMSS::PushVideoPacket(AVPacket packet)
{
	DebugMessage(L" - PushVideo\n");

	videoPacketQueue.push(packet);
}

AVPacket FFmpegInteropMSS::PopVideoPacket()
{
	DebugMessage(L" - PopVideo\n");

	AVPacket avPacket;
	av_init_packet(&avPacket);
	avPacket.data = NULL;
	avPacket.size = 0;

	if (!videoPacketQueue.empty())
	{
		avPacket = videoPacketQueue.front();
		videoPacketQueue.pop();
	}

	return avPacket;
}

// Static function to read file stream and pass data to FFmpeg. Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
static int FileStreamRead(void* ptr, uint8_t* buf, int bufSize)
{
	IStream* pStream = reinterpret_cast<IStream*>(ptr);
	ULONG bytesRead = 0;
	HRESULT hr = pStream->Read(buf, bufSize, &bytesRead);

	if (FAILED(hr))
	{
		return -1;
	}

	// If we succeed but don't have any bytes, assume end of file
	if (bytesRead == 0)
	{
		return AVERROR_EOF;  // Let FFmpeg know that we have reached eof
	}

	return bytesRead;
}

// Static function to seek in file stream. Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
static int64_t FileStreamSeek(void* ptr, int64_t pos, int whence)
{
	IStream* pStream = reinterpret_cast<IStream*>(ptr);
	LARGE_INTEGER in;
	in.QuadPart = pos;
	ULARGE_INTEGER out = { 0 };

	if (FAILED(pStream->Seek(in, whence, &out)))
	{
		return -1;
	}

	return out.QuadPart; // Return the new position:
}
