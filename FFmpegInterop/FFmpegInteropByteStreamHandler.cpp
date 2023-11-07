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
#include "FFmpegInteropByteStreamHandler.h"
#include "FFmpegInteropByteStreamHandler.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::FFmpegInterop::implementation
{
    FFmpegInteropByteStreamHandler::FFmpegInteropByteStreamHandler()
    {
        // https://learn.microsoft.com/en-us/windows/win32/medfound/mf-bytestreamhandler-accepts-share-write#remarks
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
        com_ptr<IMFByteStream> byteStream;
        byteStream.copy_from(pByteStream);

        com_ptr<IMFAsyncResult> result;
        RETURN_IF_FAILED(MFCreateAsyncResult(pByteStream, pCallback, pState, result.put()));

        auto cancelCookie{ MFPutWorkItem([
            strong_this{ get_strong() },
            byteStream = std::move(byteStream),
            result = std::move(result)]()
            {
                strong_this->CreateMediaSource(byteStream.get(), result.get());
            }) };

        if (ppCancelCookie != nullptr)
        {
            *ppCancelCookie = cancelCookie.detach();
        }

		return S_OK;
	}
    CATCH_RETURN();

    void FFmpegInteropByteStreamHandler::CreateMediaSource(_In_ IMFByteStream* byteStream, _In_ IMFAsyncResult* result)
    {
        // Wrap the byte stream
        IRandomAccessStream stream{ nullptr };
        THROW_IF_FAILED(MFCreateStreamOnMFByteStreamEx(byteStream, guid_of<decltype(stream)>(), put_abi(stream)));

        // Create the MSS via its activation factory since its constructors don't accept nullptr
        IActivationFactory mssFactory{ get_activation_factory<MediaStreamSource>() };
		MediaStreamSource mss{ mssFactory.ActivateInstance<MediaStreamSource>() };

        FFmpegInteropMSS::InitFromStream(stream, mss, nullptr);

        // We need to take care handling the MSS after this point. The MSS and FFmpegInteropMSS have circular
        // references on each other that need to be broken by calling Shutdown() on the MSS's IMFMediaSource.
        // Getting the MSS's IMFMediaSource can't fail, but theoretically if it did the MSS and FFmpegInteropMSS would leak.
        com_ptr<IMFMediaSource> mediaSource;
		THROW_IF_FAILED(mss.as<IMFGetService>()->GetService(MF_MEDIASOURCE_SERVICE, __uuidof(mediaSource), mediaSource.put_void()));

        // Store a mapping of the result to the media source.
        // EndCreateObject() will use this mapping to return the media source to the caller.
        // If for some reason ownership of the media source is never transferred to the caller,
        // then during destruction the media source will be shutdown to prevent a leak.
        m_map[result] = ShutdownWrapper<IMFMediaSource>{ std::move(mediaSource) };

        THROW_IF_FAILED(MFInvokeCallback(result));
    }

    IFACEMETHODIMP FFmpegInteropByteStreamHandler::EndCreateObject(
        _In_ IMFAsyncResult* pResult,
        _Out_ MF_OBJECT_TYPE* pObjectType,
        _COM_Outptr_ ::IUnknown** ppObject) noexcept
	try
	{
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

        // Get the media source
        auto iter{ m_map.find(pResult) };
        RETURN_HR_IF(MF_E_INVALIDREQUEST, iter == m_map.end());
        *ppObject = iter->second.Detach();
        *pObjectType = MF_OBJECT_MEDIASOURCE;

        // The caller is now responsible for shutting down the media source
        m_map.erase(iter);

		return S_OK;
	}
    CATCH_RETURN();

    IFACEMETHODIMP FFmpegInteropByteStreamHandler::CancelObjectCreation(_In_ ::IUnknown* pIUnknownCancelCookie) noexcept
	try
	{
        RETURN_HR_IF_NULL(E_INVALIDARG, pIUnknownCancelCookie);

        com_ptr<IMFAsyncResult> result;
        RETURN_IF_FAILED(pIUnknownCancelCookie->QueryInterface(result.put()));
        RETURN_IF_FAILED(result->SetStatus(MF_E_OPERATION_CANCELLED));

		return S_OK;
	}
    CATCH_RETURN();

    IFACEMETHODIMP FFmpegInteropByteStreamHandler::GetMaxNumberOfBytesRequiredForResolution(_Out_ QWORD* pcb) noexcept
	try
	{
		RETURN_HR(E_NOTIMPL);
	}
    CATCH_RETURN();
}
