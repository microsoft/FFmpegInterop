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

#include "MediaSampleProvider.h"

namespace FFmpegInterop
{
	class H264AVCSampleProvider :
		public MediaSampleProvider
	{
	public:
		H264AVCSampleProvider(FFmpegReader& reader, const AVFormatContext* avFormatCtx, const AVCodecContext* avCodecCtx);

		HRESULT WriteAVPacketToStream(const winrt::Windows::Storage::Streams::DataWriter& dataWriter, const AVPacket_ptr& packet) override;

	private:
		HRESULT WriteNALPacket(const winrt::Windows::Storage::Streams::DataWriter& dataWriter, const AVPacket_ptr& packet);
		HRESULT GetSPSAndPPSBuffer(const winrt::Windows::Storage::Streams::DataWriter& dataWriter);
	};
}
