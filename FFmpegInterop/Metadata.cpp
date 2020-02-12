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
#include "Metadata.h"

using namespace winrt::FFmpegInterop::implementation;
using namespace winrt;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace winrt::FFmpegInterop::implementation
{
	void PopulateMSSMetadata(_In_ const MediaStreamSource& mss, _In_ const AVDictionary* metadata)
	{
		// Get the MSS property handler
		com_ptr<IMFGetService> mediaSource;
		THROW_IF_FAILED(mss.as<IMFGetService>()->GetService(MF_MEDIASOURCE_SERVICE, __uuidof(mediaSource), mediaSource.put_void()));

		com_ptr<IPropertyStore> propHandler;
		THROW_IF_FAILED(mediaSource->GetService(MF_PROPERTY_HANDLER_SERVICE, __uuidof(propHandler), propHandler.put_void()));

		// TODO: Populate the property handler with the metadata
	}

	void SetMSSThumbnail(_In_ const MediaStreamSource& mss, _In_ const AVStream* thumbnailStream)
	{
		// Write the thumbnail to a stream
		AVPacket_ptr packet{ av_packet_clone(&thumbnailStream->attached_pic) };
		THROW_IF_NULL_ALLOC(packet);

		IBuffer buf{ make<FFmpegInteropBuffer>(move(packet)) };

		InMemoryRandomAccessStream randomAccessStream;
		randomAccessStream.WriteAsync(buf).get();

		// Set the thumbnail property on the MSS
		mss.Thumbnail(RandomAccessStreamReference::CreateFromStream(randomAccessStream));
	}
}
