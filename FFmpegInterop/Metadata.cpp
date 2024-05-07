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
#include "FFmpegInteropMSSConfig.h"

using namespace winrt::FFmpegInterop::implementation;
using namespace winrt;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Storage::Streams;
using namespace std;
using namespace wil;

namespace winrt::FFmpegInterop::implementation
{
	com_ptr<IPropertyStore> GetPropertyHandler(_In_ const MediaStreamSource& mss)
	{
		com_ptr<IMFGetService> mediaSource;
		THROW_IF_FAILED(mss.as<IMFGetService>()->GetService(MF_MEDIASOURCE_SERVICE, __uuidof(mediaSource), mediaSource.put_void()));

		com_ptr<IPropertyStore> propHandler;
		THROW_IF_FAILED(mediaSource->GetService(MF_PROPERTY_HANDLER_SERVICE, __uuidof(propHandler), propHandler.put_void()));

		return propHandler;
	}

	void PopulateMetadata(_In_ const MediaStreamSource& mss, _In_opt_ const AVDictionary* metadata)
	{
		if (metadata == nullptr)
		{
			return;
		}

		com_ptr<IPropertyStore> propHandler{ GetPropertyHandler(mss) };
#if DBG
		com_ptr<IPropertyStoreCapabilities> propHandlerCapabilties{ propHandler.as<IPropertyStoreCapabilities>() };
#endif // DBG

		const AVDictionaryEntry* tag{ nullptr };
		while(tag = av_dict_iterate(metadata, tag))
		{
			FFMPEG_INTEROP_TRACE("Property: Key = %hs, Value = %hs", tag->key, tag->value);

			// Check if we have a mapping for the property
			auto iter{ c_metadataPropertiesMap.find(tolower(tag->key)) };
			if (iter != c_metadataPropertiesMap.end())
			{
				const PropertyKey& propKey{ iter->second };
				unique_prop_variant prop;
				prop.vt = propKey.type;

				// Convert/parse the property value
				try
				{
					switch (propKey.type)
					{
					case VT_LPWSTR:
						prop.pwszVal = to_cotaskmem_string(tag->value).release();
						break;

					case VT_UI4:
						prop.ulVal = stoul(tag->value);
						break;

					default:
						WINRT_ASSERT(false);
						continue;
					}

#if DBG
					// The MSS property handler marks some properties read-only which prevents us from setting them
					WINRT_ASSERT(propHandlerCapabilties->IsPropertyWritable(propKey.key) == S_OK); // S_FALSE indicates the property is read-only
#endif // DBG
					THROW_IF_FAILED(propHandler->SetValue(propKey.key, prop));
				}
				catch (...)
				{
					LOG_CAUGHT_EXCEPTION_MSG("Failed to convert/parse/set the property value");
					continue;
				}

			}
		}
	}

	void SetThumbnail(
		_In_ const MediaStreamSource& mss,
		_In_ const AVStream* stream,
		_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		WINRT_ASSERT(stream->disposition == AV_DISPOSITION_ATTACHED_PIC);

		// Don't set a thumbnail on the MSS in media source app service scenarios. The remote stream reference could
		// outlive the app service connection. If the remote stream reference is released in the client app after the
		// app service has been suspended, then the remote release will block until the app service is terminated. This
		// could potentially cause the client app to become unresponsive during this time. 
		if (config != nullptr && config.IsMediaSourceAppService())
		{
			FFMPEG_INTEROP_TRACE("Stream %d: Not setting thumbnail because this is a media source app service", stream->index);
			return;
		}

		FFMPEG_INTEROP_TRACE("Stream %d: Setting thumbnail", stream->index);

		stream->attached_pic.data;
		stream->attached_pic.data;


		// Write the thumbnail to an in-memory stream
		AVPacket_ptr packet{ av_packet_clone(&stream->attached_pic) };
		THROW_IF_NULL_ALLOC(packet);

		IBuffer buf{ make<FFmpegInteropBuffer>(move(packet)) };

		InMemoryRandomAccessStream randomAccessStream;
		randomAccessStream.WriteAsync(buf).get();

		com_ptr<IStream> thumbnailStream;
		THROW_IF_FAILED(CreateStreamOverRandomAccessStream(
			winrt::get_unknown(randomAccessStream),
			__uuidof(thumbnailStream),
			thumbnailStream.put_void()));

		// We intentionally set PKEY_ThumbnailStream on the MSS property handler instead of using the MediaStreamSource.Thumbnail 
		// property in order to avoid a potential race condition. The MSS sets the PKEY_ThumbnailStream property for its
		// property handler asynchronously after the MediaStreamSource.Thumbnail property is set. This creates a window
		// of time where a property handler consumer could query for the PKEY_ThumbnailStream property before it's been
		// set and fail to retrieve the thumbnail.
		//
		// Note, however, that the MSS doesn't check its property handler for the PKEY_ThumbnailStream property when a
		// consumer gets the MediaStreamSource.Thumbnail property... This isn't an issue if the MSS consumer is another
		// Windows Media component though, as MF uses the IPropertyHandler interface rather than the IMediaStreamSource
		// interface for retrieving metadata.
		unique_prop_variant thumbnailProp;
		thumbnailProp.vt = VT_STREAM;
		thumbnailProp.pStream = thumbnailStream.detach();

		com_ptr<IPropertyStore> propHandler{ GetPropertyHandler(mss) };
		THROW_IF_FAILED(propHandler->SetValue(PKEY_ThumbnailStream, thumbnailProp));
	}
}
