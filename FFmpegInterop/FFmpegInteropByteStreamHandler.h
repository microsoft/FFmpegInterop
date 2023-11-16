//*****************************************************************************
//
//	Copyright 2023 Microsoft Corporation
//
//	Licensed under the Apache License, Version 2.0 (the "License") noexcept;
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

#include "FFmpegInteropByteStreamHandler.g.h"

namespace winrt::FFmpegInterop::implementation
{
	class FFmpegInteropByteStreamHandler :
		public FFmpegInteropByteStreamHandlerT<
            FFmpegInteropByteStreamHandler,
            IMFByteStreamHandler,
            MFAttributesImpl>
	{
	public:
        FFmpegInteropByteStreamHandler();

        // IMFByteStreamHandler
        IFACEMETHOD(BeginCreateObject)(
            _In_ IMFByteStream* pByteStream,
            _In_opt_ LPCWSTR pURL,
            _In_ DWORD dwFlags,
            _In_opt_ IPropertyStore* pPropertyStore,
            _COM_Outptr_opt_ ::IUnknown** ppCancelCookie,
            _In_ IMFAsyncCallback* pCallback,
            _In_opt_ ::IUnknown* pState) noexcept;

        IFACEMETHOD(EndCreateObject)(
            _In_ IMFAsyncResult* pResult,
            _Out_ MF_OBJECT_TYPE* pObjectType,
            _COM_Outptr_ ::IUnknown** ppObject) noexcept;

        IFACEMETHOD(CancelObjectCreation)(_In_ ::IUnknown* pIUnknownCancelCookie) noexcept;
        IFACEMETHOD(GetMaxNumberOfBytesRequiredForResolution)(_Out_ QWORD* pcb) noexcept;

    private:
        void CreateMediaSource(_In_ IMFByteStream* byteStream, _In_ IMFAsyncResult* result);

        std::map<IMFAsyncResult*, ShutdownWrapper<IMFMediaSource>> m_map;
	};
}

namespace winrt::FFmpegInterop::factory_implementation
{
	struct FFmpegInteropByteStreamHandler :
		public FFmpegInteropByteStreamHandlerT<FFmpegInteropByteStreamHandler, implementation::FFmpegInteropByteStreamHandler>
	{

	};
}
