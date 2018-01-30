#include "pch.h"
#include "CompressedSampleProvider.h"
#include "NativeBufferFactory.h"

using namespace FFmpegInterop;

CompressedSampleProvider::CompressedSampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx) : MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
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

	if (hr == S_OK)
	{
		hr = CreateBufferFromPacket(avPacket, pBuffer);
	}

	return hr;
}

void free_buffer(void *lpVoid);

HRESULT CompressedSampleProvider::CreateBufferFromPacket(AVPacket* avPacket, IBuffer^* pBuffer)
{
	HRESULT hr = S_OK;

	// Using direct buffer: just create a buffer reference to hand out to MSS pipeline
	auto bufferRef = av_buffer_ref(avPacket->buf);
	if (bufferRef)
	{
		*pBuffer = NativeBuffer::NativeBufferFactory::CreateNativeBuffer(bufferRef->data, bufferRef->size, free_buffer, bufferRef);
	}
	else
	{
		hr = E_FAIL;
	}

	return hr;
}

void free_buffer(void *lpVoid)
{
	auto buffer = (AVBufferRef *)lpVoid;
	auto count = av_buffer_get_ref_count(buffer);
	av_buffer_unref(&buffer);
}