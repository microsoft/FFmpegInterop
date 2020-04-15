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
#include "Utility.h"

using namespace winrt::Windows::Foundation;

namespace winrt::FFmpegInterop::implementation
{
	inline IInspectable CreatePropValueFromMFAttribute(_In_ const PROPVARIANT& propvar)
	{
		switch (propvar.vt)
		{
		case MF_ATTRIBUTE_UINT32:
			return PropertyValue::CreateUInt32(propvar.ulVal);

		case MF_ATTRIBUTE_UINT64:
			return PropertyValue::CreateUInt64(propvar.uhVal.QuadPart);

		case MF_ATTRIBUTE_DOUBLE:
			return PropertyValue::CreateDouble(propvar.dblVal);

		case MF_ATTRIBUTE_GUID:
			return PropertyValue::CreateGuid(*propvar.puuid);

		case MF_ATTRIBUTE_STRING:
			return PropertyValue::CreateString(propvar.pwszVal);

		case MF_ATTRIBUTE_BLOB:
			return PropertyValue::CreateUInt8Array({ propvar.blob.pBlobData, propvar.blob.pBlobData + propvar.blob.cbSize });

		case MF_ATTRIBUTE_IUNKNOWN:
		{
			IInspectable inspectable{ nullptr };
			THROW_IF_FAILED(propvar.punkVal->QueryInterface(guid_of<decltype(inspectable)>(), put_abi(inspectable)));
			return inspectable;
		}

		default:
			WINRT_ASSERT(false);
			THROW_HR(E_UNEXPECTED);
		}
	}
}
