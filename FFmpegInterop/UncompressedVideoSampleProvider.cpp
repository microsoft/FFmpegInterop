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

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	UncompressedVideoSampleProvider::UncompressedVideoSampleProvider(_In_ const AVStream* stream, _Inout_ FFmpegReader& reader) :
		UncompressedSampleProvider(stream, reader)
	{
		if (m_codecContext->pix_fmt != AV_PIX_FMT_NV12)
		{
			// Setup software scaler to convert the pixel format to NV12
			m_swsContext.reset(sws_getContext(
				m_codecContext->width,
				m_codecContext->height,
				m_codecContext->pix_fmt,
				m_codecContext->width,
				m_codecContext->height,
				AV_PIX_FMT_NV12,
				SWS_BICUBIC,
				nullptr,
				nullptr,
				nullptr));
			THROW_IF_NULL_ALLOC(m_swsContext);

			THROW_IF_FFMPEG_FAILED(av_image_fill_linesizes(m_lineSizes, AV_PIX_FMT_NV12, m_codecContext->width));

			// Create a buffer pool
			const int requiredBufferSize = av_image_get_buffer_size(AV_PIX_FMT_NV12, m_codecContext->width, m_codecContext->height, 1);
			THROW_IF_FFMPEG_FAILED(requiredBufferSize);

			m_bufferPool.reset(av_buffer_pool_init(requiredBufferSize, nullptr));
			THROW_IF_NULL_ALLOC(m_bufferPool);
		}
	}

	tuple<IBuffer, int64_t, int64_t> UncompressedVideoSampleProvider::GetSampleData()
	{
		GetFrame();

		if (m_swsContext == nullptr)
		{
			return { make<FFmpegBuffer>(m_frame->buf[0]), m_frame->best_effort_timestamp, m_frame->pkt_duration };
		}
		else
		{
			// Scale the image to the desired output format
			AVBufferRef_ptr bufferRef{ av_buffer_pool_get(m_bufferPool.get()) };
			THROW_IF_NULL_ALLOC(bufferRef);

			uint8_t* data[4]{ };
			const int requiredBufferSize = av_image_fill_pointers(data, AV_PIX_FMT_NV12, m_codecContext->height, bufferRef->data, m_lineSizes);
			THROW_IF_FFMPEG_FAILED(requiredBufferSize);
			THROW_HR_IF(MF_E_UNEXPECTED, requiredBufferSize != bufferRef->size);

			THROW_IF_FFMPEG_FAILED(sws_scale(m_swsContext.get(), m_frame->data, m_frame->linesize, 0, m_codecContext->height, data, m_lineSizes));

			return { make<FFmpegBuffer>(move(bufferRef)), m_frame->best_effort_timestamp, m_frame->pkt_duration };
		}
	}

	void UncompressedVideoSampleProvider::SetSampleProperties(const MediaStreamSample& sample)
	{
		MediaStreamSamplePropertySet propertySet = sample.ExtendedProperties();

		if (m_frame->interlaced_frame)
		{
			propertySet.Insert(MFSampleExtension_Interlaced, PropertyValue::CreateBoolean(true));
			propertySet.Insert(MFSampleExtension_BottomFieldFirst, PropertyValue::CreateBoolean(!m_frame->top_field_first));
			propertySet.Insert(MFSampleExtension_RepeatFirstField, PropertyValue::CreateBoolean(false));
		}
		else
		{
			propertySet.Insert(MFSampleExtension_Interlaced, PropertyValue::CreateBoolean(false));
		}

		av_frame_unref(m_frame.get()); // We no longer need the current frame
	}
}
