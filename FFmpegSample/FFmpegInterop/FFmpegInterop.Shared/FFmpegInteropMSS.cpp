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
#include "MediaSampleProvider.h"
#include "H264SampleProvider.h"
#include "UncompressedAudioSampleProvider.h"
#include "UncompressedVideoSampleProvider.h"
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

// Initialize an FFmpegInteropObject
FFmpegInteropMSS::FFmpegInteropMSS()
	: avIOCtx(nullptr)
	, avFormatCtx(nullptr)
	, avAudioCodecCtx(nullptr)
	, avVideoCodecCtx(nullptr)
	, audioStreamIndex(AVERROR_STREAM_NOT_FOUND)
	, videoStreamIndex(AVERROR_STREAM_NOT_FOUND)
	, fileStreamData(nullptr)
	, fileStreamBuffer(nullptr)
{
	av_register_all();
}

FFmpegInteropMSS::~FFmpegInteropMSS()
{
	if (mss)
	{
		mss->Starting -= startingRequestedToken;
		mss->SampleRequested -= sampleRequestedToken;
		mss = nullptr;
	}

	// Clear our data
	audioSampleProvider = nullptr;
	videoSampleProvider = nullptr;

	// 
	if (m_pReader != nullptr)
	{
		m_pReader->SetAudioStream(AVERROR_STREAM_NOT_FOUND, nullptr);
		m_pReader->SetVideoStream(AVERROR_STREAM_NOT_FOUND, nullptr);

		m_pReader = nullptr;
	}

	avcodec_close(avAudioCodecCtx);
	avcodec_close(avVideoCodecCtx);
	avformat_close_input(&avFormatCtx);
	av_free(avIOCtx);

}

FFmpegInteropMSS^ FFmpegInteropMSS::CreateFFmpegInteropMSSFromStream(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode)
{
	auto interopMSS = ref new FFmpegInteropMSS();
	if (FAILED(interopMSS->CreateMediaStreamSource(stream, forceAudioDecode, forceVideoDecode)))
	{
		// We failed to initialize, clear the variable to return failure
		interopMSS = nullptr;
	}

	return interopMSS;
}

FFmpegInteropMSS^ FFmpegInteropMSS::CreateFFmpegInteropMSSFromUri(String^ uri, bool forceAudioDecode, bool forceVideoDecode)
{
	auto interopMSS = ref new FFmpegInteropMSS();
	if (FAILED(interopMSS->CreateMediaStreamSource(uri, forceAudioDecode, forceVideoDecode)))
	{
		// We failed to initialize, clear the variable to return failure
		interopMSS = nullptr;
	}

	return interopMSS;
}

MediaStreamSource^ FFmpegInteropMSS::GetMediaStreamSource()
{
	return mss;
}

HRESULT FFmpegInteropMSS::CreateMediaStreamSource(String^ uri, bool forceAudioDecode, bool forceVideoDecode)
{
	HRESULT hr = S_OK;
	const char* charStr = nullptr;
	if (!uri)
	{
		hr = E_INVALIDARG;
	}

	if (SUCCEEDED(hr))
	{
		std::wstring uriW(uri->Begin());
		std::string uriA(uriW.begin(), uriW.end());
		charStr = uriA.c_str();

		if (avio_open(&avIOCtx, charStr, AVIO_FLAG_READ) < 0)
		{
			hr = E_FAIL;
		}

	}
	if (SUCCEEDED(hr))
	{
		hr = InitFFmpegContext(forceAudioDecode, forceVideoDecode);
	}

	return hr;
}

