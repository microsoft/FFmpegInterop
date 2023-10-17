
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

using namespace std;
using namespace winrt;
using namespace winrt::Windows::Foundation;

namespace winrt::FFmpegInterop::implementation
{
	event<EventHandler<FFmpegInterop::LogEventArgs>> FFmpegInteropLogging::m_logEvent;

	void FFmpegInteropLogging::Log(_In_ void* avcl, _In_ int level, _In_ const char* fmt, _In_ va_list vl)
	{
		// Get the required buffer size
		int printPrefix{ 1 };
		int lineSize = av_log_format_line2(avcl, level, fmt, vl, nullptr, 0, &printPrefix);
		THROW_HR_IF_FFMPEG_FAILED(lineSize);

		// Format the log line
		auto line{ make_unique_for_overwrite<char[]>(lineSize + 1) };
		int charWritten = av_log_format_line2(avcl, level, fmt, vl, line.get(), lineSize + 1, &printPrefix);
		THROW_HR_IF_FFMPEG_FAILED(charWritten);
		THROW_HR_IF(E_UNEXPECTED, lineSize != charWritten);

		m_logEvent(nullptr, make<LogEventArgs>(static_cast<FFmpegInterop::LogLevel>(level), to_hstring(line.get())));

		// Trim trailing whitespace
		for (int i{ lineSize - 1 }; i >= 0 && isspace(line[i]); i--)
		{
			line[i] = '\0';
		}

		FFMPEG_INTEROP_TRACE("%S", line.get());
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
