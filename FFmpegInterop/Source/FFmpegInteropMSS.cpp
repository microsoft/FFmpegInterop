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
#include "H264AVCSampleProvider.h"
#include "H264SampleProvider.h"
#include "UncompressedAudioSampleProvider.h"
#include "UncompressedVideoSampleProvider.h"
#include "CritSec.h"
#include "shcore.h"
#include <mfapi.h>

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

// Static functions passed to FFmpeg
static int FileStreamRead(void* ptr, uint8_t* buf, int bufSize);
static int64_t FileStreamSeek(void* ptr, int64_t pos, int whence);
static int lock_manager(void **mtx, enum AVLockOp op);

// Flag for ffmpeg global setup
static bool isRegistered = false;

// Initialize an FFmpegInteropObject
FFmpegInteropMSS::FFmpegInteropMSS()
	: avDict(nullptr)
	, avIOCtx(nullptr)
	, avFormatCtx(nullptr)
	, avAudioCodecCtx(nullptr)
	, avVideoCodecCtx(nullptr)
	, audioStreamIndex(AVERROR_STREAM_NOT_FOUND)
	, videoStreamIndex(AVERROR_STREAM_NOT_FOUND)
	, thumbnailStreamIndex(AVERROR_STREAM_NOT_FOUND)
	, fileStreamData(nullptr)
	, fileStreamBuffer(nullptr)
{
	if (!isRegistered)
	{
		av_register_all();
		av_lockmgr_register(lock_manager);
		isRegistered = true;
	}
}

FFmpegInteropMSS::~FFmpegInteropMSS()
{
	mutexGuard.lock();
	if (mss)
	{
		mss->Starting -= startingRequestedToken;
		mss->SampleRequested -= sampleRequestedToken;
		mss = nullptr;
	}

	// Clear our data
	audioSampleProvider = nullptr;
	videoSampleProvider = nullptr;

	if (m_pReader != nullptr)
	{
		m_pReader->SetAudioStream(AVERROR_STREAM_NOT_FOUND, nullptr);
		m_pReader->SetVideoStream(AVERROR_STREAM_NOT_FOUND, nullptr);
		m_pReader = nullptr;
	}

	avcodec_close(avVideoCodecCtx);
	avcodec_close(avAudioCodecCtx);
	avformat_close_input(&avFormatCtx);
	av_free(avIOCtx);
	av_dict_free(&avDict);
	
	if (fileStreamData != nullptr)
	{
		fileStreamData->Release();
	}
	mutexGuard.unlock();
}

FFmpegInteropMSS^ FFmpegInteropMSS::CreateFFmpegInteropMSSFromStream(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode, PropertySet^ ffmpegOptions, MediaStreamSource^ mss)
{
	auto interopMSS = ref new FFmpegInteropMSS();
	if (FAILED(interopMSS->CreateMediaStreamSource(stream, forceAudioDecode, forceVideoDecode, ffmpegOptions, mss)))
	{
		// We failed to initialize, clear the variable to return failure
		interopMSS = nullptr;
	}

	return interopMSS;
}

FFmpegInteropMSS^ FFmpegInteropMSS::CreateFFmpegInteropMSSFromStream(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode, PropertySet^ ffmpegOptions)
{
	return CreateFFmpegInteropMSSFromStream(stream, forceAudioDecode, forceVideoDecode, nullptr, nullptr);
}

FFmpegInteropMSS^ FFmpegInteropMSS::CreateFFmpegInteropMSSFromStream(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode)
{
	return CreateFFmpegInteropMSSFromStream(stream, forceAudioDecode, forceVideoDecode, nullptr);
}

FFmpegInteropMSS^ FFmpegInteropMSS::CreateFFmpegInteropMSSFromUri(String^ uri, bool forceAudioDecode, bool forceVideoDecode, PropertySet^ ffmpegOptions)
{
	auto interopMSS = ref new FFmpegInteropMSS();
	if (FAILED(interopMSS->CreateMediaStreamSource(uri, forceAudioDecode, forceVideoDecode, ffmpegOptions)))
	{
		// We failed to initialize, clear the variable to return failure
		interopMSS = nullptr;
	}

	return interopMSS;
}

