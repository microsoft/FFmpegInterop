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
#include "FLACSampleProvider.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	FLACSampleProvider::FLACSampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
		SampleProvider(stream, reader)
	{

	}

	tuple<IBuffer, int64_t, int64_t, map<GUID, IInspectable>> FLACSampleProvider::GetSampleData()
	{
		if (m_isDiscontinuous)
		{
			// On discontinuity send the codec private data containing the FLAC STREAMINFO block as the first sample to the decoder.
			// We need to prepend the FLAC STREAMINFO block header as FFmpeg strips this out.
			constexpr uint32_t FLAC_STREAMINFO_SIZE{ 34 };
			constexpr uint8_t FLAC_STREAMINFO_HEADER[]{ 'f', 'L', 'a', 'C', 0x80, 0x00, 0x00, 0x22 };

			THROW_HR_IF(E_UNEXPECTED, m_stream->codecpar->extradata_size < FLAC_STREAMINFO_SIZE);

			vector<uint8_t> buf;
			buf.reserve(sizeof(FLAC_STREAMINFO_HEADER) + FLAC_STREAMINFO_SIZE);
			buf.insert(buf.end(), FLAC_STREAMINFO_HEADER, FLAC_STREAMINFO_HEADER + sizeof(FLAC_STREAMINFO_HEADER));
			buf.insert(buf.end(), m_stream->codecpar->extradata, m_stream->codecpar->extradata + FLAC_STREAMINFO_SIZE);

			return { make<FFmpegInteropBuffer>(move(buf)), m_startOffset, 0, { } };
		}
		else
		{
			return SampleProvider::GetSampleData();
		}
	}
}
