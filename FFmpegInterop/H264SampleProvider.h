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

	private:
		static constexpr size_t MIN_SIZE{ 7 };

		uint32_t ParseParameterSets(
			_In_ uint8_t parameterSetCount,
			_In_reads_(dataSize) const uint8_t* data,
			_In_ uint32_t dataSize,
			_Inout_ std::vector<uint8_t>& spsPpsData,
			_Inout_ std::vector<uint32_t>& spsPpsNaluLengths) const;

		const uint8_t* m_data{ nullptr };
		const uint32_t m_dataSize{ 0 };
	};

	class AVCSequenceParameterSetParser
	{
	public:
		AVCSequenceParameterSetParser(_In_reads_(dataSize) const uint8_t* data, _In_ int dataSize);

		uint8_t GetProfile() const { return m_profile; }
		bool GetConstraintSet1() const { return m_constraintSet1; }

	private:
		uint8_t m_profile{ 0 };
		bool m_constraintSet1{ false };
	};

	class AVCPictureParamterSetParser
	{
	public:
		AVCPictureParamterSetParser(_In_reads_(dataSize) const uint8_t* data, _In_ int dataSize);

		uint32_t GetNumSliceGroups() const { return m_numSliceGroups; }

	private:
		uint32_t m_numSliceGroups{ 0 };
	};
}
