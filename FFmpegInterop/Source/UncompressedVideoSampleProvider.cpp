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
#include <mfapi.h>

extern "C"
{
#include <libavutil/imgutils.h>
}


using namespace FFmpegInterop;
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

	auto width = avCodecCtx->width;
	auto height = avCodecCtx->height;

	// if no scaler is used, check decoder frame size
	// we can ignore height changes because we just copy the required number of lines to output buffer
	if (m_pAvCodecCtx->pix_fmt == m_OutputPixelFormat)
	{
		avcodec_align_dimensions(m_pAvCodecCtx, &width, &height);
		height = m_pAvCodecCtx->height; 
	}

	DecoderWidth = width;
	DecoderHeight = height;
}


HRESULT UncompressedVideoSampleProvider::InitializeScalerIfRequired(AVFrame *frame)
{
	HRESULT hr = S_OK;
	if (!m_bIsInitialized)
	{
		m_bIsInitialized = true;
		bool needsScaler = m_pAvCodecCtx->pix_fmt != m_OutputPixelFormat;
		if (!needsScaler)
		{
			// check if actual frame size has changed from expected decoder size
			auto width = frame->width;
			auto height = frame->height;
			avcodec_align_dimensions(m_pAvCodecCtx, &width, &height);
			if (width != DecoderWidth)
			{
				needsScaler = true;
			}
		}
		if (needsScaler)
		{
			// Setup software scaler to convert any unsupported decoder pixel format to NV12 that is supported in Windows & Windows Phone MediaElement
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

			if (SUCCEEDED(hr))
			{
				if (av_image_alloc(m_rgVideoBufferData, m_rgVideoBufferLineSize, m_pAvCodecCtx->width, m_pAvCodecCtx->height, m_OutputPixelFormat, 1) < 0)
				{
					hr = E_FAIL;
				}
			}
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

	if (m_rgVideoBufferData)
	{
		av_freep(m_rgVideoBufferData);
	}

	if (m_pSwsCtx)
	{
		sws_freeContext(m_pSwsCtx);
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
	MediaStreamSample^ sample = MediaSampleProvider::GetNextSample();

	if (sample != nullptr)
	{
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
	}

	return sample;
}

HRESULT UncompressedVideoSampleProvider::WriteAVPacketToStream(DataWriter^ dataWriter, AVPacket* avPacket)
{
	InitializeScalerIfRequired(m_pAvFrame);

	if (m_pSwsCtx == nullptr)
	{
		// ffmpeg does not allocate contiguous buffers for YUV, so we need to manually copy all three planes
		auto YBuffer = Platform::ArrayReference<uint8_t>(m_pAvFrame->data[0], m_pAvFrame->linesize[0] * m_pAvCodecCtx->height);
		auto UBuffer = Platform::ArrayReference<uint8_t>(m_pAvFrame->data[1], m_pAvFrame->linesize[1] * m_pAvCodecCtx->height / 2);
		auto VBuffer = Platform::ArrayReference<uint8_t>(m_pAvFrame->data[2], m_pAvFrame->linesize[2] * m_pAvCodecCtx->height / 2);
		dataWriter->WriteBytes(YBuffer);
		dataWriter->WriteBytes(UBuffer);
		dataWriter->WriteBytes(VBuffer);
	}
	else
	{
		// Convert decoded video pixel format to output format using FFmpeg software scaler
		if (sws_scale(m_pSwsCtx, (const uint8_t **)(m_pAvFrame->data), m_pAvFrame->linesize, 0, m_pAvCodecCtx->height, m_rgVideoBufferData, m_rgVideoBufferLineSize) < 0)
		{
			return E_FAIL;
		}

		// we allocate a contiguous buffer for sws_scale, so we do not have to copy YUV planes separately
		auto size = 
			(m_rgVideoBufferLineSize[0] * m_pAvCodecCtx->height) + 
			(m_rgVideoBufferLineSize[1] * m_pAvCodecCtx->height / 2) + 
			(m_rgVideoBufferLineSize[2] * m_pAvCodecCtx->height / 2);
		auto buffer = Platform::ArrayReference<uint8_t>(m_rgVideoBufferData[0], size);
		dataWriter->WriteBytes(buffer);
	}

	av_frame_unref(m_pAvFrame);
	av_frame_free(&m_pAvFrame);

	return S_OK;
}
