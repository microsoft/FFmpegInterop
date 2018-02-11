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

#pragma once
#include <queue>
#include <mutex>
#include "FFmpegReader.h"
#include "MediaSampleProvider.h"
#include "MediaThumbnailData.h"
#include "VideoFrame.h"
#include <pplawait.h>

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media::Core;

extern "C"
{
#include <libavformat/avformat.h>
}

namespace FFmpegInterop
{
	public ref class FFmpegInteropMSS sealed
	{
	public:
		static IAsyncOperation<FFmpegInteropMSS^>^ CreateFromStreamAsync(IRandomAccessStream^ stream, FFmpegInteropConfig^ config);
		static IAsyncOperation<FFmpegInteropMSS^>^ CreateFromStreamAsync(IRandomAccessStream^ stream) { return CreateFromStreamAsync(stream, ref new FFmpegInteropConfig()); }

		static IAsyncOperation<FFmpegInteropMSS^>^ CreateFromUriAsync(String^ uri, FFmpegInteropConfig^ config);
		static IAsyncOperation<FFmpegInteropMSS^>^ CreateFromUriAsync(String^ uri) { return CreateFromUriAsync(uri, ref new FFmpegInteropConfig()); }

		static FFmpegInteropMSS^ CreateFFmpegInteropMSSFromStream(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode, PropertySet^ ffmpegOptions, MediaStreamSource^ mss);
		static FFmpegInteropMSS^ CreateFFmpegInteropMSSFromStream(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode, PropertySet^ ffmpegOptions);
		static FFmpegInteropMSS^ CreateFFmpegInteropMSSFromStream(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode);

		static FFmpegInteropMSS^ CreateFFmpegInteropMSSFromUri(String^ uri, bool forceAudioDecode, bool forceVideoDecode, PropertySet^ ffmpegOptions);
		static FFmpegInteropMSS^ CreateFFmpegInteropMSSFromUri(String^ uri, bool forceAudioDecode, bool forceVideoDecode);

		static IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(IRandomAccessStream^ stream, TimeSpan position, bool exactSeek, int maxFrameSkip);
		static IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(IRandomAccessStream^ stream, TimeSpan position, bool exactSeek) { return ExtractVideoFrameAsync(stream, position, exactSeek, 0); };
		static IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(IRandomAccessStream^ stream, TimeSpan position) { return ExtractVideoFrameAsync(stream, position, false, 0); };
		static IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(IRandomAccessStream^ stream) { return ExtractVideoFrameAsync(stream, { 0 }, false, 0); };
		
		MediaThumbnailData^ ExtractThumbnail();

		// Contructor
		MediaStreamSource^ GetMediaStreamSource();
		virtual ~FFmpegInteropMSS();

		// Properties
		property AudioStreamDescriptor^ AudioDescriptor
		{
			AudioStreamDescriptor^ get()
			{
				return audioStreamDescriptor;
			};
		};
		property VideoStreamDescriptor^ VideoDescriptor
		{
			VideoStreamDescriptor^ get()
			{
				return videoStreamDescriptor;
			};
		};
		property TimeSpan Duration
		{
			TimeSpan get()
			{
				return mediaDuration;
			};
		};
		property String^ VideoCodecName
		{
			String^ get()
			{
				return videoCodecName;
			};
		};
		property String^ AudioCodecName
		{
			String^ get()
			{
				return audioCodecName;
			};
		};

	internal:
		int ReadPacket();

	private:
		FFmpegInteropMSS(FFmpegInteropConfig^ config);

		static FFmpegInteropMSS^ CreateFromStream(IRandomAccessStream^ stream, FFmpegInteropConfig^ config, MediaStreamSource^ mss);
		static FFmpegInteropMSS^ CreateFromUri(String^ uri, FFmpegInteropConfig^ config);
		HRESULT CreateMediaStreamSource(IRandomAccessStream^ stream, MediaStreamSource^ mss);
		HRESULT CreateMediaStreamSource(String^ uri);
		HRESULT InitFFmpegContext();
		HRESULT CreateAudioStreamDescriptor();
		HRESULT CreateVideoStreamDescriptor();
		HRESULT ConvertCodecName(const char* codecName, String^ *outputCodecName);
		HRESULT ParseOptions(PropertySet^ ffmpegOptions);
		void OnStarting(MediaStreamSource ^sender, MediaStreamSourceStartingEventArgs ^args);
		void OnSampleRequested(MediaStreamSource ^sender, MediaStreamSourceSampleRequestedEventArgs ^args);
		HRESULT Seek(TimeSpan position);

		MediaStreamSource^ mss;
		EventRegistrationToken startingRequestedToken;
		EventRegistrationToken sampleRequestedToken;

		internal:
		AVDictionary* avDict;
		AVIOContext* avIOCtx;
		AVFormatContext* avFormatCtx;
		AVCodecContext* avAudioCodecCtx;
		AVCodecContext* avVideoCodecCtx;

		private:
		FFmpegInteropConfig ^ config;
		AudioStreamDescriptor^ audioStreamDescriptor;
		VideoStreamDescriptor^ videoStreamDescriptor;
		int audioStreamIndex;
		int videoStreamIndex;
		int thumbnailStreamIndex;
		
		bool rotateVideo;
		int rotationAngle;
		std::recursive_mutex mutexGuard;
		
		MediaSampleProvider^ audioSampleProvider;
		MediaSampleProvider^ videoSampleProvider;

		String^ videoCodecName;
		String^ audioCodecName;
		TimeSpan mediaDuration;
		IStream* fileStreamData;
		unsigned char* fileStreamBuffer;
		FFmpegReader^ m_pReader;
		bool isFirstSeek;
	};
}
