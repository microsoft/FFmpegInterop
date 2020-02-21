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

	#define TRACE_LOGGING_ACTIVITY_CLASS(FunctionName) _TRACE_LOGGING_ACTIVITY_CLASS(FunctionName ## Activity, FunctionName)
	#define _TRACE_LOGGING_ACTIVITY_CLASS(ActivityClassName, FunctionName) \
	class ActivityClassName \
	{ \
	public: \
		ActivityClassName(_Inout_ ActivityClassName&& other) noexcept : \
			m_failureCache(std::move(other.m_failureCache)), \
			m_activity(std::move(other.m_activity)), \
			m_stopped(std::exchange(other.m_stopped, true)) \
		{ \
			\
		} \
		\
		~ActivityClassName() noexcept \
		{ \
			if (!m_stopped) \
			{ \
				const wil::FailureInfo* failureInfo = m_failureCache.GetFailure(); \
				if (failureInfo != nullptr) \
				{ \
					TraceLoggingWriteTagged( \
						m_activity, \
						#FunctionName"Error", \
						TraceLoggingLevel(TRACE_LEVEL_ERROR), \
						TraceLoggingString(failureInfo->pszFile, "File"), \
						TraceLoggingString(failureInfo->pszFunction, "Function"), \
						TraceLoggingUInt32(failureInfo->uLineNumber, "Line"), \
						TraceLoggingHexInt32(failureInfo->hr, "HResult")); \
				} \
				else \
				{ \
					/* Failure not logged by WIL error macro. We should ideally only hit this case for */ \
					/* std::bad_alloc exceptions. Other scenarios that might hit this case like */ \
					/* exceptions from invalid STL usage, unlogged errors, premature return without */ \
					/* stopping this activity, etc should be addressed. Assert here to try to catch */ \
					/* these scenarios during development. */ \
					WINRT_ASSERT(failureInfo != nullptr); \
					\
					TraceLoggingWriteTagged( \
						m_activity, \
						#FunctionName"Error", \
						TraceLoggingLevel(TRACE_LEVEL_ERROR), \
						TraceLoggingString("Untraced error occurred", "ErrorMessage")); \
				} \
				\
				TraceLoggingWriteStop(m_activity, #FunctionName"Stop"); \
			} \
		} \
		\
		static ActivityClassName Start() noexcept \
		{ \
			return ActivityClassName(); \
		} \
		\
		void Stop() noexcept \
		{ \
			if (!m_stopped) \
			{ \
				TraceLoggingWriteStop(m_activity, #FunctionName"Stop"); \
				m_stopped = true; \
			} \
		} \
		\
	private: \
		ActivityClassName() noexcept \
		{ \
			TraceLoggingWriteStart(m_activity, #FunctionName"Start"); \
		} \
		\
		wil::ThreadFailureCache m_failureCache; \
		TraceLoggingActivity<g_FFmpegInteropProvider> m_activity; \
		bool m_stopped{ false }; \
	} \

	TRACE_LOGGING_ACTIVITY_CLASS(CreateFromStream);
	TRACE_LOGGING_ACTIVITY_CLASS(CreateFromUri);
	TRACE_LOGGING_ACTIVITY_CLASS(OnStarting);
	TRACE_LOGGING_ACTIVITY_CLASS(OnSampleRequested);
	TRACE_LOGGING_ACTIVITY_CLASS(OnSwitchStreamsRequested);
	TRACE_LOGGING_ACTIVITY_CLASS(OnClosed);
}
