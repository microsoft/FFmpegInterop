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
#include "MediaSampleProvider.h"
#include "FFmpegReader.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Media::Core;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	MediaSampleProvider::MediaSampleProvider(_In_ const AVStream* stream, _Inout_ FFmpegReader& reader) :
		m_stream(stream),
		m_reader(reader)
	{
		if (m_stream->start_time != AV_NOPTS_VALUE)
		{
			m_startOffset = m_stream->start_time;
			m_nextSamplePts = m_stream->start_time;
		}
		else
		{
			m_startOffset = 0;
			m_nextSamplePts = 0;
		}
	}

	void MediaSampleProvider::Select() noexcept
	{
		WINRT_ASSERT(!m_isSelected);
		m_isSelected = true;
	}

	void MediaSampleProvider::Deselect() noexcept
	{
		WINRT_ASSERT(m_isSelected);
		m_isSelected = false;
		Flush();
	}

	void MediaSampleProvider::OnSeek(_In_ int64_t seekTime) noexcept
	{
		m_nextSamplePts = seekTime;
		Flush();
	}

	void MediaSampleProvider::Flush() noexcept
	{
		m_packetQueue.clear();
		m_isDiscontinuous = true;
	}

	void MediaSampleProvider::QueuePacket(_In_ AVPacket_ptr packet)
	{
		if (m_isSelected)
		{
			m_packetQueue.push_back(move(packet));
		}
	}

	void MediaSampleProvider::GetSample(_Inout_ const MediaStreamSourceSampleRequest& request)
	{
		// Make sure this stream is selected
		THROW_HR_IF(MF_E_INVALIDREQUEST, !m_isSelected);

		// Get the sample data, timestamp, and duration
		auto [buf, pts, dur] = GetSampleData();

		// Make sure the PTS is set
		if (pts == AV_NOPTS_VALUE)
		{
			pts = m_nextSamplePts;
		}

		// Calculate the PTS for the next sample
		m_nextSamplePts = pts + dur;

		// Convert time base from FFmpeg to MF
		pts = static_cast<int64_t>(av_q2d(m_stream->time_base) * 10000000 * (pts - m_startOffset));
		dur = static_cast<int64_t>(av_q2d(m_stream->time_base) * 10000000 * dur);

		// Craete the sample
		MediaStreamSample sample{ MediaStreamSample::CreateFromBuffer(buf, static_cast<TimeSpan>(pts)) };
		sample.Duration(TimeSpan{ dur });
		sample.Discontinuous(m_isDiscontinuous);

		SetSampleProperties(sample);

		m_isDiscontinuous = false;

		request.Sample(move(sample));
	}

	tuple<IBuffer, int64_t, int64_t> MediaSampleProvider::GetSampleData()
	{
		AVPacket_ptr packet{ GetPacket() };
		return { make<FFmpegBuffer>(packet.get()), packet->pts, packet->duration };
	}

	AVPacket_ptr MediaSampleProvider::GetPacket()
	{
		// Continue reading until there is an appropriate packet in the stream
		while (m_packetQueue.empty())
		{
			m_reader.ReadPacket();
		}

		AVPacket_ptr packet{ move(m_packetQueue.front()) };
		m_packetQueue.pop_front();

		return packet;
	}
}