HRESULT FFmpegInteropMSS::CreateMediaStreamSource(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode)
{
	HRESULT hr = S_OK;
	if (!stream)
	{
		hr = E_INVALIDARG;
	}

	if (SUCCEEDED(hr))
	{
		// Convert asynchronous IRandomAccessStream to synchronous IStream. This API requires shcore.h and shcore.lib
		hr = CreateStreamOverRandomAccessStream(reinterpret_cast<IUnknown*>(stream), IID_PPV_ARGS(&fileStreamData));
	}

	if (SUCCEEDED(hr))
	{
		// Setup FFmpeg custom IO to access file as stream. This is necessary when accessing any file outside of app installation directory and appdata folder.
		// Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
		fileStreamBuffer = (unsigned char*)av_malloc(FILESTREAMBUFFERSZ);
		if (fileStreamBuffer == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		avIOCtx = avio_alloc_context(fileStreamBuffer, FILESTREAMBUFFERSZ, 0, fileStreamData, FileStreamRead, 0, FileStreamSeek);
		if (avIOCtx == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = InitFFmpegContext(forceAudioDecode, forceVideoDecode);
	}

	return hr;
}

HRESULT FFmpegInteropMSS::InitFFmpegContext(bool forceAudioDecode, bool forceVideoDecode)
{
	HRESULT hr = S_OK;
	avFormatCtx = avformat_alloc_context();
	if (avFormatCtx == nullptr)
	{
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		avFormatCtx->pb = avIOCtx;
		avFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

		// Open media file using custom IO setup above instead of using file name. Opening a file using file name will invoke fopen C API call that only have
		// access within the app installation directory and appdata folder. Custom IO allows access to file selected using FilePicker dialog.
		if (avformat_open_input(&avFormatCtx, "", NULL, NULL) < 0)
		{
			hr = E_FAIL; // Error opening file
		}
	}

	if (SUCCEEDED(hr))
	{
		if (avformat_find_stream_info(avFormatCtx, NULL) < 0)
		{
			hr = E_FAIL; // Error finding info
		}
	}

	if (SUCCEEDED(hr))
	{
		m_pReader = ref new FFmpegReader(avFormatCtx);
		if (m_pReader == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		// find the audio stream and its decoder
		AVCodec* avAudioCodec = nullptr;
		audioStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &avAudioCodec, 0);
		if (audioStreamIndex != AVERROR_STREAM_NOT_FOUND && avAudioCodec) {
			avAudioCodecCtx = avFormatCtx->streams[audioStreamIndex]->codec;
			if (avcodec_open2(avAudioCodecCtx, avAudioCodec, NULL) < 0)
			{
				avAudioCodecCtx = nullptr;
				hr = E_FAIL;
			}
			else
			{
				// Detect audio format and create audio stream descriptor accordingly
				hr = CreateAudioStreamDescriptor(forceAudioDecode);
				if (SUCCEEDED(hr))
				{
					hr = audioSampleProvider->AllocateResources();
					if (SUCCEEDED(hr))
					{
						m_pReader->SetAudioStream(audioStreamIndex, audioSampleProvider);
					}
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		// find the video stream and its decoder
		AVCodec* avVideoCodec = nullptr;
		videoStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &avVideoCodec, 0);
		if (videoStreamIndex != AVERROR_STREAM_NOT_FOUND && avVideoCodec) {
			avVideoCodecCtx = avFormatCtx->streams[videoStreamIndex]->codec;
			if (avcodec_open2(avVideoCodecCtx, avVideoCodec, NULL) < 0)
			{
				avVideoCodecCtx = nullptr;
				hr = E_FAIL; // Cannot open the video codec
			}
			else
			{

				// Detect video format and create video stream descriptor accordingly
				hr = CreateVideoStreamDescriptor(forceVideoDecode);
				if (SUCCEEDED(hr))
				{
					hr = videoSampleProvider->AllocateResources();
					if (SUCCEEDED(hr))
					{
						m_pReader->SetVideoStream(videoStreamIndex, videoSampleProvider);
					}
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		// Convert media duration from AV_TIME_BASE to TimeSpan unit
		mediaDuration = { ULONGLONG(avFormatCtx->duration * 10000000 / double(AV_TIME_BASE)) };

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
		else
		{
			hr = E_OUTOFMEMORY;
		}
	}

	return hr;
}

HRESULT FFmpegInteropMSS::CreateAudioStreamDescriptor(bool forceAudioDecode)
{
	if (avAudioCodecCtx->codec_id == AV_CODEC_ID_AAC && !forceAudioDecode)
	{
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreateAac(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, avAudioCodecCtx->bit_rate));
		audioSampleProvider = ref new MediaSampleProvider(m_pReader, avFormatCtx, avAudioCodecCtx);
	}
	else if (avAudioCodecCtx->codec_id == AV_CODEC_ID_MP3 && !forceAudioDecode)
	{
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreateMp3(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, avAudioCodecCtx->bit_rate));
		audioSampleProvider = ref new MediaSampleProvider(m_pReader, avFormatCtx, avAudioCodecCtx);
	}
	else
	{
		// Set default 16 bits when bits per sample value is unknown (0)
		unsigned int bitsPerSample = avAudioCodecCtx->bits_per_coded_sample ? avAudioCodecCtx->bits_per_coded_sample : 16;
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreatePcm(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, bitsPerSample));
		audioSampleProvider = ref new UncompressedAudioSampleProvider(m_pReader, avFormatCtx, avAudioCodecCtx);
	}

	return (audioStreamDescriptor != nullptr && audioSampleProvider != nullptr) ? S_OK : E_OUTOFMEMORY;
}

HRESULT FFmpegInteropMSS::CreateVideoStreamDescriptor(bool forceVideoDecode)
{
	VideoEncodingProperties^ videoProperties;

	if (avVideoCodecCtx->codec_id == AV_CODEC_ID_H264 && !forceVideoDecode)
	{
		videoProperties = VideoEncodingProperties::CreateH264();
		videoProperties->ProfileId = avVideoCodecCtx->profile;
		videoProperties->Height = avVideoCodecCtx->height;
		videoProperties->Width = avVideoCodecCtx->width;

		videoSampleProvider = ref new H264SampleProvider(m_pReader, avFormatCtx, avVideoCodecCtx);
	}
	else
	{
		videoProperties = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Nv12, avVideoCodecCtx->width, avVideoCodecCtx->height);
		videoSampleProvider = ref new UncompressedVideoSampleProvider(m_pReader, avFormatCtx, avVideoCodecCtx);
	}

	videoProperties->FrameRate->Numerator = avVideoCodecCtx->framerate.num;
	videoProperties->FrameRate->Denominator = avVideoCodecCtx->framerate.den;
	videoProperties->Bitrate = avVideoCodecCtx->bit_rate;
	videoStreamDescriptor = ref new VideoStreamDescriptor(videoProperties);

	return (videoStreamDescriptor != nullptr && videoSampleProvider != nullptr) ? S_OK : E_OUTOFMEMORY;
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

				// Flush the AudioSampleProvider
				if (audioSampleProvider != nullptr)
				{
					audioSampleProvider->Flush();
					avcodec_flush_buffers(avAudioCodecCtx);
				}

				// Flush the VideoSampleProvider
				if (videoSampleProvider != nullptr)
				{
					videoSampleProvider->Flush();
					avcodec_flush_buffers(avVideoCodecCtx);
				}
			}
		}

		request->SetActualStartPosition(request->StartPosition->Value);
	}
}

void FFmpegInteropMSS::OnSampleRequested(Windows::Media::Core::MediaStreamSource ^sender, MediaStreamSourceSampleRequestedEventArgs ^args)
{
	if (args->Request->StreamDescriptor == audioStreamDescriptor && audioSampleProvider != nullptr)
	{
		args->Request->Sample = audioSampleProvider->GetNextSample();
	}
	else if (args->Request->StreamDescriptor == videoStreamDescriptor && videoSampleProvider != nullptr)
	{
		args->Request->Sample = videoSampleProvider->GetNextSample();
	}
	else
	{
		args->Request->Sample = nullptr;
	}
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
