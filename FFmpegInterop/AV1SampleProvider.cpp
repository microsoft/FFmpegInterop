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
#include "AV1SampleProvider.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	AV1SampleProvider::AV1SampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
		SampleProvider(stream, reader)
	{

	}

	tuple<IBuffer, int64_t, int64_t, map<GUID, IInspectable>> AV1SampleProvider::GetSampleData()
	{
		// Get the next sample
		AVPacket_ptr packet{ GetPacket() };

		const int64_t pts{ packet->pts };
		const int64_t dur{ packet->duration };
		const bool isKeyFrame{ (packet->flags & AV_PKT_FLAG_KEY) != 0 };

		// Prepend any config OBUs to key frames
		IBuffer buf{ TransformSample(move(packet), isKeyFrame) };

		// Set sample properties
		map<GUID, IInspectable> properties;
		if (isKeyFrame)
		{
			properties[MFSampleExtension_CleanPoint] = PropertyValue::CreateBoolean(true);
		}

		return { move(buf), pts, dur, properties };
	}

	IBuffer AV1SampleProvider::TransformSample(_Inout_ AVPacket_ptr packet, _In_ bool isKeyFrame)
	{
		// FFmpeg strips out the AV1CodecConfigurationRecord so extradata only contains config OBUs
		const uint8_t* configOBUs{ m_stream->codecpar->extradata };
		size_t configOBUsSize{ static_cast<size_t>(m_stream->codecpar->extradata_size) };

		// Prepend any config OBUs to key frames
		if (isKeyFrame && configOBUs != nullptr && configOBUsSize != 0)
		{
			const size_t sampleSize{ configOBUsSize + packet->size };

			vector<uint8_t> buf;
			buf.reserve(sampleSize);
			buf.insert(buf.end(), configOBUs, configOBUs + configOBUsSize);
			buf.insert(buf.end(), packet->data, packet->data + packet->size);

			return make<FFmpegInteropBuffer>(move(buf));
		}
		else
		{
			return make<FFmpegInteropBuffer>(move(packet));
		}
	}
}
