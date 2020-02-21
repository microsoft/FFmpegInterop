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

namespace winrt::FFmpegInterop::implementation
{
	class BitstreamReader
	{
	public:
		BitstreamReader(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize);

		void SkipN(_In_ uint32_t numBits);
		void SkipExpGolomb();

		bool Read1() { return static_cast<bool>(ReadN(1)); }
		uint8_t Read8() { return static_cast<uint8_t>(ReadN(8)); }
		uint16_t Read16() { return static_cast<uint16_t>(ReadN(16)); }
		uint32_t Read32() { return ReadN(32); }
		uint32_t ReadUExpGolomb();
		int32_t ReadSExpGolomb();

	private:
		size_t BitsRemaining() const noexcept;
		uint32_t ReadN(_In_range_(<= , 32) uint32_t numBitsToRead);

		const uint8_t* m_data{ nullptr };
		uint32_t m_dataSize{ 0 };
		_Field_range_(<=, m_dataSize) uint32_t m_byteIndex{ 0 };
		_Field_range_(< , BITS_PER_BYTE) uint8_t m_bitIndex { 0 };
	};
}