FFmpegInteropMSS^ FFmpegInteropMSS::CreateFFmpegInteropMSSFromUri(String^ uri, bool forceAudioDecode, bool forceVideoDecode)
{
	return CreateFFmpegInteropMSSFromUri(uri, forceAudioDecode, forceVideoDecode, nullptr);
}

MediaStreamSource^ FFmpegInteropMSS::GetMediaStreamSource()
{
	return mss;
}

HRESULT FFmpegInteropMSS::CreateMediaStreamSource(String^ uri, bool forceAudioDecode, bool forceVideoDecode, PropertySet^ ffmpegOptions)
{
	HRESULT hr = S_OK;
	const char* charStr = nullptr;
	if (!uri)
	{
		hr = E_INVALIDARG;
	}

	if (SUCCEEDED(hr))
	{
		avFormatCtx = avformat_alloc_context();
		if (avFormatCtx == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		// Populate AVDictionary avDict based on PropertySet ffmpegOptions. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
		hr = ParseOptions(ffmpegOptions);
	}

	if (SUCCEEDED(hr))
	{
		std::wstring uriW(uri->Begin());
		std::string uriA(uriW.begin(), uriW.end());
		charStr = uriA.c_str();

		// Open media in the given URI using the specified options
		if (avformat_open_input(&avFormatCtx, charStr, NULL, &avDict) < 0)
		{
			hr = E_FAIL; // Error opening file
		}

		// avDict is not NULL only when there is an issue with the given ffmpegOptions such as invalid key, value type etc. Iterate through it to see which one is causing the issue.
		if (avDict != nullptr)
		{
			DebugMessage(L"Invalid FFmpeg option(s)");
			av_dict_free(&avDict);
			avDict = nullptr;
		}
	}

	if (SUCCEEDED(hr))
	{
		this->mss = nullptr;
		hr = InitFFmpegContext(forceAudioDecode, forceVideoDecode);
	}

	return hr;
}

HRESULT FFmpegInteropMSS::CreateMediaStreamSource(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode, PropertySet^ ffmpegOptions, MediaStreamSource^ mss)
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
		avFormatCtx = avformat_alloc_context();
		if (avFormatCtx == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	if (SUCCEEDED(hr))
	{
		// Populate AVDictionary avDict based on PropertySet ffmpegOptions. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
		hr = ParseOptions(ffmpegOptions);
	}

	if (SUCCEEDED(hr))
	{
		avFormatCtx->pb = avIOCtx;
		avFormatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

		// Open media file using custom IO setup above instead of using file name. Opening a file using file name will invoke fopen C API call that only have
		// access within the app installation directory and appdata folder. Custom IO allows access to file selected using FilePicker dialog.
		if (avformat_open_input(&avFormatCtx, "", NULL, &avDict) < 0)
		{
			hr = E_FAIL; // Error opening file
		}

		// avDict is not NULL only when there is an issue with the given ffmpegOptions such as invalid key, value type etc. Iterate through it to see which one is causing the issue.
		if (avDict != nullptr)
		{
			DebugMessage(L"Invalid FFmpeg option(s)");
			av_dict_free(&avDict);
			avDict = nullptr;
		}
	}

	if (SUCCEEDED(hr))
	{
		this->mss = mss;
		hr = InitFFmpegContext(forceAudioDecode, forceVideoDecode);
	}

	return hr;
}

HRESULT FFmpegInteropMSS::InitFFmpegContext(bool forceAudioDecode, bool forceVideoDecode)
{
	HRESULT hr = S_OK;

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
		// Find the audio stream and its decoder
		AVCodec* avAudioCodec = nullptr;
		audioStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &avAudioCodec, 0);
		if (audioStreamIndex != AVERROR_STREAM_NOT_FOUND && avAudioCodec)
		{
			// allocate a new decoding context
			avAudioCodecCtx = avcodec_alloc_context3(avAudioCodec);
			if (!avAudioCodecCtx)
			{
				hr = E_OUTOFMEMORY;
				DebugMessage(L"Could not allocate a decoding context\n");
				avformat_close_input(&avFormatCtx);
			}

			if (SUCCEEDED(hr))
			{
				// initialize the stream parameters with demuxer information
				if (avcodec_parameters_to_context(avAudioCodecCtx, avFormatCtx->streams[audioStreamIndex]->codecpar) < 0)
				{
					hr = E_FAIL;
					avformat_close_input(&avFormatCtx);
					avcodec_free_context(&avAudioCodecCtx);
				}

				if (SUCCEEDED(hr))
				{
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

						if (SUCCEEDED(hr))
						{
							// Convert audio codec name for property
							hr = ConvertCodecName(avAudioCodec->name, &audioCodecName);
						}
					}
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		// Find the video stream and its decoder
		AVCodec* avVideoCodec = nullptr;
		videoStreamIndex = av_find_best_stream(avFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &avVideoCodec, 0);
		if (videoStreamIndex != AVERROR_STREAM_NOT_FOUND && avVideoCodec)
		{
			// FFmpeg identifies album/cover art from a music file as a video stream
			// Avoid creating unnecessarily video stream from this album/cover art
			if (avFormatCtx->streams[videoStreamIndex]->disposition == AV_DISPOSITION_ATTACHED_PIC)
			{
				thumbnailStreamIndex = videoStreamIndex;
				videoStreamIndex = AVERROR_STREAM_NOT_FOUND;
				avVideoCodec = nullptr;
			}
			else
			{
				thumbnailStreamIndex = AVERROR_STREAM_NOT_FOUND;
				AVDictionaryEntry *rotate_tag = av_dict_get(avFormatCtx->streams[videoStreamIndex]->metadata, "rotate", NULL, 0);
				if (rotate_tag != NULL)
				{
					rotateVideo = true;
					rotationAngle = atoi(rotate_tag->value);
				}
				else
				{
					rotateVideo = false;
				}
				// allocate a new decoding context
				avVideoCodecCtx = avcodec_alloc_context3(avVideoCodec);
				if (!avVideoCodecCtx)
				{
					DebugMessage(L"Could not allocate a decoding context\n");
					avformat_close_input(&avFormatCtx);
					hr = E_OUTOFMEMORY;
				}

				if (SUCCEEDED(hr))
				{
					// initialize the stream parameters with demuxer information
					if (avcodec_parameters_to_context(avVideoCodecCtx, avFormatCtx->streams[videoStreamIndex]->codecpar) < 0)
					{
						avformat_close_input(&avFormatCtx);
						avcodec_free_context(&avVideoCodecCtx);
						hr = E_FAIL;
					}
				}

				if (SUCCEEDED(hr))
				{
					// enable multi threading
					unsigned threads = std::thread::hardware_concurrency();
					if (threads > 0)
					{
						avVideoCodecCtx->thread_count = threads;
						avVideoCodecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
					}

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

						if (SUCCEEDED(hr))
						{
							// Convert video codec name for property
							hr = ConvertCodecName(avVideoCodec->name, &videoCodecName);
						}
					}
				}
			}
		}
	}

	if (SUCCEEDED(hr))
	{
		// Convert media duration from AV_TIME_BASE to TimeSpan unit
		mediaDuration = { LONGLONG(avFormatCtx->duration * 10000000 / double(AV_TIME_BASE)) };

		if (audioStreamDescriptor)
		{
			if (videoStreamDescriptor)
			{
				if (mss)
				{
					mss->AddStreamDescriptor(videoStreamDescriptor);
					mss->AddStreamDescriptor(audioStreamDescriptor);
				}
				else
				{
					mss = ref new MediaStreamSource(videoStreamDescriptor, audioStreamDescriptor);
				}
			}
			else
			{
				if (mss)
				{
					mss->AddStreamDescriptor(audioStreamDescriptor);
				}
				else
				{
					mss = ref new MediaStreamSource(audioStreamDescriptor);
				}
			}
		}
		else if (videoStreamDescriptor)
		{
			if (mss)
			{
				mss->AddStreamDescriptor(videoStreamDescriptor);
			}
			else
			{
				mss = ref new MediaStreamSource(videoStreamDescriptor);
			}
		}
		if (mss)
		{
			if (mediaDuration.Duration > 0)
			{
				mss->Duration = mediaDuration;
				mss->CanSeek = true;
			}
			else
			{
				// Set buffer time to 0 for realtime streaming to reduce latency
				mss->BufferTime = { 0 };
			}

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

MediaThumbnailData ^ FFmpegInterop::FFmpegInteropMSS::ExtractThumbnail()
{
	if (thumbnailStreamIndex != AVERROR_STREAM_NOT_FOUND)
	{
		// FFmpeg identifies album/cover art from a music file as a video stream
		// Avoid creating unnecessarily video stream from this album/cover art
		if (avFormatCtx->streams[thumbnailStreamIndex]->disposition == AV_DISPOSITION_ATTACHED_PIC)
		{
			auto imageStream = avFormatCtx->streams[thumbnailStreamIndex];
			//save album art to file.
			String^ extension = ".jpeg";
			switch (imageStream->codecpar->codec_id)
			{
			case AV_CODEC_ID_MJPEG:
			case AV_CODEC_ID_MJPEGB:
			case AV_CODEC_ID_JPEG2000:
			case AV_CODEC_ID_JPEGLS: extension = ".jpeg"; break;
			case AV_CODEC_ID_PNG: extension = ".png"; break;
			case AV_CODEC_ID_BMP: extension = ".bmp"; break;
			}


			auto vector = ref new Array<uint8_t>(imageStream->attached_pic.data, imageStream->attached_pic.size);
			DataWriter^ writer = ref new DataWriter();
			writer->WriteBytes(vector);

			return (ref new MediaThumbnailData(writer->DetachBuffer(), extension));
		}
	}

	return nullptr;
}

HRESULT FFmpegInteropMSS::ConvertCodecName(const char* codecName, String^ *outputCodecName)
{
	HRESULT hr = S_OK;

	// Convert codec name from const char* to Platform::String
	auto codecNameChars = codecName;
	size_t newsize = strlen(codecNameChars) + 1;
	wchar_t * wcstring = new(std::nothrow) wchar_t[newsize];
	if (wcstring == nullptr)
	{
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		size_t convertedChars = 0;
		mbstowcs_s(&convertedChars, wcstring, newsize, codecNameChars, _TRUNCATE);
		*outputCodecName = ref new Platform::String(wcstring);
		delete[] wcstring;
	}

	return hr;
}

HRESULT FFmpegInteropMSS::CreateAudioStreamDescriptor(bool forceAudioDecode)
{
	if (avAudioCodecCtx->codec_id == AV_CODEC_ID_AAC && !forceAudioDecode)
	{
		if (avAudioCodecCtx->extradata_size == 0)
		{
			audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreateAacAdts(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, (unsigned int)avAudioCodecCtx->bit_rate));
		}
		else
		{
			audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreateAac(avAudioCodecCtx->profile == FF_PROFILE_AAC_HE || avAudioCodecCtx->profile == FF_PROFILE_AAC_HE_V2 ? avAudioCodecCtx->sample_rate / 2 : avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, (unsigned int)avAudioCodecCtx->bit_rate));
		}
		audioSampleProvider = ref new MediaSampleProvider(m_pReader, avFormatCtx, avAudioCodecCtx);
	}
	else if (avAudioCodecCtx->codec_id == AV_CODEC_ID_MP3 && !forceAudioDecode)
	{
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreateMp3(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, (unsigned int)avAudioCodecCtx->bit_rate));
		audioSampleProvider = ref new MediaSampleProvider(m_pReader, avFormatCtx, avAudioCodecCtx);
	}
	else
	{
		// We always convert to 16-bit audio so set the size here
		audioStreamDescriptor = ref new AudioStreamDescriptor(AudioEncodingProperties::CreatePcm(avAudioCodecCtx->sample_rate, avAudioCodecCtx->channels, 16));
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

		// Check for H264 bitstream flavor. H.264 AVC extradata starts with 1 while non AVC one starts with 0
		if (avVideoCodecCtx->extradata != nullptr && avVideoCodecCtx->extradata_size > 0 && avVideoCodecCtx->extradata[0] == 1)
		{
			videoSampleProvider = ref new H264AVCSampleProvider(m_pReader, avFormatCtx, avVideoCodecCtx);
		}
		else
		{
			videoSampleProvider = ref new H264SampleProvider(m_pReader, avFormatCtx, avVideoCodecCtx);
		}
	}
	else
	{
		videoProperties = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Nv12, avVideoCodecCtx->width, avVideoCodecCtx->height);
		videoSampleProvider = ref new UncompressedVideoSampleProvider(m_pReader, avFormatCtx, avVideoCodecCtx);

		if (avVideoCodecCtx->sample_aspect_ratio.num > 0 && avVideoCodecCtx->sample_aspect_ratio.den != 0)
		{
			videoProperties->PixelAspectRatio->Numerator = avVideoCodecCtx->sample_aspect_ratio.num;
			videoProperties->PixelAspectRatio->Denominator = avVideoCodecCtx->sample_aspect_ratio.den;
		}

		videoProperties->Properties->Insert(MF_MT_INTERLACE_MODE, (uint32)_MFVideoInterlaceMode::MFVideoInterlace_MixedInterlaceOrProgressive);
	}
	if (rotateVideo)
	{
		Platform::Guid MF_MT_VIDEO_ROTATION(0xC380465D, 0x2271, 0x428C, 0x9B, 0x83, 0xEC, 0xEA, 0x3B, 0x4A, 0x85, 0xC1);
		videoProperties->Properties->Insert(MF_MT_VIDEO_ROTATION, (uint32)rotationAngle);
	}
	// Detect the correct framerate
	if (avVideoCodecCtx->framerate.num != 0 || avVideoCodecCtx->framerate.den != 1)
	{
		videoProperties->FrameRate->Numerator = avVideoCodecCtx->framerate.num;
		videoProperties->FrameRate->Denominator = avVideoCodecCtx->framerate.den;
	}
	else if (avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.num != 0 || avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.den != 0)
	{
		videoProperties->FrameRate->Numerator = avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.num;
		videoProperties->FrameRate->Denominator = avFormatCtx->streams[videoStreamIndex]->avg_frame_rate.den;
	}

	videoProperties->Bitrate = (unsigned int)avVideoCodecCtx->bit_rate;
	videoStreamDescriptor = ref new VideoStreamDescriptor(videoProperties);

	return (videoStreamDescriptor != nullptr && videoSampleProvider != nullptr) ? S_OK : E_OUTOFMEMORY;
}

