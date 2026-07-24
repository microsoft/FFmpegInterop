//*****************************************************************************
//
//	Copyright 2023 Microsoft Corporation
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
#include "FFmpegInteropByteStreamHandler.h"
#include "FFmpegInteropByteStreamHandler.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Storage::Streams;

namespace
{
	// {54a658c4-fa41-4ff5-9531-1cc79259198c}
	constexpr GUID FFMPEGINTEROP_FACTORY_CACHE_RESET
	{
		0x54a658c4, 0xfa41, 0x4ff5, { 0x95, 0x31, 0x1c, 0xc7, 0x92, 0x59, 0x19, 0x8c }
	};

	struct FactoryCacheReset : winrt::implements<FactoryCacheReset, ::IUnknown>
	{
		~FactoryCacheReset()
		{
			winrt::clear_factory_cache();
		}

		static winrt::com_ptr<FactoryCacheReset> GetInstance()
		{
			static std::mutex mutex;
			const std::lock_guard lock{ mutex };

			if (auto instance{ s_weakInstance.get() })
			{
				return instance;
			}

			auto instance{ winrt::make_self<FactoryCacheReset>() };
			s_weakInstance = instance;
			return instance;
		}

		inline static winrt::weak_ref<FactoryCacheReset> s_weakInstance;
	};
}

namespace winrt::FFmpegInterop::implementation
{
	FFmpegInteropByteStreamHandler::FFmpegInteropByteStreamHandler()
	{
		THROW_IF_FAILED(SetUINT32(MF_BYTESTREAMHANDLER_ACCEPTS_SHARE_WRITE, 1));
	}

	IFACEMETHODIMP FFmpegInteropByteStreamHandler::BeginCreateObject(
		_In_ IMFByteStream* pByteStream,
		_In_opt_ LPCWSTR /*pURL*/,
		_In_ DWORD dwFlags,
		_In_opt_ IPropertyStore* /*pPropertyStore*/,
		_COM_Outptr_opt_ ::IUnknown** ppCancelCookie,
		_In_ IMFAsyncCallback* pCallback,
		_In_opt_ ::IUnknown* pState) noexcept
	try
	{
		auto logger{ FFmpegInteropProvider::BeginCreateObject::Start() };

		if (ppCancelCookie != nullptr)
		{
			*ppCancelCookie = nullptr;
		}

		RETURN_HR_IF_NULL(E_INVALIDARG, pByteStream);
		RETURN_HR_IF_NULL(E_INVALIDARG, pCallback);

		// Verify the byte stream is readable
		DWORD dwCapabilities{ 0 };
		RETURN_IF_FAILED(pByteStream->GetCapabilities(&dwCapabilities));
		RETURN_HR_IF(E_INVALIDARG, (dwCapabilities & MFBYTESTREAM_IS_READABLE) == 0);

		// Verify we were asked for a media source
		RETURN_HR_IF(E_INVALIDARG, (dwFlags & 0xF) != MF_RESOLUTION_MEDIASOURCE);

		// Queue a work item to create the media source
		auto state{ make_self<FFmpegInteropByteStreamHandlerState>(pByteStream) };
		com_ptr<IMFAsyncResult> result;
		RETURN_IF_FAILED(MFCreateAsyncResult(state.get(), pCallback, pState, result.put()));

		auto cancelCookie{ MFPutWorkItem([
			strong_this{ get_strong() },
			result{ std::move(result) }]()
			{
				auto invokeCallback{ wil::scope_exit([&result]()
				{
					LOG_IF_FAILED(MFInvokeCallback(result.get()));
				}) };

				strong_this->CreateMediaSource(result.get());
			}) };

		if (ppCancelCookie != nullptr)
		{
			*ppCancelCookie = cancelCookie.detach();
		}

		logger.Stop();
		return S_OK;
	}
	CATCH_RETURN();

