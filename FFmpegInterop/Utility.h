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
	inline std::string tolower(_Inout_ std::string str)
	{
		std::transform(str.begin(), str.end(), str.begin(),
			[](_In_ unsigned char c)
			{
				return static_cast<unsigned char>(std::tolower(c));
			});
		return str;
	}

	template <typename T, std::enable_if_t<std::is_convertible_v<T, std::string_view>, int> = 0>
	inline wil::unique_cotaskmem_string to_cotaskmem_string(_In_ const T& str)
	{
		const std::string_view strView{ str };
		const size_t originalSizeInBytes{ strView.size() * sizeof(char) };
		THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
			originalSizeInBytes > static_cast<size_t>(std::numeric_limits<int32_t>::max()));

		// Get the required buffer size
		int size{ MultiByteToWideChar(CP_UTF8, 0, strView.data(), static_cast<int32_t>(originalSizeInBytes), nullptr, 0) };
		THROW_LAST_ERROR_IF(size == 0);

		// Allocate the buffer and convert the string
		wil::unique_cotaskmem_string result{ wil::make_cotaskmem_string(nullptr, size) };
		int charsWritten{ MultiByteToWideChar(CP_UTF8, 0, strView.data(), static_cast<int32_t>(originalSizeInBytes), result.get(), size) };
		THROW_HR_IF(E_UNEXPECTED, charsWritten != size);

		return result;
	};

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

	// Helper function to map AVERROR to HRESULT
	inline constexpr HRESULT averror_to_hresult(_In_range_(< , 0) int status) noexcept
	{
		switch (status)
		{
		case AVERROR(EINVAL):
			return E_INVALIDARG;
		case AVERROR(ENOMEM):
			return E_OUTOFMEMORY;
		case AVERROR_BUFFER_TOO_SMALL:
			return MF_E_BUFFERTOOSMALL;
		case AVERROR_EOF:
			return MF_E_END_OF_STREAM;
		case AVERROR_INVALIDDATA:
			return MF_E_INVALID_FILE_FORMAT;
		default:
			{
				wil::FailureInfo info;
				if (wil::GetLastError(info, 0, status))
				{
					// Assume status is an HRESULT if it was traced by WIL
					return status;
				}

				return E_FAIL;
			}
		}
	}

	// Macro to check the result of FFmpeg calls
	template <typename T>
	_Post_satisfies_(return == status)
	inline constexpr int verify_averror(T status)
	{
		static_assert(std::is_same<T, int>::value, "Wrong Type: int expected");
		return status;
	}

	#define THROW_HR_IF_FFMPEG_FAILED(status) \
	do { \
		const int __status = verify_averror(status); \
		if (__status < 0) \
		{ \
			__pragma(warning(disable:4456)) \
		 	char buf[AV_ERROR_MAX_STRING_SIZE]{0}; \
			__pragma(warning(default:4456)) \
			FFMPEG_INTEROP_TRACE("FFmpeg failed: %hs", av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, __status)); \
			THROW_HR_MSG(averror_to_hresult(__status), #status); \
		} \
	} while (false) \

	// Helper function to create a PropertyValue from an MF attribute
	extern Windows::Foundation::IInspectable CreatePropValueFromMFAttribute(_In_ const PROPVARIANT& propVar);

	class MFCallbackBase :
		public implements<MFCallbackBase, IMFAsyncCallback>
	{
	public:
		MFCallbackBase(DWORD flags = 0, DWORD queue = MFASYNC_CALLBACK_QUEUE_MULTITHREADED) noexcept :
			m_flags(flags),
			m_queue(queue)
		{

		}

		DWORD GetQueue() const noexcept { return m_queue; }
		DWORD GetFlags() const noexcept { return m_flags; }

		// IMFAsyncCallback
		IFACEMETHODIMP GetParameters(_Out_ DWORD* flags, _Out_ DWORD* queue) noexcept
		{
			*flags = m_flags;
			*queue = m_queue;
			return S_OK;
		}

	private:
		DWORD m_flags{ 0 };
		DWORD m_queue{ 0 };
	};

	class MFWorkItem :
		public MFCallbackBase
	{
	public:
		MFWorkItem(
			_In_ std::move_only_function<void()> callback,
			_In_ DWORD flags = 0,
			_In_ DWORD queue = MFASYNC_CALLBACK_QUEUE_MULTITHREADED) : 
			MFCallbackBase(flags, queue),
			m_callback(std::move(callback))
		{

		}

		// IMFAsyncCallback
		IFACEMETHODIMP Invoke(_In_ IMFAsyncResult* result) noexcept override
		try
		{
			RETURN_IF_FAILED(result->GetStatus());
			m_callback();
			return S_OK;
		}
		CATCH_RETURN();

	private:
		std::move_only_function<void()> m_callback;
	};

	inline com_ptr<IMFAsyncResult> MFPutWorkItem(_In_ std::move_only_function<void()> callback)
	{
		auto workItem{ make_self<MFWorkItem>(std::move(callback)) };
		com_ptr<IMFAsyncResult> result;
		THROW_IF_FAILED(MFCreateAsyncResult(nullptr, workItem.get(), nullptr, result.put()));
		THROW_IF_FAILED(MFPutWorkItemEx2(workItem->GetQueue(), 0, result.get()));
		return result;
	}

	// Smart classes for managing MF objects
	template <typename T>
	class ShutdownWrapper
	{
	public:
		ShutdownWrapper(_In_ const ShutdownWrapper& other) = delete;
		ShutdownWrapper(_In_ ShutdownWrapper&& other) = default;

		ShutdownWrapper(_In_opt_ std::nullptr_t /*ptr*/ = nullptr) noexcept { }

		ShutdownWrapper(_In_opt_ T* ptr) noexcept :
			m_ptr(ptr)
		{

		}

		ShutdownWrapper(_In_ const com_ptr<T>& ptr) noexcept :
			m_ptr(ptr)
		{

		}

		ShutdownWrapper(_In_ com_ptr<T>&& ptr) noexcept :
			m_ptr(std::move(ptr))
		{

		}

		~ShutdownWrapper() noexcept
		{
			if (m_ptr != nullptr)
			{
				LOG_IF_FAILED(m_ptr->Shutdown());
			}
		}

		ShutdownWrapper& operator=(_In_ const ShutdownWrapper& other) = delete;
		ShutdownWrapper& operator=(_In_ ShutdownWrapper&& other) = default;

		ShutdownWrapper& operator=(_In_opt_ T* ptr) noexcept
		{
			m_ptr = ptr;
			return *this;
		}

		ShutdownWrapper& operator=(_In_ const com_ptr<T>& ptr) noexcept
		{
			m_ptr = ptr;
			return *this;
		}

		ShutdownWrapper& operator=(_In_ com_ptr<T>&& ptr) noexcept
		{
			m_ptr = std::move(ptr);
			return *this;
		}


		T* Get() const noexcept
		{
			return m_ptr.get();
		}

		T* Detach() noexcept
		{
			return m_ptr.detach();
		}

		void Reset() noexcept
		{
			m_ptr = nullptr;
		}

	private:
		com_ptr<T> m_ptr;
	};

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

	struct AVChannelLayoutWrapper :
		public AVChannelLayout
	{
		AVChannelLayoutWrapper() noexcept
		{
			order = AV_CHANNEL_ORDER_UNSPEC;
			nb_channels = 0;
			u.mask = 0;
			opaque = nullptr;
		}

		AVChannelLayoutWrapper(const AVChannelLayout& other) :
			AVChannelLayoutWrapper()
		{
			THROW_HR_IF_FFMPEG_FAILED(av_channel_layout_copy(this, &other));
		}

		AVChannelLayoutWrapper(const AVChannelLayoutWrapper& other) :
			AVChannelLayoutWrapper()
		{
			THROW_HR_IF_FFMPEG_FAILED(av_channel_layout_copy(this, &other));
		}

		AVChannelLayoutWrapper(AVChannelLayoutWrapper&& other) noexcept :
			AVChannelLayoutWrapper()
		{
			*this = std::move(other);
		}

		AVChannelLayoutWrapper(int channels) noexcept :
			AVChannelLayoutWrapper()
		{
			av_channel_layout_default(this, channels);
		}

		AVChannelLayoutWrapper(uint64_t mask) :
			AVChannelLayoutWrapper()
		{
			THROW_HR_IF_FFMPEG_FAILED(av_channel_layout_from_mask(this, mask));
		}

		~AVChannelLayoutWrapper() noexcept
		{
			av_channel_layout_uninit(this);
		}

		AVChannelLayoutWrapper& operator=(const AVChannelLayout& other)
		{
			if (this != &other)
			{
				THROW_HR_IF_FFMPEG_FAILED(av_channel_layout_copy(this, &other));
			}
			return *this;
		}

		AVChannelLayoutWrapper& operator=(AVChannelLayoutWrapper&& other) noexcept
		{
			if (this != &other)
			{
				std::swap(*this, other);
			}
			return *this;
		}
	};

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
