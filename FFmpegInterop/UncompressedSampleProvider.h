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

#include "MediaSampleProvider.h"

namespace FFmpegInterop
{
	class UncompressedSampleProvider :
		public MediaSampleProvider
	{
	public:
		UncompressedSampleProvider(FFmpegReader& reader, const AVFormatContext* avFormatCtx, AVCodecContext* avCodecCtx);

		HRESULT DecodeAVPacket(const winrt::Windows::Storage::Streams::DataWriter& dataWriter, const AVPacket_ptr& packet, int64_t& framePts, int64_t& frameDuration) override;

	protected:
		virtual HRESULT ProcessDecodedFrame(const winrt::Windows::Storage::Streams::DataWriter& dataWriter) = 0;
	};
}
