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
#include "ACMSampleProvider.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Media::MediaProperties;
using namespace std;
using namespace wil;

namespace winrt::FFmpegInterop::implementation
{
	ACMSampleProvider::ACMSampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
		SampleProvider(stream, reader)
	{

	}

	void ACMSampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
	{
		// We intentionally don't call SampleProvider::SetEncodingProperties() here. We'll set all of the encoding properties we need.

		// FFmpeg strips the wave format header from the codec private data. Recreate it.
		const AVCodecParameters* codecPar{ m_stream->codecpar };
		vector<uint8_t> waveFormatBuf(sizeof(WAVEFORMATEXTENSIBLE) + codecPar->extradata_size);
		const bool setValidBitsPerSample{ codecPar->bits_per_coded_sample % BITS_PER_BYTE != 0 };

		WAVEFORMATEXTENSIBLE* waveFormatExtensible{ reinterpret_cast<WAVEFORMATEXTENSIBLE*>(waveFormatBuf.data()) };
		waveFormatExtensible->SubFormat = MFAudioFormat_Base;
		waveFormatExtensible->SubFormat.Data1 = codecPar->codec_tag;
		waveFormatExtensible->Samples.wValidBitsPerSample = setValidBitsPerSample ? codecPar->bits_per_coded_sample : 0;
		waveFormatExtensible->dwChannelMask = codecPar->channel_layout;

		WAVEFORMATEX& waveFormatEx{ waveFormatExtensible->Format };
		waveFormatEx.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		waveFormatEx.nChannels = codecPar->channels;
		waveFormatEx.nSamplesPerSec = codecPar->sample_rate;
		waveFormatEx.nAvgBytesPerSec = static_cast<uint32_t>(codecPar->bit_rate / BITS_PER_BYTE);
		waveFormatEx.nBlockAlign = codecPar->block_align;
		waveFormatEx.wBitsPerSample = setValidBitsPerSample ? (codecPar->bits_per_coded_sample + BITS_PER_BYTE - codecPar->bits_per_coded_sample % BITS_PER_BYTE) : codecPar->bits_per_coded_sample;
		waveFormatEx.cbSize = codecPar->extradata_size + sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

		copy(codecPar->extradata, codecPar->extradata + codecPar->extradata_size, waveFormatBuf.begin() + sizeof(WAVEFORMATEXTENSIBLE));

		// Initialize a media type from the wave format
		com_ptr<IMFMediaType> mediaType;
		THROW_IF_FAILED(MFCreateMediaType(mediaType.put()));
		THROW_IF_FAILED(MFInitMediaTypeFromWaveFormatEx(mediaType.get(), reinterpret_cast<WAVEFORMATEX*>(waveFormatBuf.data()), static_cast<uint32_t>(waveFormatBuf.size())));

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
