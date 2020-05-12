//*****************************************************************************
//
//	Copyright 2020 Microsoft Corporation
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
#include "Codecs.h"

using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Media::Core;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	bool UsesCodecPack(_In_ AVCodecID codecId)
	{
		switch (codecId)
		{
		case AV_CODEC_ID_AV1:
		case AV_CODEC_ID_HEVC:
		case AV_CODEC_ID_MPEG1VIDEO:
		case AV_CODEC_ID_MPEG2VIDEO:
		case AV_CODEC_ID_VP8:
		case AV_CODEC_ID_VP9:
			return true;

		default:
			return false;
		}
	}

	bool IsCodecPackInstalled(_In_ const hstring& subtype)
	{
		CodecQuery codecQuery;
		IVectorView<CodecInfo> codecInfos{ codecQuery.FindAllAsync(CodecKind::Video, CodecCategory::Decoder, subtype).get() };
		for (uint32_t i{ 0 }; i < codecInfos.Size(); i++)
		{
			CodecInfo codecInfo{ codecInfos.GetAt(i) };
			wstring displayName{ codecInfo.DisplayName() };
			if (displayName.find(L"VideoExtension") != string::npos)
			{
				return true;
			}
		}

		return false;
	}

	bool IsCodecPackInstalled(_In_ AVCodecID codecId)
	{
		switch (codecId)
		{
		case AV_CODEC_ID_AV1:
			return IsCodecPackInstalled(to_hstring(MFVideoFormat_AV1));

		case AV_CODEC_ID_HEVC:
			return IsCodecPackInstalled(to_hstring(MFVideoFormat_HEVC));

		case AV_CODEC_ID_MPEG1VIDEO:
			return IsCodecPackInstalled(to_hstring(MFVideoFormat_MPG1));

		case AV_CODEC_ID_MPEG2VIDEO:
			return IsCodecPackInstalled(to_hstring(MFVideoFormat_MPEG2));

		case AV_CODEC_ID_VP8:
			return IsCodecPackInstalled(to_hstring(MFVideoFormat_VP80));

		case AV_CODEC_ID_VP9:
			return IsCodecPackInstalled(to_hstring(MFVideoFormat_VP90));

		default:
			WINRT_ASSERT(false);
			THROW_HR(E_INVALIDARG);
			return false;
		}
	}
}
