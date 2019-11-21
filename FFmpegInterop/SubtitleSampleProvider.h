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

#include <MediaSampleProvider.h>

namespace winrt::FFmpegInterop::implementation
{
	class SubtitleSampleProvider :
		public MediaSampleProvider
	{
	public:
		SubtitleSampleProvider(_In_ const AVStream* stream, _Inout_ FFmpegReader& reader);

		void GetSample(_Inout_ const Windows::Media::Core::MediaStreamSourceSampleRequest& request) override;
		void QueuePacket(_In_ AVPacket_ptr packet) override;

	protected:
		void Flush() noexcept override;

	private:
		Windows::Media::Core::MediaStreamSourceSampleRequest m_sampleRequest{ nullptr };
		Windows::Media::Core::MediaStreamSourceSampleRequestDeferral m_sampleRequestDeferral{ nullptr };
	};
}
