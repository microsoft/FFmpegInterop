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

#include "MainPage.g.h"

namespace winrt::MediaPlayerCPP::implementation
{
	struct MainPage :
		public MainPageT<MainPage>
	{
	public:
		MainPage();

		fire_and_forget OpenFileAsync(_In_ const Windows::Foundation::IInspectable& sender, _In_ const Windows::UI::Xaml::RoutedEventArgs& args);
		fire_and_forget OnFileActivated(_In_ const Windows::Storage::StorageFile& file);
		void OnUriBoxKeyUp(_In_ const Windows::Foundation::IInspectable& sender, _In_ const Windows::UI::Xaml::Input::KeyRoutedEventArgs& args);
		void OnMediaFailed(_In_ const Windows::Foundation::IInspectable& sender, _In_ const Windows::UI::Xaml::ExceptionRoutedEventArgs& args);

	private:
		void OpenStream(_In_ const Windows::Storage::Streams::IRandomAccessStream& stream);
		void OpenUri(_In_ const hstring& uri);
		void OpenMedia(_In_ std::function<void(const Windows::Media::Core::MediaStreamSource&)> createFunc);
		void OnError(_In_ const hstring& errMsg);
	};
}

namespace winrt::MediaPlayerCPP::factory_implementation
{
	struct MainPage :
		public MainPageT<MainPage, implementation::MainPage>
	{

	};
}
