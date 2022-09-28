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
#include "UncompressedVideoSampleProvider.h"
#include "NativeBufferFactory.h"
#include <mfapi.h>

extern "C"
{
#include <libavutil/imgutils.h>
}

using namespace FFmpegInterop;
using namespace NativeBuffer;
using namespace Windows::Media::MediaProperties;

UncompressedVideoSampleProvider::UncompressedVideoSampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx)
	: UncompressedSampleProvider(reader, avFormatCtx, avCodecCtx)
{
	switch (m_pAvCodecCtx->pix_fmt)
	{
	case AV_PIX_FMT_YUV420P:
		m_OutputPixelFormat = AV_PIX_FMT_YUV420P;
		OutputMediaSubtype = MediaEncodingSubtypes::Iyuv;
		break;
	case AV_PIX_FMT_YUVJ420P:
		m_OutputPixelFormat = AV_PIX_FMT_YUVJ420P;
		OutputMediaSubtype = MediaEncodingSubtypes::Iyuv;
		break;
	case AV_PIX_FMT_YUVA420P:
		m_OutputPixelFormat = AV_PIX_FMT_BGRA;
		OutputMediaSubtype = MediaEncodingSubtypes::Argb32;
		break;
	default:
		m_OutputPixelFormat = AV_PIX_FMT_NV12;
		OutputMediaSubtype = MediaEncodingSubtypes::Nv12;
		break;
	}

	// MPEG2 is often interlaced, and DXVA HW deinterlacing only works with NV12. Let's force output to NV12
	if (m_pAvCodecCtx->codec_id == AV_CODEC_ID_MPEG2VIDEO || m_pAvCodecCtx->codec_id == AV_CODEC_ID_MPEG2VIDEO_XVMC)
	{
		m_OutputPixelFormat = AV_PIX_FMT_NV12;
		OutputMediaSubtype = MediaEncodingSubtypes::Nv12;
	}

	auto width = avCodecCtx->width;
	auto height = avCodecCtx->height;

	if (m_pAvCodecCtx->pix_fmt == m_OutputPixelFormat)
	{
		if (m_pAvCodecCtx->codec->capabilities & AV_CODEC_CAP_DR1)
		{
			// This codec supports direct buffer decoding.
			// Get decoder frame size and override get_buffer2...
			avcodec_align_dimensions(m_pAvCodecCtx, &width, &height);

			m_pAvCodecCtx->get_buffer2 = get_buffer2;
			m_pAvCodecCtx->opaque = (void*)this;
		}
		else
		{
			// We cannot use direct buffer decoding with this codec.
			// Now that we must use scaler, let's directly scale to NV12.
			if (m_OutputPixelFormat == AV_PIX_FMT_YUV420P || m_OutputPixelFormat == AV_PIX_FMT_YUVJ420P)
			{
				m_OutputPixelFormat = AV_PIX_FMT_NV12;
				OutputMediaSubtype = MediaEncodingSubtypes::Nv12;
			}
			m_bUseScaler = true;
		}
	}
	else
	{
		// Scaler required to convert pixel format
		m_bUseScaler = true;
	}

	DecoderWidth = width;
	DecoderHeight = height;
}

HRESULT UncompressedVideoSampleProvider::InitializeScalerIfRequired()
{
	HRESULT hr = S_OK;
	if (m_bUseScaler && !m_pSwsCtx)
	{
		// Setup software scaler to convert frame to output pixel type
		m_pSwsCtx = sws_getContext(
			m_pAvCodecCtx->width,
			m_pAvCodecCtx->height,
			m_pAvCodecCtx->pix_fmt,
			m_pAvCodecCtx->width,
			m_pAvCodecCtx->height,
			m_OutputPixelFormat,
			SWS_BICUBIC,
			NULL,
			NULL,
			NULL);

		if (m_pSwsCtx == nullptr)
		{
			hr = E_OUTOFMEMORY;
		}
	}

	return hr;
}

UncompressedVideoSampleProvider::~UncompressedVideoSampleProvider()
{
	if (m_pAvFrame)
	{
		av_frame_free(&m_pAvFrame);
	}

	if (m_pSwsCtx)
	{
		sws_freeContext(m_pSwsCtx);
	}

	if (m_pBufferPool)
	{
		av_buffer_pool_uninit(&m_pBufferPool);
	}

	if (m_pDirectBuffer)
	{
		m_pDirectBuffer = nullptr;
	}
}

HRESULT UncompressedVideoSampleProvider::DecodeAVPacket(DataWriter^ dataWriter, AVPacket* avPacket, int64_t& framePts, int64_t& frameDuration)
{
	HRESULT hr = S_OK;
	hr = UncompressedSampleProvider::DecodeAVPacket(dataWriter, avPacket, framePts, frameDuration);

	// Don't set a timestamp on S_FALSE
	if (hr == S_OK)
	{
		// Try to get the best effort timestamp for the frame.
		framePts = av_frame_get_best_effort_timestamp(m_pAvFrame);
		m_interlaced_frame = m_pAvFrame->interlaced_frame == 1;
		m_top_field_first = m_pAvFrame->top_field_first == 1;
	}

	return hr;
}

