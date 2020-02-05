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
#include "MPEGSampleProvider.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::MediaProperties;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	MPEGSampleProvider::MPEGSampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
		SampleProvider(stream, reader)
	{

	}

	void MPEGSampleProvider::SetEncodingProperties(_Inout_ const IMediaEncodingProperties& encProp, _In_ bool setFormatUserData)
	{
		SampleProvider::SetEncodingProperties(encProp, setFormatUserData);

		const AVCodecParameters* codecPar{ m_stream->codecpar };

		if (codecPar->extradata != nullptr && codecPar->extradata_size > 0)
		{
			MediaPropertySet videoProp{ encProp.Properties() };
			videoProp.Insert(MF_MT_MPEG_SEQUENCE_HEADER, PropertyValue::CreateUInt8Array({ codecPar->extradata, codecPar->extradata + codecPar->extradata_size }));
		}
	}
}
