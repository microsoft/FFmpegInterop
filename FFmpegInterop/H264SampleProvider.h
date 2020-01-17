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

#include "SampleProvider.h"

namespace winrt::FFmpegInterop::implementation
{
	class H264SampleProvider :
		public SampleProvider
	{
	public:
		H264SampleProvider(_In_ const AVStream* stream, _In_ Reader& reader);

		void SetEncodingProperties(_Inout_ const Windows::Media::MediaProperties::IMediaEncodingProperties& encProp) override;

	protected:
		std::tuple<Windows::Storage::Streams::IBuffer, int64_t, int64_t, std::map<GUID, Windows::Foundation::IInspectable>> GetSampleData() override;

	private:
		static constexpr uint8_t NALU_START_CODE[]{ 0x00, 0x00, 0x00, 0x01 };
		static constexpr uint8_t NALU_TYPE_AUD{ 0x1F };

		class AVCCodecPrivate
		{
		public:
			AVCCodecPrivate(_In_reads_(codecPrivateDataSize) const uint8_t* codecPrivateData, _In_ int codecPrivateDataSize);

			uint8_t GetProfile() const { return m_profile; }
			uint8_t GetLevel() const { return m_level; }
			uint8_t GetNaluLengthSize() const { return m_naluLengthSize; }
			const std::vector<uint8_t>& GetSpsPpsData() const { return m_spsPpsData; }
			const std::vector<uint32_t>& GetSpsPpsNaluLengths() const { return m_spsPpsNaluLengths; }

		private:
			static constexpr size_t MIN_SIZE{ 7 };

			uint32_t ParseParameterSets(
				_In_ uint8_t parameterSetCount,
				_In_reads_(codecPrivateDataSize) const uint8_t* codecPrivateData,
				_In_ uint32_t codecPrivateDataSize);

			uint8_t m_profile;
			uint8_t m_level;
			uint8_t m_naluLengthSize;
			std::vector<uint8_t> m_spsPpsData;
			std::vector<uint32_t> m_spsPpsNaluLengths;
		};

		class AVCSequenceParameterSet
		{
		public:
			AVCSequenceParameterSet(_In_reads_(dataSize) const uint8_t* data, _In_ int dataSize);

			uint8_t GetProfile() const { return m_profile; }
			bool GetConstraintSet1() const { return m_constraintSet1; }

		private:
			uint8_t m_profile;
			bool m_constraintSet1;
		};

		class AVCPictureParamterSet
		{
		public:
			AVCPictureParamterSet(_In_reads_(dataSize) const uint8_t* data, _In_ int dataSize);

			uint32_t GetNumSliceGroups() const { return m_numSliceGroups; }

		private:
			uint32_t m_numSliceGroups;
		};

		std::tuple<Windows::Storage::Streams::IBuffer, std::vector<uint32_t>> TransformSample(_Inout_ AVPacket_ptr packet);

		AVCCodecPrivate m_avcCodecPrivate;
	};
}
