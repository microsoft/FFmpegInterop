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

namespace winrt::FFmpegInterop::implementation
{
	class FFmpegInteropBuffer :
		public implements<
			FFmpegInteropBuffer,
			Windows::Storage::Streams::IBuffer,
			::Windows::Storage::Streams::IBufferByteAccess,
			IMarshal>
	{
	public:
		FFmpegInteropBuffer(_In_ AVBufferRef* bufRef);
		FFmpegInteropBuffer(_In_ AVBufferRef_ptr bufRef) noexcept;
		FFmpegInteropBuffer(_In_ AVPacket_ptr packet);
		FFmpegInteropBuffer(_In_ std::vector<uint8_t>&& buf) noexcept;

		// IBuffer
		uint32_t Capacity() const noexcept;
		uint32_t Length() const noexcept;
		void Length(_In_ uint32_t length);

		// IBufferByteAccess
		STDMETHODIMP Buffer(_Outptr_ uint8_t** buf) noexcept;

		// IMarshal
		STDMETHODIMP GetUnmarshalClass(
			_In_ REFIID riid,
			_In_opt_ void* pv,
			_In_ DWORD dwDestContext,
			_Reserved_ void* pvDestContext,
			_In_ DWORD mshlflags,
			_Out_ CLSID* pclsid) noexcept;
		STDMETHODIMP GetMarshalSizeMax(
			_In_ REFIID riid,
			_In_opt_ void* pv,
			_In_ DWORD dwDestContext,
			_Reserved_ void* pvDestContext,
			_In_ DWORD mshlflags,
			_Out_ DWORD* pcbSize) noexcept;
		STDMETHODIMP MarshalInterface(
			_In_ IStream* pStm,
			_In_ REFIID riid,
			_In_opt_ void* pv,
			_In_ DWORD dwDestContext,
			_Reserved_ void* pvDestContext,
			_In_ DWORD mshlflags) noexcept;
		STDMETHODIMP UnmarshalInterface(_In_ IStream* pStm, _In_ REFIID riid, _Outptr_ void** ppv) noexcept;
		STDMETHODIMP ReleaseMarshalData(_In_ IStream* pStm) noexcept;
		STDMETHODIMP DisconnectObject(_Reserved_ DWORD dwReserved) noexcept;

	private:
		HRESULT InitMarshalerIfNeeded() noexcept;

		uint32_t m_length;
		std::unique_ptr<uint8_t, std::function<void(uint8_t*)>> m_buf;
		com_ptr<IMarshal> m_marshaler;
	};
}
