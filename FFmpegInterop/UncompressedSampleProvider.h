//*****************************************************************************
//
//	Copyright 2016 Microsoft Corporation
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

#include "SampleProvider.h"

namespace winrt::FFmpegInterop::implementation
{
	class UncompressedSampleProvider :
		public SampleProvider
	{
	public:
		using InitCodecContextFunc = void(*)(_In_ AVCodecContext* codecContext) noexcept;

		UncompressedSampleProvider(
			_In_ const AVFormatContext* formatContext,
			_In_ AVStream* stream, _In_ Reader& reader,
			_In_ uint32_t allowedDecodeErrors,
			_In_ InitCodecContextFunc initCodecContext);

	protected:
		void Flush() noexcept override;

		AVFrame_ptr GetFrame();

		AVCodecContext_ptr m_codecContext;
		bool m_sendInput{ true };
		uint32_t m_allowedDecodeErrors{ 0 };
	};
}
