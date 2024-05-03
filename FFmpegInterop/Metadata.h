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

namespace winrt::FFmpegInterop
{
	struct FFmpegInteropMSSConfig;
}

namespace winrt::FFmpegInterop::implementation
{
	struct PropertyKey
	{
		PropertyKey(_In_ PROPERTYKEY key, _In_ VARTYPE type) :
			key(key),
			type(type)
		{

		}

		PROPERTYKEY key;
		VARTYPE type;
	};

	inline const std::unordered_map<std::string, PropertyKey> c_metadataPropertiesMap
	{
		{ "album", { PKEY_Music_AlbumTitle, VT_LPWSTR } },
		{ "album_artist", { PKEY_Music_AlbumArtist, VT_LPWSTR } },
		{ "artist", { PKEY_Music_Artist, VT_LPWSTR } },
		{ "bpm", { PKEY_Music_BeatsPerMinute, VT_LPWSTR } },
		{ "conductor", { PKEY_Music_Conductor, VT_LPWSTR } },
		{ "comment", { PKEY_Comment, VT_LPWSTR } },
		{ "composer", { PKEY_Music_Composer, VT_LPWSTR } },
		// { "copyright", { PKEY_Copyright, VT_LPWSTR } }, // The MSS property handler prevents us from setting this property
		{ "date", { PKEY_Media_Year, VT_UI4 } },
		{ "date_released", { PKEY_Media_DateReleased, VT_LPWSTR } },
		{ "director", { PKEY_Video_Director, VT_LPWSTR } },
		{ "disc", { PKEY_Music_DiscNumber, VT_UI4 } },
		{ "encoded_by", { PKEY_Media_EncodedBy, VT_LPWSTR } },
		{ "encoder", { PKEY_Media_EncodedBy, VT_LPWSTR } },
		{ "encoder_options", { PKEY_Media_EncodingSettings, VT_LPWSTR } },
		{ "encoder_settings", { PKEY_Media_EncodingSettings, VT_LPWSTR } },
		{ "filename", { PKEY_OriginalFileName, VT_LPWSTR } },
		{ "genre", { PKEY_Music_Genre, VT_VECTOR | VT_LPWSTR } },
		{ "initial_key", { PKEY_Music_InitialKey, VT_LPWSTR } },
		{ "keywords", { PKEY_Keywords, VT_LPWSTR } },
		{ "language", { PKEY_Language, VT_LPWSTR } },
		{ "law_rating", { PKEY_ParentalRating, VT_LPWSTR } },
		{ "lyrics", { PKEY_Music_Lyrics, VT_LPWSTR } },
		{ "mood", { PKEY_Music_Mood, VT_LPWSTR } },
		{ "period", { PKEY_Music_Period, VT_LPWSTR } },
		{ "producer", { PKEY_Media_Producer, VT_LPWSTR } },
		{ "publisher", { PKEY_Media_Publisher, VT_LPWSTR } },
		{ "title", { PKEY_Title, VT_LPWSTR } },
		{ "rating", { PKEY_Rating, VT_UI4 } },
		{ "subtitle", { PKEY_Media_SubTitle, VT_LPWSTR } },
		{ "track", { PKEY_Music_TrackNumber, VT_UI4 } },
		{ "written_by", { PKEY_Media_Writer, VT_LPWSTR } },
	};

	com_ptr<IPropertyStore> GetPropertyHandler(_In_ const Windows::Media::Core::MediaStreamSource& mss);

	void PopulateMetadata(
		_In_ const Windows::Media::Core::MediaStreamSource& mss,
		_In_ const AVFormatContext* formatContext,
		_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);

	void PopulateMetadata(
		_In_ const Windows::Media::Core::MediaStreamSource& mss,
		_In_ const AVStream* stream,
		_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);

	void PopulateMetadata(
		_In_ const Windows::Media::Core::MediaStreamSource& mss,
		_In_ const AVDictionary* metadata,
		_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);

	void SetThumbnail(
		_In_ const Windows::Media::Core::MediaStreamSource& mss,
		_In_ const AVStream* thumbnailStream,
		_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);
}
