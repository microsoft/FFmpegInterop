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

extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}

using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::Core;

const int AUDIOPKTBUFFERSZ = 320;
const int VIDEOPKTBUFFERSZ = 320;
const int FILESTREAMBUFFERSZ = 1024;

namespace FFMPEGMediaStreamSource
{
	public ref class FFMPEG sealed
	{
	public:
		FFMPEG(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode);
		virtual ~FFMPEG();
		MediaStreamSource^ GetMSS();

	private:
		void OnStarting(MediaStreamSource ^sender, MediaStreamSourceStartingEventArgs ^args);
		void OnSampleRequested(MediaStreamSource ^sender, MediaStreamSourceSampleRequestedEventArgs ^args);
		void CreateAudioStreamDescriptor(bool forceAudioDecode);
		void CreateVideoStreamDescriptor(bool forceVideoDecode);
		MediaStreamSource^ CreateMediaStreamSource(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode);
		MediaStreamSample^ FillAudioSample();
		MediaStreamSample^ FillVideoSample();
		int ReadPacket();
		void GetSPSAndPPSBuffer(DataWriter^ dataWriter);
		void WriteAnnexBPacket(DataWriter^ dataWriter, AVPacket avPacket);

		MediaStreamSource^ mss;
		AVIOContext *avIOCtx;
		AVFormatContext* avFormatCtx;
		AVCodecContext* avAudioCodecCtx;
		AVCodecContext* avVideoCodecCtx;
		SwrContext *swrCtx;
		AVFrame* avFrame;
		AudioStreamDescriptor^ audioStreamDescriptor;
		VideoStreamDescriptor^ videoStreamDescriptor;
		int audioStreamIndex;
		int videoStreamIndex;
		bool generateUncompressedAudio;
		bool generateUncompressedVideo;

		uint8_t* videoBufferData[4];
		int videoBufferLineSize[4];

		AVPacket audioPacketQueue[AUDIOPKTBUFFERSZ];
		int audioPacketQueueHead;
		int audioPacketQueueCount;
		void PushAudioPacket(AVPacket packet);
		AVPacket PopAudioPacket();

		AVPacket videoPacketQueue[VIDEOPKTBUFFERSZ];
		int videoPacketQueueHead;
		int videoPacketQueueCount;
		void PushVideoPacket(AVPacket packet);
		AVPacket PopVideoPacket();

		TimeSpan mediaDuration;
		IStream* fileStreamData;
		unsigned char* fileStreamBuffer;
	};
}
