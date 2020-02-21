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

#include "pch.h"
#include "BitstreamReader.h"

using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	BitstreamReader::BitstreamReader(_In_reads_(dataSize) const uint8_t* data, _In_ uint32_t dataSize) :
		m_data(data),
		m_dataSize(dataSize)
	{
		THROW_HR_IF_NULL(E_INVALIDARG, data);
	}

	size_t BitstreamReader::BitsRemaining() const noexcept
	{
		if (m_dataSize > m_byteIndex)
		{
			return static_cast<size_t>(BITS_PER_BYTE) * (m_dataSize - m_byteIndex) - m_bitIndex;
		}
		else
		{
			return 0;
		}
	}

	void BitstreamReader::SkipN(_In_ uint32_t numBits)
	{
		// Make sure we have enough data left to fulfill the request
		THROW_HR_IF(MF_E_INVALID_POSITION, numBits > BitsRemaining());

		auto [numBytesToSkip, numBitsToSkip] = div(numBits, BITS_PER_BYTE);

		m_byteIndex += numBytesToSkip;
		m_bitIndex += numBitsToSkip;
		if (m_bitIndex == BITS_PER_BYTE)
		{
			m_byteIndex++;
			m_bitIndex = 0;
		}
	}

	uint32_t BitstreamReader::ReadN(_In_range_(<=, 32) uint32_t numBitsToRead)
	{
		WINRT_ASSERT(numBitsToRead <= 32);

		// Make sure we have enough data left to fulfill the read request
		THROW_HR_IF(MF_E_INVALID_POSITION, numBitsToRead > BitsRemaining());

		uint32_t result{ 0 };
		while (numBitsToRead > 0)
		{
			const uint8_t readSize{ static_cast<uint8_t>(min(numBitsToRead, static_cast<uint32_t>(BITS_PER_BYTE - m_bitIndex))) };
			const uint8_t readMask{ static_cast<uint8_t>((1 << readSize) - 1) };

			result <<= readSize;
			result |= (m_data[m_byteIndex] >> (BITS_PER_BYTE - m_bitIndex - readSize)) & readMask;

			m_bitIndex += readSize;
			if (m_bitIndex == BITS_PER_BYTE)
			{
				m_byteIndex++;
				m_bitIndex = 0;
			}

			numBitsToRead -= readSize;
		}

		return result;
	}

	void BitstreamReader::SkipExpGolomb()
	{
		uint8_t numLeadingZeroBits{ 0 };
		while (ReadN(1) != 1)
		{
			numLeadingZeroBits++;
		}

		SkipN(numLeadingZeroBits);
	}

	uint32_t BitstreamReader::ReadUExpGolomb()
	{
		uint8_t numLeadingZeroBits{ 0 };
		while (ReadN(1) != 1)
		{
			THROW_HR_IF(MF_E_INVALID_FILE_FORMAT, ++numLeadingZeroBits > 32);
		}

		return (1 << numLeadingZeroBits) - 1 + ReadN(numLeadingZeroBits);
	}

	int32_t BitstreamReader::ReadSExpGolomb()
	{
		uint32_t uExpGolomb{ ReadUExpGolomb() };
		if ((uExpGolomb & 1) == 0)
		{
			return -static_cast<int32_t>(uExpGolomb / 2);
		}
		else
		{
			return static_cast<int32_t>(uExpGolomb / 2) + 1;
		}
	}
}
