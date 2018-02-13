#pragma once

#include "MediaSampleProvider.h"

namespace FFmpegInterop
{
	ref class CompressedSampleProvider : MediaSampleProvider
	{
	public:
		virtual ~CompressedSampleProvider();

	internal:
		CompressedSampleProvider(FFmpegReader^ reader, AVFormatContext* avFormatCtx, AVCodecContext* avCodecCtx, FFmpegInteropConfig^ config, int streamIndex);
		virtual HRESULT CreateNextSampleBuffer(IBuffer^* pBuffer, int64_t& samplePts, int64_t& sampleDuration) override;
		virtual HRESULT CreateBufferFromPacket(AVPacket* avPacket, IBuffer^* pBuffer);
	};
}