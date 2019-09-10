
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

extern "C"
{
#include <libavutil/log.h>
}

using namespace winrt;
using namespace winrt::FFmpegInterop;

namespace winrt::FFmpegInterop::implementation
{
	ILogProvider FFmpegInteropLogging::s_LogProvider{ nullptr };

	void FFmpegInteropLogging::SetLogLevel(const LogLevel& level)
	{
		av_log_set_level((int)level);
	}

	void FFmpegInteropLogging::SetLogProvider(const ILogProvider& logProvider)
	{
		s_LogProvider = logProvider;
		av_log_set_callback([](void* avcl, int level, const char* fmt, va_list vl)->void
		{
			if (level <= av_log_get_level())
			{
				if (s_LogProvider != nullptr)
				{
					char pLine[1000];
					int printPrefix = 1;
					av_log_format_line(avcl, level, fmt, vl, pLine, sizeof(pLine), &printPrefix);

					wchar_t wLine[sizeof(pLine)];
					if (MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, pLine, -1, wLine, sizeof(pLine)) != 0)
					{
						s_LogProvider.Log((LogLevel)level, hstring(wLine));
					}
				}
			}
		});
	}

	void FFmpegInteropLogging::SetDefaultLogProvider()
	{
		av_log_set_callback(av_log_default_callback);
	}
}
