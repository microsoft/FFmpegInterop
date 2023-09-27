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

#ifndef NOMINMAX
#define NOMINMAX // Don't define min()/max() macros in Windows.h
#endif

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

// WIL
#include <wil/cppwinrt.h>
#include <wil/result.h>

// WinRT
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Metadata.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <robuffer.h>

// Windows
#include <Windows.h>
#include <evntrace.h>
#include <TraceLoggingProvider.h>
#include <TraceLoggingActivity.h>
#include <shcore.h>

// MF
#include <mfidl.h>
#include <mfapi.h>
#include <mferror.h>
#include <codecapi.h>
#include <wmcodecdsp.h>
#include <amvideo.h>

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
DEFINE_GUID(MEDIASUBTYPE_DOLBY_TRUEHD, 0xeb27cec4, 0x163e, 0x4ca3, 0x8b, 0x74, 0x8e, 0x25, 0xf9, 0x1b, 0x51, 0x7e);
DEFINE_GUID(MF_PROPERTY_HANDLER_SERVICE, 0xa3face02, 0x32b8, 0x41dd, 0x90, 0xe7, 0x5f, 0xef, 0x7c, 0x89, 0x91, 0xb5);
DEFINE_GUID(MF_MT_CUSTOM_VIDEO_PRIMARIES, 0x47537213, 0x8cfb, 0x4722, 0xaa, 0x34, 0xfb, 0xc9, 0xe2, 0x4d, 0x77, 0xb8);

typedef struct _MT_CUSTOM_VIDEO_PRIMARIES {
    float fRx;
    float fRy;
    float fGx;
    float fGy;
    float fBx;
    float fBy;
    float fWx;
    float fWy;
} MT_CUSTOM_VIDEO_PRIMARIES;
#endif // !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

// FFmpeg
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/imgutils.h>
#include <libavutil/mastering_display_metadata.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

// STL
#include <algorithm>
#include <memory>
#include <functional>
#include <deque>
#include <map>
#include <mutex>
#include <tuple>
#include <limits>
#include <codecvt> // TODO: Deprecated in C++17. Replace when an alternative is available.
#include <cstdlib>

// FFmpegInterop
#include "Tracing.h"
#include "Utility.h"
#include "FFmpegInteropBuffer.h"
