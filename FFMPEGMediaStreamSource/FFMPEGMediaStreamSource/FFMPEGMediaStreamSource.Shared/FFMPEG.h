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
#include <libavformat\avformat.h>
}

using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::Media::Core;

const int AUDIOPKTBUFFERSZ = 32;
const int VIDEOPKTBUFFERSZ = 32;
const int FILESTREAMBUFFERSZ = 32 * 1024;

namespace FFMPEGMediaStreamSource
{
	public ref class FFMPEG sealed
	{
	public:
		FFMPEG();
		virtual ~FFMPEG();

		void Initialize();
		void Close();

		MediaStreamSource^ CreateMediaStreamSource(IRandomAccessStream^ stream, bool forceAudioDecode, bool forceVideoDecode);

		void OnStarting(MediaStreamSource ^sender, MediaStreamSourceStartingEventArgs ^args);
		void OnSampleRequested(MediaStreamSource ^sender, MediaStreamSourceSampleRequestedEventArgs ^args);

	private:
		MediaStreamSample^ FillAudioSample();
		MediaStreamSample^ FillVideoSample();
		int ReadPacket();
		void GetSPSAndPPSBuffer(DataWriter^ dataWriter);
		void WriteAnnexBPacket(DataWriter^ dataWriter, AVPacket avPacket);

		AVIOContext *avIOCtx;
		AVFormatContext* avFormatCtx;
		AVCodecContext* avAudioCodecCtx;
		AVCodecContext* avVideoCodecCtx;
		AVCodec* avAudioCodec;
		AVCodec* avVideoCodec;
		AVFrame* avFrame;
		int audioStreamIndex;
		int videoStreamIndex;
		bool compressedVideo;

		uint8_t* videoBufferData[4];
		int videoBufferLineSize[4];

		int audioStreamId;
		int videoStreamId;

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

