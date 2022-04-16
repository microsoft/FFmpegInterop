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
#include "BitstreamReader.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	FLACSampleProvider::FLACSampleProvider(_In_ const AVFormatContext* formatContext, _In_ AVStream* stream, _In_ Reader& reader) :
		SampleProvider(formatContext, stream, reader)
	{
		THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, m_stream->codecpar->extradata_size < FLAC_STREAMINFO_SIZE);

		// Check if the codec private data includes the FLAC marker and block header before the FLAC stream info
		if (equal(FLAC_MARKER.begin(), FLAC_MARKER.end(), m_stream->codecpar->extradata))
		{
			THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, m_stream->codecpar->extradata_size < FLAC_MARKER.size() + FLAC_STREAMINFO_HEADER.size() + FLAC_STREAMINFO_SIZE);
			m_flacStreamInfo = m_stream->codecpar->extradata + FLAC_MARKER.size() + FLAC_STREAMINFO_HEADER.size();
		}
		else
		{
			m_flacStreamInfo = m_stream->codecpar->extradata;
		}

		// Update the codec parameters based on the stream info. These values may not have been set if FFmpeg wasn't built with the FLAC decoder.
		BitstreamReader bitstreamReader{ m_flacStreamInfo, FLAC_STREAMINFO_SIZE };
		bitstreamReader.SkipN(16); // Min block size
		bitstreamReader.SkipN(16); // Max block size
		bitstreamReader.SkipN(24); // Min frame size
		bitstreamReader.SkipN(24); // Max frame size
		m_stream->codecpar->sample_rate = static_cast<int>(bitstreamReader.ReadN(20));
		m_stream->codecpar->channels = static_cast<int>(bitstreamReader.ReadN(3)) + 1;
		m_stream->codecpar->bits_per_raw_sample = static_cast<int>(bitstreamReader.ReadN(5)) + 1;
		// Remaining fields left unparsed as they are unneeded at this time
	}

	tuple<IBuffer, int64_t, int64_t, vector<pair<GUID, Windows::Foundation::IInspectable>>, vector<pair<GUID, Windows::Foundation::IInspectable>>> FLACSampleProvider::GetSampleData()
	{
		if (m_isDiscontinuous)
		{
			// On discontinuity send the FLAC stream info as the first sample to the decoder
			vector<uint8_t> buf;
			buf.reserve(FLAC_MARKER.size() + FLAC_STREAMINFO_HEADER.size() + FLAC_STREAMINFO_SIZE);
			buf.insert(buf.end(), FLAC_MARKER.begin(), FLAC_MARKER.end());
			buf.insert(buf.end(), FLAC_STREAMINFO_HEADER.begin(), FLAC_STREAMINFO_HEADER.end());
			buf.insert(buf.end(), m_flacStreamInfo, m_flacStreamInfo + FLAC_STREAMINFO_SIZE);

			return { make<FFmpegInteropBuffer>(move(buf)), m_startOffset, 0, { }, { } };
		}
		else
		{
			return SampleProvider::GetSampleData();
		}
	}
}
