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
#include "SubtitleSampleProvider.h"

using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::MediaProperties;

namespace winrt::FFmpegInterop::implementation
{
	SubtitleSampleProvider::SubtitleSampleProvider(_In_ const AVStream* stream, _In_ Reader& reader) :
		SampleProvider(stream, reader)
	{

	}

	void SubtitleSampleProvider::NotifyEOF() noexcept
	{
		SampleProvider::NotifyEOF();

		// If we're at EOS now, complete any deferred sample request
		if (m_isEOS && m_sampleRequestDeferral != nullptr)
		{
			TraceLoggingWrite(g_FFmpegInteropProvider, "DeferredSubtitleSampleRequestFilledEOS", TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
				TraceLoggingValue(m_stream->index, "StreamId"));

			m_sampleRequestDeferral.Complete();
			m_sampleRequest = nullptr;
			m_sampleRequestDeferral = nullptr;
		}
	}

	void SubtitleSampleProvider::Flush() noexcept
	{
		SampleProvider::Flush();

		// Drop any outstanding sample request
		if (m_sampleRequestDeferral != nullptr)
		{
			TraceLoggingWrite(g_FFmpegInteropProvider, "DeferredSubtitleSampleRequestDropped", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
				TraceLoggingValue(m_stream->index, "StreamId"));

			m_sampleRequest = nullptr;
			m_sampleRequestDeferral = nullptr;
		}
	}

	void SubtitleSampleProvider::QueuePacket(_In_ AVPacket_ptr packet)
	{
		SampleProvider::QueuePacket(move(packet));

		// Check if there's an outstanding sample request
		if (m_sampleRequestDeferral != nullptr)
		{
			SampleProvider::GetSample(m_sampleRequest);
			m_sampleRequestDeferral.Complete();

			m_sampleRequest = nullptr;
			m_sampleRequestDeferral = nullptr;

			TraceLoggingWrite(g_FFmpegInteropProvider, "DeferredSubtitleSampleRequestFilled", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
				TraceLoggingValue(m_stream->index, "StreamId"));
		}
	}

	void SubtitleSampleProvider::GetSample(_Inout_ const MediaStreamSourceSampleRequest& request)
	{
		if (HasPacket())
		{
			SampleProvider::GetSample(request);
		}
		else
		{
			// We should never have more than one outstanding sample request
			WINRT_ASSERT(m_sampleRequestDeferral == nullptr);

			// Check if we're at EOS
			THROW_HR_IF(MF_E_END_OF_STREAM, m_isEOS);

			// Request a deferral
			m_sampleRequest = request;
			m_sampleRequestDeferral = request.GetDeferral();

			TraceLoggingWrite(g_FFmpegInteropProvider, "DeferredSubtitleSampleRequest", TraceLoggingLevel(TRACE_LEVEL_VERBOSE), TraceLoggingPointer(this, "this"),
				TraceLoggingValue(m_stream->index, "StreamId"));
		}
	}
}

