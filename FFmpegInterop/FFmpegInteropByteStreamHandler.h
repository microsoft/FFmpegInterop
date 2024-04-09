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

	// Byte stream wrapper that prevents the underlying byte stream from being closed unless permitted
	class ByteStreamProxy :
		public implements<
			ByteStreamProxy,
			IMFByteStream,
			IMFGetService>
	{
	public:
		ByteStreamProxy(_In_ IMFByteStream* byteStream)
		{
			m_byteStream.copy_from(byteStream);
		}

		void AllowClosing(bool fAllowClosing) noexcept
		{
			m_fAllowClosing = fAllowClosing;
		}

		// IMFByteStream
		IFACEMETHOD(GetCapabilities(_Out_ DWORD* pdwCapabilities)) noexcept
		{
			return m_byteStream->GetCapabilities(pdwCapabilities);
		}

		IFACEMETHOD(GetLength(_Out_ QWORD* pqwLength)) noexcept
		{
			return m_byteStream->GetLength(pqwLength);
		}

		IFACEMETHOD(SetLength(_In_ QWORD qwLength)) noexcept
		{
			return m_byteStream->SetLength(qwLength);
		}

		IFACEMETHOD(GetCurrentPosition(_Out_ QWORD* pqwPosition)) noexcept
		{
			return m_byteStream->GetCurrentPosition(pqwPosition);
		}

		IFACEMETHOD(SetCurrentPosition(_In_ QWORD qwPosition)) noexcept
		{
			return m_byteStream->SetCurrentPosition(qwPosition);
		}

		IFACEMETHOD(IsEndOfStream(_Out_ BOOL* pfEndOfStream)) noexcept
		{
			return m_byteStream->IsEndOfStream(pfEndOfStream);
		}

		IFACEMETHOD(Read(
			_Out_writes_to_(cb, *pcbRead) BYTE* pb,
			_In_ ULONG cb,
			_Out_ ULONG* pcbRead)) noexcept
		{
			return m_byteStream->Read(pb, cb, pcbRead);
		}

		IFACEMETHOD(BeginRead(
			_Out_writes_(cb) BYTE* pb,
			_In_ ULONG cb,
			_In_ IMFAsyncCallback* pCallback,
			_In_ IUnknown* pUnkState)) noexcept
		{
			return m_byteStream->BeginRead(pb, cb, pCallback, pUnkState);
		}

		IFACEMETHOD(EndRead(
			_In_ IMFAsyncResult* pResult,
			_Out_ ULONG* pcbRead)) noexcept
		{
			return m_byteStream->EndRead(pResult, pcbRead);
		}

		IFACEMETHOD(Write(
			_In_reads_(cb) const BYTE* pb,
			_In_ ULONG cb,
			_Out_ ULONG* pcbWritten)) noexcept
		{
			return m_byteStream->Write(pb, cb, pcbWritten);
		}

		IFACEMETHOD(BeginWrite(
			_In_reads_(cb) const BYTE* pb,
			_In_ ULONG cb,
			_In_ IMFAsyncCallback* pCallback,
			_In_ IUnknown* pUnkState)) noexcept
		{
			return m_byteStream->BeginWrite(pb, cb, pCallback, pUnkState);
		}

		IFACEMETHOD(EndWrite(
			_In_ IMFAsyncResult* pResult,
			_Out_ ULONG* pcbWritten)) noexcept
		{
			return m_byteStream->EndWrite(pResult, pcbWritten);
		}

		IFACEMETHOD(Seek(
			_In_ MFBYTESTREAM_SEEK_ORIGIN seekOrigin,
			_In_ LONGLONG llSeekOffset,
			_In_ DWORD dwSeekFlags,
			_Out_ QWORD* pqwCurrentPosition)) noexcept
		{
			return m_byteStream->Seek(seekOrigin, llSeekOffset, dwSeekFlags, pqwCurrentPosition);
		}

		IFACEMETHOD(Flush()) noexcept
		{
			return m_byteStream->Flush();
		}

		IFACEMETHOD(Close()) noexcept
		{
			RETURN_HR_IF_EXPECTED(S_FALSE, !m_fAllowClosing);
			return m_byteStream->Close();
		}

		// IMFGetService
		IFACEMETHOD(GetService)(
			_In_ REFGUID guidService,
			_In_ REFIID riid,
			_COM_Outptr_ void** ppv) noexcept
		{
			RETURN_HR_IF_NULL(E_POINTER, ppv);

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
			if (guidService == MF_WRAPPED_OBJECT)
			{
				return m_byteStream->QueryInterface(riid, ppv);
			}
#endif // WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

			RETURN_HR(MF_E_UNSUPPORTED_SERVICE);
		}

	private:
		com_ptr<IMFByteStream> m_byteStream;
		bool m_fAllowClosing{ false };
	};
}

namespace winrt::FFmpegInterop::factory_implementation
{
	struct FFmpegInteropByteStreamHandler :
		public FFmpegInteropByteStreamHandlerT<FFmpegInteropByteStreamHandler, implementation::FFmpegInteropByteStreamHandler>
	{

	};
}
