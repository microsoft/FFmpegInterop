#include "pch.h"
#include "CompressedSampleProvider.h"
#include "NativeBufferFactory.h"

using namespace FFmpegInterop;

CompressedSampleProvider::CompressedSampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx,
	FFmpegInteropConfig^ config,
	int streamIndex,
	VideoEncodingProperties^ encodingProperties) :
	MediaSampleProvider(reader, avFormatCtx, avCodecCtx, config, streamIndex),
	videoEncodingProperties(encodingProperties)
{
}

CompressedSampleProvider::CompressedSampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx,
	FFmpegInteropConfig^ config,
	int streamIndex,
	AudioEncodingProperties^ encodingProperties) :
	MediaSampleProvider(reader, avFormatCtx, avCodecCtx, config, streamIndex),
	audioEncodingProperties(encodingProperties)
{
}

CompressedSampleProvider::~CompressedSampleProvider()
{
}

HRESULT CompressedSampleProvider::CreateNextSampleBuffer(IBuffer^* pBuffer, int64_t& samplePts, int64_t& sampleDuration)
{
	HRESULT hr = S_OK;

	AVPacket* avPacket = NULL;

	hr = GetNextPacket(&avPacket, samplePts, sampleDuration);

	if (hr == S_OK) // Do not create packet at end of stream (S_FALSE)
	{
		hr = CreateBufferFromPacket(avPacket, pBuffer);
	}

	return hr;
}

HRESULT CompressedSampleProvider::CreateBufferFromPacket(AVPacket* avPacket, IBuffer^* pBuffer)
{
	HRESULT hr = S_OK;

	// Using direct buffer: just create a buffer reference to hand out to MSS pipeline
	auto bufferRef = av_buffer_ref(avPacket->buf);
	if (bufferRef)
	{
		*pBuffer = NativeBuffer::NativeBufferFactory::CreateNativeBuffer(avPacket->data, avPacket->size, free_buffer, bufferRef);
	}
	else
	{
		hr = E_FAIL;
	}

	return hr;
}

IMediaStreamDescriptor^ CompressedSampleProvider::CreateStreamDescriptor()
{
	IMediaStreamDescriptor^ mediaStreamDescriptor;
	if (videoEncodingProperties != nullptr)
	{
		SetCommonVideoEncodingProperties(videoEncodingProperties, true);
		mediaStreamDescriptor = ref new VideoStreamDescriptor(videoEncodingProperties);
	}
	else
	{
		mediaStreamDescriptor = ref new AudioStreamDescriptor(audioEncodingProperties);
	}
	return mediaStreamDescriptor;
}
