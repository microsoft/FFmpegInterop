//*****************************************************************************
//
//	Copyright 2019 Microsoft Corporation
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

namespace std
{
	// Less than comparator for using GUID as std::map key
	template<> struct less<GUID>
	{
		bool operator() (_In_ const GUID& lhs, _In_ const GUID& rhs) const
		{
			auto a = reinterpret_cast<const unsigned long*>(&lhs);
			auto b = reinterpret_cast<const unsigned long*>(&rhs);

			if (a[0] != b[0])
			{
				return a[0] < b[0];
			}
			else if (a[1] != b[1])
			{
				return a[1] < b[1];
			}
			else if (a[2] != b[2])
			{
				return a[2] < b[2];
			}
			else if (a[3] != b[3])
			{
				return a[3] < b[3];
			}
			else
			{
				return false;
			}
		}
	};
}

namespace winrt::FFmpegInterop::implementation
{
	constexpr uint8_t BITS_PER_BYTE{ 8 };
	constexpr int64_t MS_PER_SEC{ 1000 };
	constexpr int64_t HNS_PER_SEC{ 10000000 };

	// Convert arbitrary units to AV time
	inline int64_t ConvertToAVTime(_In_ int64_t units, _In_ int64_t unitsPerSec, _In_ AVRational avTimeBase)
	{
		return static_cast<int64_t>(units / (av_q2d(avTimeBase) * unitsPerSec));
	}

	// Convert to arbitrary units from AV time
	inline int64_t ConvertFromAVTime(_In_ int64_t avTime, _In_ AVRational avTimeBase, _In_ int64_t unitsPerSec)
	{
		return static_cast<int64_t>(avTime *  av_q2d(avTimeBase) * unitsPerSec);
	}

	// Map of AVERROR -> HRESULT
	const std::map<int, HRESULT> c_errorCodeMap
	{
		{ AVERROR(EINVAL), E_INVALIDARG },
		{ AVERROR(ENOMEM), E_OUTOFMEMORY },
		{ AVERROR_BUFFER_TOO_SMALL, MF_E_BUFFERTOOSMALL },
		{ AVERROR_EOF, MF_E_END_OF_STREAM }
	};

	// Helper function to map AVERROR to HRESULT
	inline HRESULT averror_to_hresult(_In_range_(< , 0) int result)
	{
		auto iter{ c_errorCodeMap.find(result) };
		return (iter != c_errorCodeMap.end()) ? iter->second : E_FAIL;
	}

	// Macro to check the result of FFmpeg calls
	#define THROW_HR_IF_FFMPEG_FAILED(result) if ((result) < 0) { THROW_HR(averror_to_hresult(result)); }

	// Helper function to create a PropertyValue from an MF attribute
	extern winrt::Windows::Foundation::IInspectable CreatePropValueFromMFAttribute(_In_ const PROPVARIANT& propVar);

	// Smart classes for managing FFmpeg objects
	struct AVBlobDeleter
	{
		void operator()(_In_opt_ void* blob)
		{
			av_freep(&blob);
		}
	};
	typedef std::unique_ptr<void, AVBlobDeleter> AVBlob_ptr;

	struct AVBufferPoolDeleter
	{
		void operator()(_In_opt_ AVBufferPool* bufferPool)
		{
			av_buffer_pool_uninit(&bufferPool);
		}
	};
	typedef std::unique_ptr<AVBufferPool, AVBufferPoolDeleter> AVBufferPool_ptr;

	struct AVBufferRefDeleter
	{
		void operator()(_In_opt_ AVBufferRef* bufferRef)
		{
			av_buffer_unref(&bufferRef);
		}
	};
	typedef std::unique_ptr<AVBufferRef, AVBufferRefDeleter> AVBufferRef_ptr;

	struct AVDictionaryDeleter
	{
		void operator()(_In_opt_ AVDictionary* dict)
		{
			av_dict_free(&dict);
		}
	};
	typedef std::unique_ptr<AVDictionary, AVDictionaryDeleter> AVDictionary_ptr;

	struct AVIOContextDeleter
	{
		void operator()(_In_opt_ AVIOContext* ioContext)
		{
			if (ioContext != nullptr)
			{
				av_free(ioContext->buffer);
				avio_context_free(&ioContext);
			}
		}
	};
	typedef std::unique_ptr<AVIOContext, AVIOContextDeleter> AVIOContext_ptr;

	struct AVFormatContextDeleter
	{
		void operator()(_In_opt_ AVFormatContext* formatContext)
		{
			avformat_close_input(&formatContext);
		}
	};
	typedef std::unique_ptr<AVFormatContext, AVFormatContextDeleter> AVFormatContext_ptr;

	struct AVCodecContextDeleter
	{
		void operator()(_In_opt_ AVCodecContext* codecContext)
		{
			avcodec_free_context(&codecContext);
		}
	};
	typedef std::unique_ptr<AVCodecContext, AVCodecContextDeleter> AVCodecContext_ptr;

	struct AVCodecParametersDeleter
	{
		void operator()(_In_opt_ AVCodecParameters* codecParams)
		{
			avcodec_parameters_free(&codecParams);
		}
	};
	typedef std::unique_ptr<AVCodecParameters, AVCodecParametersDeleter> AVCodecParameters_ptr;

	struct AVPacketDeleter
	{
		void operator()(_In_opt_ AVPacket* packet)
		{
			av_packet_free(&packet);
		}
	};
	typedef std::unique_ptr<AVPacket, AVPacketDeleter> AVPacket_ptr;

	struct AVFrameDeleter
	{
		void operator()(_In_opt_ AVFrame* frame)
		{
			av_frame_free(&frame);
		}
	};
	typedef std::unique_ptr<AVFrame, AVFrameDeleter> AVFrame_ptr;

	struct SwrContextDeleter
	{
		void operator()(_In_opt_ SwrContext* swrContext)
		{
			swr_free(&swrContext);
		}
	};
	typedef std::unique_ptr<SwrContext, SwrContextDeleter> SwrContext_ptr;

	struct SwsContextDeleter
	{
		void operator()(_In_opt_ SwsContext* swsContext)
		{
			sws_freeContext(swsContext);
		}
	};
	typedef std::unique_ptr<SwsContext, SwsContextDeleter> SwsContext_ptr;
}