MediaStreamSample^ UncompressedVideoSampleProvider::GetNextSample()
{
	DebugMessage(L"GetNextSample\n");

	HRESULT hr = S_OK;

	MediaStreamSample^ sample;
	if (m_isEnabled)
	{
		LONGLONG pts = 0;
		LONGLONG dur = 0;

		hr = GetNextPacket(nullptr, pts, dur, true);

		if (hr == S_OK && !m_pDirectBuffer)
		{
			hr = E_FAIL;
		}

		if (hr == S_OK)
		{
			sample = MediaStreamSample::CreateFromBuffer(m_pDirectBuffer, { pts });
			sample->Duration = { dur };
			sample->Discontinuous = m_isDiscontinuous;
			if (m_interlaced_frame)
			{
				sample->ExtendedProperties->Insert(MFSampleExtension_Interlaced, TRUE);
				sample->ExtendedProperties->Insert(MFSampleExtension_BottomFieldFirst, m_top_field_first ? safe_cast<Platform::Object^>(FALSE) : TRUE);
				sample->ExtendedProperties->Insert(MFSampleExtension_RepeatFirstField, safe_cast<Platform::Object^>(FALSE));
			}
			else
			{
				sample->ExtendedProperties->Insert(MFSampleExtension_Interlaced, safe_cast<Platform::Object^>(FALSE));
			}
			m_isDiscontinuous = false;
			m_pDirectBuffer = nullptr;
		}
		else
		{
			DebugMessage(L"Too many broken packets - disable stream\n");
			DisableStream();
		}
	}

	return sample;
}

HRESULT UncompressedVideoSampleProvider::WriteAVPacketToStream(DataWriter^ dataWriter, AVPacket* avPacket)
{
	auto hr = InitializeScalerIfRequired();

	if (SUCCEEDED(hr))
	{
		if (!m_bUseScaler)
		{
			// Using direct buffer: just create a buffer reference to hand out to MSS pipeline
			auto bufferRef = av_buffer_ref(m_pAvFrame->buf[0]);
			if (bufferRef)
			{
				m_pDirectBuffer = NativeBufferFactory::CreateNativeBuffer(bufferRef->data, bufferRef->size, free_buffer, bufferRef);
			}
			else
			{
				hr = E_FAIL;
			}
		}
		else
		{
			// Using scaler: allocate a new frame from buffer pool
			auto frame = ref new FrameDataHolder();
			hr = FillLinesAndBuffer(frame->linesize, frame->data, &frame->buffer);
			if (SUCCEEDED(hr))
			{
				// Convert to output format using FFmpeg software scaler
				if (sws_scale(m_pSwsCtx, (const uint8_t **)(m_pAvFrame->data), m_pAvFrame->linesize, 0, m_pAvCodecCtx->height, frame->data, frame->linesize) > 0)
				{
					m_pDirectBuffer = NativeBufferFactory::CreateNativeBuffer(frame->buffer->data, frame->buffer->size, free_buffer, frame->buffer);
				}
				else
				{
					free_buffer(frame->buffer);
					hr = E_FAIL;
				}
			}
		}
	}

	av_frame_unref(m_pAvFrame);
	av_frame_free(&m_pAvFrame);

	return hr;
}

HRESULT UncompressedVideoSampleProvider::FillLinesAndBuffer(int* linesize, byte** data, AVBufferRef** buffer)
{
	if (av_image_fill_linesizes(linesize, m_OutputPixelFormat, DecoderWidth) < 0)
	{
		return E_FAIL;
	}

	auto YBufferSize = linesize[0] * DecoderHeight;
	auto UBufferSize = linesize[1] * DecoderHeight / 2;
	auto VBufferSize = linesize[2] * DecoderHeight / 2;
	auto totalSize = YBufferSize + UBufferSize + VBufferSize;

	buffer[0] = AllocateBuffer(totalSize);
	if (!buffer[0])
	{
		return E_OUTOFMEMORY;
	}

	data[0] = buffer[0]->data;
	data[1] = UBufferSize > 0 ? buffer[0]->data + YBufferSize : NULL;
	data[2] = VBufferSize > 0 ? buffer[0]->data + YBufferSize + UBufferSize : NULL;
	data[3] = NULL;

	return S_OK;
}

AVBufferRef* UncompressedVideoSampleProvider::AllocateBuffer(int totalSize)
{
	if (!m_pBufferPool)
	{
		m_pBufferPool = av_buffer_pool_init(totalSize, NULL);
		if (!m_pBufferPool)
		{
			return NULL;
		}
	}

	auto buffer = av_buffer_pool_get(m_pBufferPool);
	if (!buffer)
	{
		return NULL;
	}
	if (buffer->size != totalSize)
	{
		free_buffer(buffer);
		return NULL;
	}

	return buffer;
}

void UncompressedVideoSampleProvider::free_buffer(void *lpVoid)
{
	auto buffer = (AVBufferRef *)lpVoid;
	auto count = av_buffer_get_ref_count(buffer);
	av_buffer_unref(&buffer);
}

int UncompressedVideoSampleProvider::get_buffer2(AVCodecContext *avCodecContext, AVFrame *frame, int flags)
{
	// If frame size changes during playback and gets larger than our buffer, we need to switch to sws_scale
	auto provider = reinterpret_cast<UncompressedVideoSampleProvider^>(avCodecContext->opaque);
	provider->m_bUseScaler = frame->height > provider->DecoderHeight || frame->width > provider->DecoderWidth;
	if (provider->m_bUseScaler)
	{
		return avcodec_default_get_buffer2(avCodecContext, frame, flags);
	}
	else
	{
		return provider->FillLinesAndBuffer(frame->linesize, frame->data, frame->buf);
	}
}
