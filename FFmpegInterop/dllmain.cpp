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

using namespace winrt::FFmpegInterop::implementation;

BOOL WINAPI DllMain(_In_ HINSTANCE hInstance, _In_ DWORD dwReason, _In_opt_ LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		// Disable DLL_THREAD_ATTACH and DLL_THREAD_DETACH notifications
		DisableThreadLibraryCalls(hInstance);

		// Register TraceLogging provider
		TraceLoggingRegister(g_FFmpegInteropProvider);
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		// Unregister TraceLogging provider
		TraceLoggingUnregister(g_FFmpegInteropProvider);
	}

	return TRUE;
}
