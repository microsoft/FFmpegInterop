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

        auto mediaSource{ CreateMediaSource(pByteStream) };

        com_ptr<IMFAsyncResult> result;
        RETURN_IF_FAILED(MFCreateAsyncResult(mediaSource.Get(), pCallback, pState, result.put()));
        RETURN_IF_FAILED(MFPutWorkItemEx2(MFASYNC_CALLBACK_QUEUE_MULTITHREADED, 0, result.get()));

        if (ppCancelCookie != nullptr)
        {
            *ppCancelCookie = result.detach();
        }

        mediaSource.Reset(); // Don't shutdown the media source
		return S_OK;
	}
    CATCH_RETURN();

    ShutdownWrapper<IMFMediaSource> FFmpegInteropByteStreamHandler::CreateMediaSource(_In_ IMFByteStream* pByteStream)
    {
        // Wrap the byte stream
        IRandomAccessStream stream{ nullptr };
        THROW_IF_FAILED(MFCreateStreamOnMFByteStreamEx(pByteStream, guid_of<decltype(stream)>(), put_abi(stream)));

        // Create the MSS via its activation factory since its constructors don't accept nullptr
        IActivationFactory mssFactory{ get_activation_factory<MediaStreamSource>() };
		MediaStreamSource mss{ mssFactory.ActivateInstance<MediaStreamSource>() };

        // Initialize the MSS
        FFmpegInteropMSS::InitFromStream(stream, mss, nullptr);

        // We need to take care handling the MSS after this point. The MSS and FFmpegInteropMSS have circular
        // references on each other that need to be broken by calling Shutdown() on the MSS's IMFMediaSource.
        // Getting the MSS's IMFMediaSource can't fail, but theoretically if it did the MSS and FFmpegInteropMSS would leak.
        com_ptr<IMFMediaSource> mediaSource;
		THROW_IF_FAILED(mss.as<IMFGetService>()->GetService(MF_MEDIASOURCE_SERVICE, __uuidof(mediaSource), mediaSource.put_void()));
        return ShutdownWrapper<IMFMediaSource>{ std::move(mediaSource) };
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

        // Check if object creation was canceled
        RETURN_IF_FAILED(pResult->GetStatus());

        // Get the media source
        com_ptr<::IUnknown> object;
        RETURN_IF_FAILED(pResult->GetObject(ppObject));

		return S_OK;
	}
    CATCH_RETURN();

    IFACEMETHODIMP FFmpegInteropByteStreamHandler::CancelObjectCreation(_In_ ::IUnknown* pIUnknownCancelCookie) noexcept
	try
	{
        RETURN_HR_IF_NULL(E_INVALIDARG, pIUnknownCancelCookie);

        com_ptr<IMFAsyncResult> result;
        RETURN_IF_FAILED(pIUnknownCancelCookie->QueryInterface(result.put()));

        // Check if object creation has already been canceled
        RETURN_IF_FAILED(result->GetStatus());

        // Get the media source
        com_ptr<::IUnknown> object;
        RETURN_IF_FAILED(result->GetObject(object.put()));

        // Shutdown the media source
        com_ptr<IMFMediaSource> mediaSource{ object.as<IMFMediaSource>() };
        RETURN_IF_FAILED(mediaSource->Shutdown());

        // Update the result status to indicate object creation was canceled
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
