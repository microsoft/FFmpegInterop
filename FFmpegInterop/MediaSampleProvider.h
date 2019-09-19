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

#pragma once

#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Storage.Streams.h>

struct AVFormatContext;
struct AVCodecContext;
struct AVPacket;

namespace FFmpegInterop
{
	class FFmpegReader;

	// Smart pointer for managing AVPacket
	struct AVPacketDeleter
	{
		void operator()(AVPacket* packet);
	};
	typedef std::unique_ptr<AVPacket, AVPacketDeleter> AVPacket_ptr;

	class MediaSampleProvider
	{
	public:
		MediaSampleProvider(FFmpegReader& reader, const AVFormatContext* avFormatCtx, const AVCodecContext* avCodecCtx);
		virtual ~MediaSampleProvider();
		
		void SetCurrentStreamIndex(int streamIndex);
		void DisableStream();
		void EnableStream();
		void QueuePacket(AVPacket_ptr packet);
		void Flush();

		virtual winrt::Windows::Media::Core::MediaStreamSample GetNextSample();
		virtual HRESULT AllocateResources();
		virtual HRESULT WriteAVPacketToStream(const winrt::Windows::Storage::Streams::DataWriter& dataWriter, const AVPacket_ptr& packet);
		virtual HRESULT DecodeAVPacket(const winrt::Windows::Storage::Streams::DataWriter& dataWriter, const AVPacket_ptr& packet, int64_t& framePts, int64_t& frameDuration);
		virtual HRESULT GetNextPacket(const winrt::Windows::Storage::Streams::DataWriter& dataWriter, LONGLONG& pts, LONGLONG& dur, bool allowSkip);

	private:
		FFmpegReader& m_reader;
		const AVFormatContext* m_pAvFormatCtx;
		const AVCodecContext* m_pAvCodecCtx;
		int m_streamIndex;
		bool m_isDiscontinuous;
		bool m_isEnabled;
		std::deque<AVPacket_ptr> m_packetQueue;
		int64_t m_startOffset;
		int64_t m_nextFramePts;
	};
}
