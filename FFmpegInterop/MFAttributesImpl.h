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

#pragma once

namespace winrt::FFmpegInterop::implementation
{
    class MFAttributesImpl :
        public implements<
            MFAttributesImpl,
            IMFAttributes>
    {
    public:
        MFAttributesImpl()
        {
            THROW_IF_FAILED(MFCreateAttributes(m_attributes.put(), 0));
        }

        IFACEMETHOD(GetItem)(_In_ REFGUID guidKey, _Out_opt_ PROPVARIANT* pValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetItem(guidKey, pValue));
            return S_OK;
        }

        IFACEMETHOD(GetItemType)(_In_ REFGUID guidKey, _Out_ MF_ATTRIBUTE_TYPE* pType) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetItemType(guidKey, pType));
            return S_OK;
        }

        IFACEMETHOD(CompareItem)(_In_ REFGUID guidKey, _In_ REFPROPVARIANT value, _Out_ BOOL* pbResult) noexcept
        {
            RETURN_IF_FAILED(m_attributes->CompareItem(guidKey, value, pbResult));
            return S_OK;
        }

        IFACEMETHOD(Compare)(
            _In_ IMFAttributes* pTheirs,
            _In_ MF_ATTRIBUTES_MATCH_TYPE matchType,
            _Out_ BOOL* pbResult) noexcept
        {
            RETURN_IF_FAILED(m_attributes->Compare(pTheirs, matchType, pbResult));
            return S_OK;
        }

        IFACEMETHOD(GetUINT32)(_In_ REFGUID guidKey, _Out_ UINT32* punValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetUINT32(guidKey, punValue));
            return S_OK;
        }

        IFACEMETHOD(GetUINT64)(_In_ REFGUID guidKey, _Out_ UINT64* punValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetUINT64(guidKey, punValue));
            return S_OK;
        }

        IFACEMETHOD(GetDouble)(_In_ REFGUID guidKey, _Out_ double* pfValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetDouble(guidKey, pfValue));
            return S_OK;
        }

        IFACEMETHOD(GetGUID)(_In_ REFGUID guidKey, _Out_ GUID* pguidValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetGUID(guidKey, pguidValue));
            return S_OK;
        }

        IFACEMETHOD(GetStringLength)(_In_ REFGUID guidKey, _Out_ UINT32* pcchLength) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetStringLength(guidKey, pcchLength));
            return S_OK;
        }

        IFACEMETHOD(GetString)(
            _In_ REFGUID guidKey,
            _Out_writes_z_(cchBufSize) LPWSTR pwszValue,
            _In_ UINT32 cchBufSize,
            _Out_opt_ UINT32* pcchLength) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetString(guidKey, pwszValue, cchBufSize, pcchLength));
            return S_OK;
        }

        IFACEMETHOD(GetAllocatedString)(
            _In_ REFGUID guidKey,
            _Outptr_result_z_ LPWSTR* ppwszValue,
            _Out_ UINT32* pcchLength) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetAllocatedString(guidKey, ppwszValue, pcchLength));
            return S_OK;
        }

        IFACEMETHOD(GetBlobSize)(_In_ REFGUID guidKey, _Out_ UINT32* pcbBlobSize) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetBlobSize(guidKey, pcbBlobSize));
            return S_OK;
        }

        IFACEMETHOD(GetBlob)(
            _In_ REFGUID guidKey,
            _Out_writes_to_(cbBufSize, *pcbBlobSize) UINT8* pBuf,
            _In_ UINT32 cbBufSize, _Out_ UINT32* pcbBlobSize) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetBlob(guidKey, pBuf, cbBufSize, pcbBlobSize));
            return S_OK;
        }

        IFACEMETHOD(GetAllocatedBlob)(
            _In_ REFGUID guidKey,
            _Outptr_result_buffer_(*pcbSize) UINT8** ppBuf, _Out_ UINT32* pcbSize) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetAllocatedBlob(guidKey, ppBuf, pcbSize));
            return S_OK;
        }

        IFACEMETHOD(GetUnknown)(_In_ REFGUID guidKey, _In_ REFIID riid, _COM_Outptr_ LPVOID* ppv) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetUnknown(guidKey, riid, ppv));
            return S_OK;
        }

        IFACEMETHOD(SetItem)(_In_ REFGUID guidKey, _In_ REFPROPVARIANT value) noexcept
        {
            RETURN_IF_FAILED(m_attributes->SetItem(guidKey, value));
            return S_OK;
        }

        IFACEMETHOD(DeleteItem)(_In_ REFGUID guidKey) noexcept
        {
            RETURN_IF_FAILED(m_attributes->DeleteItem(guidKey));
            return S_OK;
        }

        IFACEMETHOD(DeleteAllItems)() noexcept
        {
            RETURN_IF_FAILED(m_attributes->DeleteAllItems());
            return S_OK;
        }

        IFACEMETHOD(SetUINT32)(_In_ REFGUID guidKey, _In_ UINT32 unValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->SetUINT32(guidKey, unValue));
            return S_OK;
        }

        IFACEMETHOD(SetUINT64)(_In_ REFGUID guidKey, _In_ UINT64 unValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->SetUINT64(guidKey, unValue));
            return S_OK;
        }

        IFACEMETHOD(SetDouble)(_In_ REFGUID guidKey, _In_ double fValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->SetDouble(guidKey, fValue));
            return S_OK;
        }

        IFACEMETHOD(SetGUID)(_In_ REFGUID guidKey, _In_ REFGUID guidValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->SetGUID(guidKey, guidValue));
            return S_OK;
        }

        IFACEMETHOD(SetString)(_In_ REFGUID guidKey, _In_z_ LPCWSTR wszValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->SetString(guidKey, wszValue));
            return S_OK;
        }

        IFACEMETHOD(SetBlob)(
            _In_ REFGUID guidKey,
            _In_reads_(cbBufSize) const UINT8* pBuf,
            _In_ UINT32 cbBufSize) noexcept
        {
            RETURN_IF_FAILED(m_attributes->SetBlob(guidKey, pBuf, cbBufSize));
            return S_OK;
        }

        IFACEMETHOD(SetUnknown)(_In_ REFGUID guidKey, _In_ IUnknown* pUnknown) noexcept
        {
            RETURN_IF_FAILED(m_attributes->SetUnknown(guidKey, pUnknown));
            return S_OK;
        }

        IFACEMETHOD(LockStore)() noexcept
        {
            RETURN_IF_FAILED(m_attributes->LockStore());
            return S_OK;
        }

        IFACEMETHOD(UnlockStore)() noexcept
        {
            RETURN_IF_FAILED(m_attributes->UnlockStore());
            return S_OK;
        }

        IFACEMETHOD(GetCount)(_Out_ UINT32* pcItems) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetCount(pcItems));
            return S_OK;
        }

        IFACEMETHOD(GetItemByIndex)(_In_ UINT32 unIndex, _Out_ GUID* pguidKey, _Out_ PROPVARIANT* pValue) noexcept
        {
            RETURN_IF_FAILED(m_attributes->GetItemByIndex(unIndex, pguidKey, pValue));
            return S_OK;
        }

        IFACEMETHOD(CopyAllItems)(_In_ IMFAttributes* pDest) noexcept
        {
            RETURN_IF_FAILED(m_attributes->CopyAllItems(pDest));
            return S_OK;
        }

    private:
        com_ptr<IMFAttributes> m_attributes;
    };
}
