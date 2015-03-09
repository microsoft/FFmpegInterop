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

using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::Core;

extern "C"
{
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace FFmpeg
{
	public ref class FFmpegLibrary sealed
	{
	public:
		FFmpegLibrary(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode);
		virtual ~FFmpegLibrary();
		MediaStreamSource^ GetMediaStreamSource();

	private:
		MediaStreamSource^ CreateMediaStreamSource(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode);
		void CreateAudioStreamDescriptor(bool forceAudioDecode);
		void CreateVideoStreamDescriptor(bool forceVideoDecode);
		void OnStarting(MediaStreamSource ^sender, MediaStreamSourceStartingEventArgs ^args);
		void OnSampleRequested(MediaStreamSource ^sender, MediaStreamSourceSampleRequestedEventArgs ^args);
		MediaStreamSample^ FillAudioSample();
		MediaStreamSample^ FillVideoSample();
		int ReadPacket();
		void GetSPSAndPPSBuffer(DataWriter^ dataWriter);
		void WriteAnnexBPacket(DataWriter^ dataWriter, AVPacket avPacket);

		MediaStreamSource^ mss;
		EventRegistrationToken startingRequestedToken;
		EventRegistrationToken sampleRequestedToken;

		AVIOContext* avIOCtx;
		AVFormatContext* avFormatCtx;
		AVCodecContext* avAudioCodecCtx;
		AVCodecContext* avVideoCodecCtx;
		SwrContext *swrCtx;
		SwsContext *swsCtx;
		AVFrame* avFrame;

		AudioStreamDescriptor^ audioStreamDescriptor;
		VideoStreamDescriptor^ videoStreamDescriptor;
		int audioStreamIndex;
		int videoStreamIndex;
		bool generateUncompressedAudio;
		bool generateUncompressedVideo;

		uint8_t* videoBufferData[4];
		int videoBufferLineSize[4];
		TimeSpan mediaDuration;
		IStream* fileStreamData;
		unsigned char* fileStreamBuffer;

		std::queue<AVPacket> audioPacketQueue;
		std::queue<AVPacket> videoPacketQueue;
		void PushAudioPacket(AVPacket packet);
		AVPacket PopAudioPacket();
		void PushVideoPacket(AVPacket packet);
		AVPacket PopVideoPacket();
	};
}
