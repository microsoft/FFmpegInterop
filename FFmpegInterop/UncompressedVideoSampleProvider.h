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

#include "UncompressedSampleProvider.h"

namespace winrt::FFmpegInterop::implementation
{
	// TODO: Minimize resource usage when stream is deselected
	class UncompressedVideoSampleProvider :
		public UncompressedSampleProvider
	{
	public:
		UncompressedVideoSampleProvider(_In_ const AVStream* stream, _Inout_ FFmpegReader& reader);

	protected:
		std::tuple<Windows::Storage::Streams::IBuffer, int64_t, int64_t> GetSampleData() override;
		void SetSampleProperties(const Windows::Media::Core::MediaStreamSample& sample) override;

	private:
		SwsContext_ptr m_swsContext;
		int m_lineSizes[4];
		AVBufferPool_ptr m_bufferPool;
	};
}
