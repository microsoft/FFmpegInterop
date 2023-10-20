//*****************************************************************************
//
//	Copyright 2017 Microsoft Corporation
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

#include "FFmpegInteropLogging.g.h"

namespace winrt::FFmpegInterop::implementation
{
	class FFmpegInteropLogging
	{
	public:
		static void Log(_In_ void* avcl, _In_ int level, _In_ const char* fmt, _In_ va_list vl) noexcept;

		static event_token Log(_In_ const Windows::Foundation::EventHandler<FFmpegInterop::LogEventArgs>& handler) noexcept;
		static void Log(_In_ const event_token& token) noexcept;

	private:
		FFmpegInteropLogging() = delete;

		static event<Windows::Foundation::EventHandler<FFmpegInterop::LogEventArgs>> m_logEvent;
	};
}

namespace winrt::FFmpegInterop::factory_implementation
{
	struct FFmpegInteropLogging :
		public FFmpegInteropLoggingT<FFmpegInteropLogging, implementation::FFmpegInteropLogging>
	{

	};
}
