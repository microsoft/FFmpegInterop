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

#pragma once

namespace winrt::FFmpegInterop::implementation
{
	TRACELOGGING_DECLARE_PROVIDER(g_FFmpegInteropProvider);

#define FFMPEG_INTEROP_PROVIDER_NAME "Microsoft.Windows.MediaFoundation.FFmpegInterop"
// {3D64F3FC-1826-4F56-9688-AD139DAF7B1A}
#define FFMPEG_INTEROP_PROVIDER_GUID (0x3d64f3fc, 0x1826, 0x4f56, 0x96, 0x88, 0xad, 0x13, 0x9d, 0xaf, 0x7b, 0x1a)

	class FFmpegInteropProvider : 
		public wil::TraceLoggingProvider
	{
		IMPLEMENT_TRACELOGGING_CLASS_WITH_MICROSOFT_TELEMETRY(
			FFmpegInteropProvider,
			FFMPEG_INTEROP_PROVIDER_NAME,
			FFMPEG_INTEROP_PROVIDER_GUID);

	public:
		DEFINE_TRACELOGGING_ACTIVITY(CreateFromStream);
		DEFINE_TRACELOGGING_ACTIVITY(CreateFromUri);
		DEFINE_TRACELOGGING_ACTIVITY(OnStarting);
		DEFINE_TRACELOGGING_ACTIVITY(OnSampleRequested);
		DEFINE_TRACELOGGING_ACTIVITY(OnSwitchStreamsRequested);
		DEFINE_TRACELOGGING_ACTIVITY(OnClosed);
	};

// Strip path from __FILE__
#define FILENAME(file) std::string_view(file).substr(std::string_view(file).rfind('\\') + 1).data()

#define FFMPEG_INTEROP_TRACE(message, ...) \
__if_exists(__identifier(this)) \
{ \
	FFmpegInteropProvider::TraceLoggingInfo("(%S%d) %S@0x%p: " message, \
		FILENAME(__FILE__), __LINE__, __func__, this, __VA_ARGS__) \
} \
__if_not_exists(__identifier(this)) \
{ \
	FFmpegInteropProvider::TraceLoggingInfo("(%S%d) %S: " message, \
		FILENAME(__FILE__), __LINE__, __func__, __VA_ARGS__) \
} \

}
