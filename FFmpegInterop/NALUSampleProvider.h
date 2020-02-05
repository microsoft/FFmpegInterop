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
	constexpr uint8_t NALU_START_CODE[]{ 0x00, 0x00, 0x00, 0x01 };
	constexpr uint8_t NALU_TYPE_AUD{ 0x1F };

	uint32_t GetAnnexBNaluLength(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize);
	uint32_t GetAVCNaluLength(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize, _In_ uint8_t naluLengthSize);

	class NALUSampleProvider :
		public SampleProvider
	{
	public:
		NALUSampleProvider(_In_ const AVStream* stream, _In_ Reader& reader);

		void SetEncodingProperties(_Inout_ const Windows::Media::MediaProperties::IMediaEncodingProperties& encProp, _In_ bool setFormatUserData) override;

	protected:
		std::tuple<Windows::Storage::Streams::IBuffer, int64_t, int64_t, std::map<GUID, Windows::Foundation::IInspectable>> GetSampleData() override;

		bool m_isBitstreamAnnexB{ true };
		uint8_t m_naluLengthSize{ 0 }; // Only valid when bitstream is *not* Annex B
		std::vector<uint8_t> m_spsPpsData;
		std::vector<uint32_t> m_spsPpsNaluLengths;

	private:
		static constexpr size_t MAX_NALU_NUM_SUPPORTED{ 512 };

		std::tuple<Windows::Storage::Streams::IBuffer, std::vector<uint32_t>> TransformSample(_Inout_ AVPacket_ptr packet, _In_ bool isKeyFrame);
	};

	class AnnexBParser
	{
	public:
		AnnexBParser(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize);

		std::tuple<std::vector<uint8_t>, std::vector<uint32_t>> GetSpsPpsData() const;

	private:
		const uint8_t* m_data{ nullptr };
		const uint32_t m_dataSize{ 0 };
	};
}
