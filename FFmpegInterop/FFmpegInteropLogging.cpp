
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

#include "pch.h"
#include "FFmpegInteropLogging.h"
#include "FFmpegInteropLogging.g.cpp"
#include "LogEventArgs.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;

namespace winrt::FFmpegInterop::implementation
{
	event<EventHandler<FFmpegInterop::LogEventArgs>> FFmpegInteropLogging::m_logEvent;

	void FFmpegInteropLogging::Log(_In_ void* avcl, _In_ int level, _In_ const char* fmt, _In_ va_list vl)
	{
		constexpr int LINE_SIZE{ 1024 };

		// Format the log line
		char lineA[LINE_SIZE];
		int printPrefix{ 1 };
		av_log_format_line(avcl, level, fmt, vl, lineA, LINE_SIZE, &printPrefix);

		// Convert from UTF8 -> UTF16
		wchar_t lineW[LINE_SIZE];
		if (MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, lineA, -1, lineW, LINE_SIZE) > 0)
		{
			TraceLoggingWrite(g_FFmpegInteropProvider, "FFmpegTrace", TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
				TraceLoggingValue(lineW, "Message"));

			// Raise a log event to any registered handlers
			m_logEvent(nullptr, make<LogEventArgs>(static_cast<FFmpegInterop::LogLevel>(level), hstring{ lineW }));
		}
	}

	event_token FFmpegInteropLogging::Log(_In_ const EventHandler<FFmpegInterop::LogEventArgs>& handler)
	{
		return m_logEvent.add(handler);
	}

	void FFmpegInteropLogging::Log(_In_ const event_token& token) noexcept
	{
		m_logEvent.remove(token);
	}
}
