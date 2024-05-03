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

	void PopulateMetadata(
		_In_ const MediaStreamSource& mss,
		_In_ const AVFormatContext* formatContext,
		_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		PopulateMetadata(mss, formatContext->metadata, config);

		// TODO (brbeec): Add container-specific metadata?
	}

	void PopulateMetadata(
		_In_ const MediaStreamSource& mss,
		_In_ const AVStream* stream,
		_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		PopulateMetadata(mss, stream->metadata, config);

		// TODO (brbeec): Add stream-specific metadata?
	}

	void PopulateMetadata(
		_In_ const MediaStreamSource& mss,
		_In_opt_ const AVDictionary* metadata,
		_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		if (metadata == nullptr)
		{
			// Nothing to do
			return;
		}

		com_ptr<IPropertyStore> propHandler{ GetPropertyHandler(mss) };
		bool isMediaSourceAppService{ config != nullptr && config.IsMediaSourceAppService() };

		const AVDictionaryEntry* tag{ nullptr };
		while(tag = av_dict_iterate(metadata, tag))
		{
			FFMPEG_INTEROP_TRACE("Property: Key = %hs, Value = %hs", tag->key, tag->value);

			// Check if we have a mapping for property
			auto iter{ c_metadataPropertiesMap.find(tolower(tag->key)) };
			if (iter != c_metadataPropertiesMap.end())
			{
				const PropertyKey& propKey{ iter->second };
				unique_prop_variant prop;
				prop.vt = propKey.type;

				// TODO (brbeec): Is this needed?
				if (isMediaSourceAppService)
				{
					// Don't set property values that are object pointers in media source app service scenarios. The remote
					// stream reference could outlive the app service connection. If the remote stream reference is
					// released after the app  service has been suspended, then the remote release will block until the
					// app service is terminated. This could cause the client app (e.g. File Explorer) to become
					// unresponsive during this time.
					switch (propKey.type)
					{
					case VT_LPWSTR:
						continue;
					}
				}

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
						// Unhandled property type
						WINRT_ASSERT(false);
						break;
					}

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
		_In_ const AVStream* thumbnailStream,
		_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		// Don't set a thumbnail on the MSS in media source app service scenarios. The remote stream reference could
		// outlive the app service connection. If the remote stream reference is released after the app  service has
		// been suspended, then the remote release will block until the app service is terminated. This could cause the
		// client app (e.g. File Explorer) to become unresponsive during this time. 
		if (config != nullptr && !config.IsMediaSourceAppService())
		{
			return;
		}

		// Write the thumbnail to an in-memory stream
		AVPacket_ptr packet{ av_packet_clone(&thumbnailStream->attached_pic) };
		THROW_IF_NULL_ALLOC(packet);

		IBuffer buf{ make<FFmpegInteropBuffer>(move(packet)) };

		InMemoryRandomAccessStream randomAccessStream;
		randomAccessStream.WriteAsync(buf).get();

		com_ptr<IStream> stream;
		THROW_IF_FAILED(CreateStreamOverRandomAccessStream(
			static_cast<::IUnknown*>(get_abi(randomAccessStream)),
			__uuidof(stream),
			stream.put_void()));

		// We set the thumbnail on the MSS property handler directly instead of using the MediaStreamSource.Thumbnail
		// property to avoid a potential race condition. The MSS populates the the thumbnail property for its property
		// handler asynchronously after the MediaStreamSource.Thumbnail property is set. However, internally MF and its
		// consumers typically use Win32 interfaces (i.e. IPropertyStore) rather than WinRT interfaces (i.e. IMediaStreamSource),
		// and may try to get the thumbnail property from the property handler before the MSS has set it.
		unique_prop_variant thumbnailProp;
		thumbnailProp.vt = VT_STREAM;
		thumbnailProp.pStream = stream.detach();

		com_ptr<IPropertyStore> propHandler{ GetPropertyHandler(mss) };
		THROW_IF_FAILED(propHandler->SetValue(PKEY_ThumbnailStream, thumbnailProp));
	}
}
