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

#include "pch.h"
#include "VFWSampleProvider.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Media::MediaProperties;
using namespace std;
using namespace wil;

namespace winrt::FFmpegInterop::implementation
{
	VFWSampleProvider::VFWSampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
		SampleProvider(stream, reader)
	{
		
	}

	void VFWSampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
	{
		// We intentionally don't call SampleProvider::SetEncodingProperties() here. We'll set all of the encoding properties we need.

		// FFmpeg strips the bitmap info header from the codec private data. Recreate it.
		const AVCodecParameters* codecPar{ m_stream->codecpar };
		vector<uint8_t> vihBuf(sizeof(VIDEOINFOHEADER) + codecPar->extradata_size);

		BITMAPINFOHEADER& bih{ reinterpret_cast<VIDEOINFOHEADER*>(vihBuf.data())->bmiHeader };
		bih.biSize = sizeof(BITMAPINFOHEADER) + codecPar->extradata_size;
		bih.biWidth = codecPar->width;
		bih.biHeight = codecPar->height;
		bih.biPlanes = 1; // Must be 1
		bih.biBitCount = codecPar->bits_per_coded_sample;
		bih.biCompression = codecPar->codec_tag;

		copy(codecPar->extradata, codecPar->extradata + codecPar->extradata_size, vihBuf.begin() + sizeof(VIDEOINFOHEADER));

		// Initialize a media type from the video info header
		com_ptr<IMFMediaType> mediaType;
		THROW_IF_FAILED(MFCreateMediaType(mediaType.put()));
		THROW_IF_FAILED(MFInitMediaTypeFromVideoInfoHeader(mediaType.get(), reinterpret_cast<VIDEOINFOHEADER*>(vihBuf.data()), static_cast<uint32_t>(vihBuf.size()), nullptr));

		// Initialize the encoding properties from the media type
		MediaPropertySet encPropSet{ encProp.Properties() };

		com_ptr<IMFAttributes> mediaTypeAttributes{ mediaType.as<IMFAttributes>() };
		uint32_t mediaTypeAttributeCount{ 0 };
		THROW_IF_FAILED(mediaTypeAttributes->GetCount(&mediaTypeAttributeCount));

		for (uint32_t i{ 0 }; i < mediaTypeAttributeCount; i++)
		{
			// Get the next property
			GUID propGuid{ GUID_NULL };
			unique_prop_variant propValue;
			THROW_IF_FAILED(mediaTypeAttributes->GetItemByIndex(i, &propGuid, &propValue));

			// Add the property to the encoding property set
			encPropSet.Insert(propGuid, CreatePropValueFromMFAttribute(propValue));
		}
	}
}
