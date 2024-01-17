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
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	UncompressedVideoSampleProvider::UncompressedVideoSampleProvider(_In_ const AVFormatContext* formatContext, _In_ AVStream* stream, _In_ Reader& reader, _In_ uint32_t allowedDecodeErrors) :
		UncompressedSampleProvider(formatContext, stream, reader, allowedDecodeErrors),
		m_outputWidth(m_codecContext->width),
		m_outputHeight(m_codecContext->height)
	{
		if (m_codecContext->pix_fmt != AV_PIX_FMT_NV12)
		{
			InitScaler();
		}
	}

	void UncompressedVideoSampleProvider::InitScaler()
	{
		// Setup software scaler to convert the pixel format to NV12
		m_swsContext.reset(sws_getContext(
			m_outputWidth,
			m_outputHeight,
			m_codecContext->pix_fmt,
			m_outputWidth,
			m_outputHeight,
			AV_PIX_FMT_NV12,
			SWS_BICUBIC,
			nullptr,
			nullptr,
			nullptr));
		THROW_IF_NULL_ALLOC(m_swsContext);

		THROW_HR_IF_FFMPEG_FAILED(av_image_fill_linesizes(m_lineSizes, AV_PIX_FMT_NV12, m_outputWidth));

		// Create a buffer pool
		const int requiredBufferSize{ av_image_get_buffer_size(AV_PIX_FMT_NV12, m_outputWidth, m_outputHeight, 1) };
		THROW_HR_IF_FFMPEG_FAILED(requiredBufferSize);

		m_bufferPool.reset(av_buffer_pool_init(requiredBufferSize, nullptr));
		THROW_IF_NULL_ALLOC(m_bufferPool);
	}

	void UncompressedVideoSampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
	{
		SampleProvider::SetEncodingProperties(encProp, setFormatUserData);

		VideoEncodingProperties videoEncProp{ encProp.as<VideoEncodingProperties>() };

		if (m_codecContext->framerate.num != 0 && m_codecContext->framerate.den != 0)
		{
			MediaRatio frameRate{ videoEncProp.FrameRate() };
			frameRate.Numerator(m_codecContext->framerate.num);
			frameRate.Denominator(m_codecContext->framerate.den);
		}

		MediaPropertySet videoProp{ videoEncProp.Properties() };
		videoProp.Insert(MF_MT_INTERLACE_MODE, PropertyValue::CreateUInt32(MFVideoInterlace_MixedInterlaceOrProgressive));
	}

	tuple<IBuffer, int64_t, int64_t, vector<pair<GUID, Windows::Foundation::IInspectable>>, vector<pair<GUID, Windows::Foundation::IInspectable>>> UncompressedVideoSampleProvider::GetSampleData()
	{
		// Get the next decoded sample
		AVFrame_ptr frame;
		uint32_t decodeErrors{ 0 };

		while (true)
		{
			try
			{
				frame = GetFrame();
				break;
			}
			catch (...)
			{
				const hresult hr{ to_hresult() };
				switch (hr)
				{
				case MF_E_END_OF_STREAM: // We've reached EOF. Nothing more to do.
				case E_OUTOFMEMORY: // Always treat as fatal error
					throw;

				default:
					// Unexpected decode error
					if (decodeErrors < m_allowedDecodeErrors)
					{
						decodeErrors++;
						FFMPEG_INTEROP_TRACE("Stream %d: Decode error. Total decoder errors = %d, Limit = %d",
							m_stream->index, decodeErrors, m_allowedDecodeErrors);

						m_isDiscontinuous = true;
					}
					else
					{
						throw;
					}

					break;
				}
			}
		}

		// Check for dynamic format changes
		vector<pair<GUID, Windows::Foundation::IInspectable>> formatChanges{ CheckForFormatChanges(frame.get()) };

		// Get the sample buffer
		IBuffer sampleBuf{ nullptr };
		if (m_swsContext == nullptr)
		{
			// Image is already in the desired output format
			sampleBuf = make<FFmpegInteropBuffer>(frame->buf[0]);
		}
		else
		{
			// Scale the image to the desired output format
			AVBufferRef_ptr bufferRef{ av_buffer_pool_get(m_bufferPool.get()) };
			THROW_IF_NULL_ALLOC(bufferRef);

			uint8_t* data[4]{ };
			const int requiredBufferSize{ av_image_fill_pointers(data, AV_PIX_FMT_NV12, frame->height, bufferRef->data, m_lineSizes) };
			THROW_HR_IF_FFMPEG_FAILED(requiredBufferSize);
			THROW_HR_IF(MF_E_UNEXPECTED, requiredBufferSize != bufferRef->size);

			THROW_HR_IF_FFMPEG_FAILED(sws_scale(m_swsContext.get(), frame->data, frame->linesize, 0, frame->height, data, m_lineSizes));

			sampleBuf = make<FFmpegInteropBuffer>(move(bufferRef));
		}

		// Get the sample properties
		vector<pair<GUID, Windows::Foundation::IInspectable>> properties{ GetSampleProperties(frame.get()) };

		return { move(sampleBuf), frame->best_effort_timestamp, frame->duration, move(properties), move(formatChanges) };
	}

	vector<pair<GUID, Windows::Foundation::IInspectable>> UncompressedVideoSampleProvider::CheckForFormatChanges(_In_ const AVFrame* frame)
	{
		vector<pair<GUID, Windows::Foundation::IInspectable>> formatChanges;

		// Check if the resolution changed
		if (frame->width != m_outputWidth || frame->height != m_outputHeight)
		{
			FFMPEG_INTEROP_TRACE("Stream %d: Resolution change. Old Width = %d, Old Height = %d, New Width = %d, New Height = %d",
				m_stream->index, m_outputWidth, m_outputHeight, frame->width, frame->height);

			m_outputWidth = frame->width;
			m_outputHeight = frame->height;
			formatChanges.emplace_back(MF_MT_FRAME_SIZE, PropertyValue::CreateUInt64(Pack2UINT32AsUINT64(m_outputWidth, m_outputHeight)));

			if (m_swsContext != nullptr)
			{
				InitScaler();
			}
		}

		return formatChanges;
	}

	vector<pair<GUID, Windows::Foundation::IInspectable>> UncompressedVideoSampleProvider::GetSampleProperties(_In_ const AVFrame* frame)
	{
		vector<pair<GUID, Windows::Foundation::IInspectable>> properties;

		if ((frame->flags & AV_FRAME_FLAG_INTERLACED) != 0)
		{
			properties.emplace_back(MFSampleExtension_Interlaced, PropertyValue::CreateUInt32(true));
			properties.emplace_back(MFSampleExtension_BottomFieldFirst, PropertyValue::CreateUInt32((frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST) == 0));
			properties.emplace_back(MFSampleExtension_RepeatFirstField, PropertyValue::CreateUInt32(false));
		}
		else
		{
			properties.emplace_back(MFSampleExtension_Interlaced, PropertyValue::CreateUInt32(false));
		}

		return properties;
	}
}
