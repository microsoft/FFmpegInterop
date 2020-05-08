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
	class UncompressedVideoSampleProvider :
		public UncompressedSampleProvider
	{
	public:
		UncompressedVideoSampleProvider(_In_ const AVFormatContext* formatContext, _In_ AVStream* stream, _In_ Reader& reader, _In_ uint32_t allowedDecodeErrors);

		void SetEncodingProperties(_Inout_ const Windows::Media::MediaProperties::IMediaEncodingProperties& encProp, _In_ bool setFormatUserData) override;

	protected:
		std::tuple<Windows::Storage::Streams::IBuffer, int64_t, int64_t, std::vector<std::pair<GUID, Windows::Foundation::IInspectable>>, std::vector<std::pair<GUID, Windows::Foundation::IInspectable>>> GetSampleData() override;

	private:
		static constexpr AVPixelFormat DEFAULT_FORMAT{ AV_PIX_FMT_NV12 };
		static std::tuple<bool, GUID> MapAVSampleFormatToMFVideoFormat(_In_ AVPixelFormat format) noexcept;
		static bool IsSupportedFormat(_In_ AVPixelFormat format) noexcept { auto [isSupported, mfVideoFormat] = MapAVSampleFormatToMFVideoFormat(format); return isSupported; }
		static GUID GetMFVideoFormat(_In_ AVPixelFormat format) noexcept { auto [isSupported, mfVideoFormat] = MapAVSampleFormatToMFVideoFormat(format); return mfVideoFormat; }
		static void InitCodecContext(_In_ AVCodecContext* codecContext) noexcept;

		static AVPixelFormat GetFormat(_In_ AVCodecContext* codecContext, _In_ const AVPixelFormat* formats) noexcept;

		void InitScaler();
		bool IsUsingScaler() const noexcept { return m_swsContext != nullptr; }
		void InitBufferPool(_In_ AVPixelFormat format);
		std::vector<std::pair<GUID, Windows::Foundation::IInspectable>> CheckForFormatChanges(_In_ const AVFrame* frame);
		std::vector<std::pair<GUID, Windows::Foundation::IInspectable>> GetSampleProperties(_In_ const AVFrame* frame);

		int m_outputWidth{ 0 };
		int m_outputHeight{ 0 };
		SwsContext_ptr m_swsContext;
		int m_lineSizes[4]{ 0, 0, 0, 0};
		AVBufferPool_ptr m_bufferPool;
	};
}
