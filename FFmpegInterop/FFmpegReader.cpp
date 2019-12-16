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
#include "FFmpegReader.h"
#include "SampleProvider.h"

using namespace std;

using namespace winrt::FFmpegInterop::implementation;

FFmpegReader::FFmpegReader(_In_ AVFormatContext* formatContext, _In_ const map<int, SampleProvider*>& streamMap) :
	m_formatContext(formatContext),
	m_streamIdMap(streamMap)
{

}

void FFmpegReader::ReadPacket()
{
	AVPacket_ptr packet{ av_packet_alloc() };
	THROW_IF_NULL_ALLOC(packet);

	// Read the next packet and push it into the appropriate sample provider.
	// Drop the packet if the stream is not being used.
	THROW_IF_FFMPEG_FAILED(av_read_frame(m_formatContext, packet.get()));

	auto iter = m_streamIdMap.find(packet->stream_index);
	if (iter != m_streamIdMap.end())
	{
		iter->second->QueuePacket(move(packet));
	}
}