HRESULT FFmpegInteropMSS::ParseOptions(PropertySet^ ffmpegOptions)
{
	HRESULT hr = S_OK;

	// Convert FFmpeg options given in PropertySet to AVDictionary. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
	if (ffmpegOptions != nullptr)
	{
		auto options = ffmpegOptions->First();

		while (options->HasCurrent)
		{
			String^ key = options->Current->Key;
			std::wstring keyW(key->Begin());
			std::string keyA(keyW.begin(), keyW.end());
			const char* keyChar = keyA.c_str();

			// Convert value from Object^ to const char*. avformat_open_input will internally convert value from const char* to the correct type
			String^ value = options->Current->Value->ToString();
			std::wstring valueW(value->Begin());
			std::string valueA(valueW.begin(), valueW.end());
			const char* valueChar = valueA.c_str();

			// Add key and value pair entry
			if (av_dict_set(&avDict, keyChar, valueChar, 0) < 0)
			{
				hr = E_INVALIDARG;
				break;
			}

			options->MoveNext();
		}
	}

	return hr;
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

			if (av_seek_frame(avFormatCtx, streamIndex, seekTarget, AVSEEK_FLAG_BACKWARD) < 0)
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
	mutexGuard.lock();
	if (mss != nullptr)
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
	mutexGuard.unlock();
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

static int lock_manager(void **mtx, enum AVLockOp op)
{
	switch (op)
	{
	case AV_LOCK_CREATE:
	{
		*mtx = new CritSec();
		return 0;
	}
	case AV_LOCK_OBTAIN:
	{
		auto mutex = static_cast<CritSec*>(*mtx);
		mutex->Lock();
		return 0;
	}
	case AV_LOCK_RELEASE:
	{
		auto mutex = static_cast<CritSec*>(*mtx);
		mutex->Unlock();
		return 0;
	}
	case AV_LOCK_DESTROY:
	{
		auto mutex = static_cast<CritSec*>(*mtx);
		delete mutex;
		return 0;
	}
	}
	return 1;
}
