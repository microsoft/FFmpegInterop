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
#include "UncompressedSampleProvider.h"

extern "C"
{
#include <libswscale/swscale.h>
}

using namespace Platform;

namespace FFmpegInterop
{
	ref class UncompressedVideoSampleProvider: UncompressedSampleProvider
	{
	public:
		virtual ~UncompressedVideoSampleProvider();
		virtual MediaStreamSample^ GetNextSample() override;
		property String^ OutputMediaSubtype;

	internal:
		UncompressedVideoSampleProvider(
			FFmpegReader^ reader,
			AVFormatContext* avFormatCtx,
			AVCodecContext* avCodecCtx);
		virtual HRESULT WriteAVPacketToStream(DataWriter^ writer, AVPacket* avPacket) override;
		virtual HRESULT DecodeAVPacket(DataWriter^ dataWriter, AVPacket* avPacket, int64_t& framePts, int64_t& frameDuration) override;

	private:
		HRESULT InitializeScalerIfRequired(AVFrame* frame);

		static int get_buffer2(AVCodecContext *avCodecContext, AVFrame *frame, int flags);
		static void free_buffer(void *lpVoid);
		AVBufferPool *m_pBufferPool;
		int m_bufferHeight;
		AVPixelFormat m_OutputPixelFormat;
		SwsContext* m_pSwsCtx;
		bool m_interlaced_frame;
		bool m_top_field_first;
		bool m_bIsInitialized;
	};

	private ref class FrameDataHolder
	{
	internal:
		int LineSize[4];
		uint8_t* Data[4];
		bool IsAllocated;

	public:
		virtual ~FrameDataHolder()
		{
			if (IsAllocated)
			{
				av_freep(Data);
			}
		};
	};
}

