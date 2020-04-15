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

#include "FFmpegInteropMSS.g.h"
#include "Reader.h"

namespace winrt::FFmpegInterop::implementation
{
	class FFmpegInteropMSS :
		public FFmpegInteropMSST<FFmpegInteropMSS>
	{
	public:
		static void CreateFromStream(_In_ const Windows::Storage::Streams::IRandomAccessStream& fileStream, _In_ const Windows::Media::Core::MediaStreamSource& mss, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);
		static void CreateFromUri(_In_ const hstring& uri, _In_ const Windows::Media::Core::MediaStreamSource& mss, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);

		FFmpegInteropMSS(_In_ const Windows::Storage::Streams::IRandomAccessStream& fileStream, _In_ const Windows::Media::Core::MediaStreamSource& mss, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);
		FFmpegInteropMSS(_In_ const hstring& uri, _In_ const Windows::Media::Core::MediaStreamSource& mss, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);

	private:
		FFmpegInteropMSS(_In_ const Windows::Media::Core::MediaStreamSource& mss);

		void OpenFile(_In_ const Windows::Storage::Streams::IRandomAccessStream& fileStream, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);
		void OpenFile(_In_z_ const char* uri, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);

		void InitFFmpegContext(_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);

		void OnStarting(_In_ const Windows::Media::Core::MediaStreamSource& sender, _In_ const Windows::Media::Core::MediaStreamSourceStartingEventArgs& args);
		void OnSampleRequested(_In_ const Windows::Media::Core::MediaStreamSource& sender, _In_ const Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs& args);
		void OnSwitchStreamsRequested(_In_ const Windows::Media::Core::MediaStreamSource& sender, _In_ const Windows::Media::Core::MediaStreamSourceSwitchStreamsRequestedEventArgs& args);
		void OnClosed(_In_ const Windows::Media::Core::MediaStreamSource& sender, _In_ const Windows::Media::Core::MediaStreamSourceClosedEventArgs& args);

		std::mutex m_lock;
		Windows::Media::Core::MediaStreamSource m_mss; // We hold a circular reference to the provided MSS which we break when the Closed event is fired
		com_ptr<IStream> m_fileStream;
		AVIOContext_ptr m_ioContext;
		AVFormatContext_ptr m_formatContext;
		Reader m_reader;
		std::map<Windows::Media::Core::IMediaStreamDescriptor, std::unique_ptr<SampleProvider>> m_streamDescriptorMap;
		std::map<int, SampleProvider*> m_streamIdMap;

		event_token m_startingEventToken;
		event_token m_sampleRequestedEventToken;
		event_token m_switchStreamsRequestedEventToken;
		event_token m_closedEventToken;
	};
}

namespace winrt::FFmpegInterop::factory_implementation
{
	struct FFmpegInteropMSS :
		public FFmpegInteropMSST<FFmpegInteropMSS, implementation::FFmpegInteropMSS>
	{

	};
}
