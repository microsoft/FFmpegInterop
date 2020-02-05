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

#include "pch.h"
#include "Tracing.h"

namespace winrt::FFmpegInterop::implementation
{
	// {3D64F3FC-1826-4F56-9688-AD139DAF7B1A}
	TRACELOGGING_DEFINE_PROVIDER(
		g_FFmpegInteropProvider,
		"FFmpegInterop",
		(0x3d64f3fc, 0x1826, 0x4f56, 0x96, 0x88, 0xad, 0x13, 0x9d, 0xaf, 0x7b, 0x1a));
}
