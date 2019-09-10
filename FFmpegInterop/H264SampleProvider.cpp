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

#include "pch.h"
#include "H264SampleProvider.h"

using namespace FFmpegInterop;

H264SampleProvider::H264SampleProvider(
	FFmpegReader^ reader,
	AVFormatContext* avFormatCtx,
	AVCodecContext* avCodecCtx)
	: MediaSampleProvider(reader, avFormatCtx, avCodecCtx)
{
}

H264SampleProvider::~H264SampleProvider()
{
}

HRESULT H264SampleProvider::WriteAVPacketToStream(DataWriter^ dataWriter, AVPacket* avPacket)
{
	HRESULT hr = S_OK;
	// On a KeyFrame, write the SPS and PPS
	if (avPacket->flags & AV_PKT_FLAG_KEY)
	{
		hr = GetSPSAndPPSBuffer(dataWriter);
	}

	if (SUCCEEDED(hr))
	{
		// Call base class method that simply write the packet to stream as is
		hr = MediaSampleProvider::WriteAVPacketToStream(dataWriter, avPacket);
	}

	// We have a complete frame
	return hr;
}

HRESULT H264SampleProvider::GetSPSAndPPSBuffer(DataWriter^ dataWriter)
{
	HRESULT hr = S_OK;

	if (!m_pAvCodecCtx || (m_pAvCodecCtx->extradata == nullptr && m_pAvCodecCtx->extradata_size < 8))
	{
		// The data isn't present
		hr = E_FAIL;
	}
	else
	{
		// Write both SPS and PPS sequence as is from extradata
		auto vSPSPPS = ref new Platform::Array<uint8_t>(m_pAvCodecCtx->extradata, m_pAvCodecCtx->extradata_size);
		dataWriter->WriteBytes(vSPSPPS);
	}

	return hr;
}