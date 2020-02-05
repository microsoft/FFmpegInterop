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

#include "NALUSampleProvider.h"

namespace winrt::FFmpegInterop::implementation
{
	constexpr uint8_t NALU_TYPE_HEVC_VPS{ 0x20 };
	constexpr uint8_t NALU_TYPE_HEVC_SPS{ 0x21 };
	constexpr uint8_t NALU_TYPE_HEVC_PPS{ 0x22 };

	class HEVCSampleProvider :
		public NALUSampleProvider
	{
	public:
		HEVCSampleProvider(_In_ const AVStream* stream, _In_ Reader& reader);
	};

	class HEVCConfigParser
	{
	public:
		HEVCConfigParser(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize);

		uint8_t GetNaluLengthSize() const noexcept;
		std::tuple<std::vector<uint8_t>, std::vector<uint32_t>> GetSpsPpsData() const;

	private:
		static constexpr size_t MIN_SIZE{ 23 };

		const uint8_t* m_data{ nullptr };
		const uint32_t m_dataSize{ 0 };
	};
}
