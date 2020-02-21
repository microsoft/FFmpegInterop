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

#include "NALUSampleProvider.h"

namespace winrt::FFmpegInterop::implementation
{
	class H264SampleProvider :
		public NALUSampleProvider
	{
	public:
		H264SampleProvider(_In_ const AVStream* stream, _In_ Reader& reader);

		void SetEncodingProperties(_Inout_ const Windows::Media::MediaProperties::IMediaEncodingProperties& encProp, _In_ bool setFormatUserData) override;
	};

	class AVCConfigParser
	{
	public:
		AVCConfigParser(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize);

		uint8_t GetNaluLengthSize() const noexcept;
		std::tuple<std::vector<uint8_t>, std::vector<uint32_t>> GetSpsPpsData() const;
		bool HasNoFMOASO() const;

	private:
		static constexpr size_t MIN_SIZE{ 7 };

		uint32_t ParseParameterSets(
			_In_ uint8_t parameterSetCount,
			_In_reads_(dataSize) const uint8_t* data,
			_In_ uint32_t dataSize,
			_Inout_ std::vector<uint8_t>& spsPpsData,
			_Inout_ std::vector<uint32_t>& spsPpsNaluLengths) const;

		const uint8_t* m_data{ nullptr };
		uint32_t m_dataSize{ 0 };
	};

	class AVCSequenceParameterSet
	{
	public:
		AVCSequenceParameterSet(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize);

		uint8_t GetProfile() const noexcept { return m_profile; }
		uint8_t GetProfileCompatibility() const noexcept { return m_profileCompatibility; }
		bool GetConstraintSet0() const noexcept { return m_profileCompatibility & 0x80; }
		bool GetConstraintSet1() const noexcept { return m_profileCompatibility & 0x40; }
		bool GetConstraintSet2() const noexcept { return m_profileCompatibility & 0x20; }
		bool GetConstraintSet3() const noexcept { return m_profileCompatibility & 0x10; }
		bool GetConstraintSet4() const noexcept { return m_profileCompatibility & 0x08; }
		bool GetConstraintSet5() const noexcept { return m_profileCompatibility & 0x04; }
		uint8_t GetLevel() const noexcept{ return m_level; }
		uint32_t GetSpsId() const noexcept { return m_spsId; }

		bool HasNonConstrainedBaseline() const noexcept { return m_profile == FF_PROFILE_H264_BASELINE && !GetConstraintSet1(); }

	private:
		uint8_t m_profile{ 0 };
		uint8_t m_profileCompatibility{ 0 };
		uint8_t m_level{ 0 };
		uint32_t m_spsId{ 0 };
	};

	class AVCPictureParameterSet
	{
	public:
		AVCPictureParameterSet(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize);

		uint32_t GetPpsId() const noexcept { return m_ppsId;  }
		uint32_t GetSpsId() const noexcept { return m_spsId; }
		bool GetEntropyCodingModeFlag() const noexcept { return m_entropyCodingModeFlag; }
		bool GetPicOrderPresentFlag() const noexcept { return m_picOrderPresentFlag; }
		uint32_t GetNumSliceGroups() const noexcept { return m_numSliceGroups; }

	private:
		uint32_t m_ppsId{ 0 };
		uint32_t m_spsId{ 0 };
		bool m_entropyCodingModeFlag{ false };
		bool m_picOrderPresentFlag{ false };
		uint32_t m_numSliceGroups{ 0 };
	};
}