	void FFmpegInteropByteStreamHandler::CreateMediaSource(_In_ IMFAsyncResult* result)
	try
	{
		auto logger{ FFmpegInteropProvider::CreateMediaSource::Start() };

		com_ptr<::IUnknown> object;
		THROW_IF_FAILED(result->GetObject(object.put()));
		auto state{ get_self<FFmpegInteropByteStreamHandlerState>(object.as<IFFmpegInteropByteStreamHandlerStateMarker>()) };

		// Wrap the byte stream in a proxy to prevent it from being closed if we fail to create and initialize the MSS,
		// so that the source resolver can rollover and attempt other byte stream handlers.
		auto byteStreamProxy{ make_self<ByteStreamProxy>(std::move(state->m_byteStream)) };

		// Wrap the byte stream into a random access stream
		IRandomAccessStream stream{ nullptr };
		THROW_IF_FAILED(MFCreateStreamOnMFByteStreamEx(byteStreamProxy.get(), guid_of<decltype(stream)>(), put_abi(stream)));

		// Create the MSS via its activation factory since its constructors don't accept nullptr
		IActivationFactory mssFactory{ get_activation_factory<MediaStreamSource>() };
		MediaStreamSource mss{ mssFactory.ActivateInstance<MediaStreamSource>() };

		com_ptr<IMFMediaSource> mediaSource;
		THROW_IF_FAILED(mss.as<IMFGetService>()->GetService(MF_MEDIASOURCE_SERVICE, __uuidof(mediaSource), mediaSource.put_void()));

		// Add a FactoryCacheReset reference to the media source's attribute store so that FFmpegInterop.dll's C++/WinRT
		// factory cache is cleared when the last MSS is destroyed. This prevents cached Windows.Media.dll activation
		// factories from dangling if Windows.Media.dll is unloaded.
		THROW_IF_FAILED(mediaSource.as<IMFAttributes>()->SetUnknown(FFMPEGINTEROP_FACTORY_CACHE_RESET, FactoryCacheReset::GetInstance().get()));

		FFmpegInteropMSS::InitializeFromStream(stream, mss, nullptr);

		// After initialization, the MSS and FFmpegInteropMSS have circular references on each other that need to be
		// broken by calling Shutdown() on the MSS's IMFMediaSource.
		state->m_mediaSource = ShutdownWrapper<IMFMediaSource>{ std::move(mediaSource) };

		// Allow the byte stream to be closed when the media source shuts down
		byteStreamProxy->AllowClosing(true);

		logger.Stop();
	}
	catch (...)
	{
		THROW_IF_FAILED(result->SetStatus(to_hresult()));
	}

	IFACEMETHODIMP FFmpegInteropByteStreamHandler::EndCreateObject(
		_In_ IMFAsyncResult* pResult,
		_Out_ MF_OBJECT_TYPE* pObjectType,
		_COM_Outptr_ ::IUnknown** ppObject) noexcept
	try
	{
		auto logger{ FFmpegInteropProvider::EndCreateObject::Start() };

		if (pObjectType != nullptr)
		{
			*pObjectType = MF_OBJECT_INVALID;
		}

		if (ppObject != nullptr)
		{
			*ppObject = nullptr;
		}

		RETURN_HR_IF_NULL(E_INVALIDARG, pResult);
		RETURN_HR_IF_NULL(E_INVALIDARG, pObjectType);
		RETURN_HR_IF_NULL(E_POINTER, ppObject);

		com_ptr<::IUnknown> object;
		THROW_IF_FAILED(pResult->GetObject(object.put()));
		auto state{ get_self<FFmpegInteropByteStreamHandlerState>(object.try_as<IFFmpegInteropByteStreamHandlerStateMarker>()) };
		RETURN_HR_IF_NULL(E_INVALIDARG, state);

		RETURN_IF_FAILED(pResult->GetStatus());

		*ppObject = state->m_mediaSource.Detach();
		*pObjectType = MF_OBJECT_MEDIASOURCE;

		logger.Stop();
		return S_OK;
	}
	CATCH_RETURN();

	IFACEMETHODIMP FFmpegInteropByteStreamHandler::CancelObjectCreation(_In_ ::IUnknown* pIUnknownCancelCookie) noexcept
	try
	{
		auto logger{ FFmpegInteropProvider::CancelObjectCreation::Start() };

		RETURN_HR_IF_NULL(E_INVALIDARG, pIUnknownCancelCookie);

		com_ptr<IMFAsyncResult> result;
		RETURN_IF_FAILED(pIUnknownCancelCookie->QueryInterface(result.put()));
		RETURN_IF_FAILED(result->SetStatus(MF_E_OPERATION_CANCELLED));

		logger.Stop();
		return S_OK;
	}
	CATCH_RETURN();

	IFACEMETHODIMP FFmpegInteropByteStreamHandler::GetMaxNumberOfBytesRequiredForResolution(_Out_ QWORD* /*pcb*/) noexcept
	try
	{
		RETURN_HR(E_NOTIMPL);
	}
	CATCH_RETURN();
}
