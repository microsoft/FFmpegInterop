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
#include "MainPage.h"
#include "MainPage.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Popups;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Pickers;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Media::Core;
using namespace winrt::FFmpegInterop;
using namespace std;

namespace winrt::MediaPlayerCPP::implementation
{
	MainPage::MainPage()
	{
		InitializeComponent();

		// Show the control panel on startup so user can start opening media
		Splitter().IsPaneOpen(true);
	}

	fire_and_forget MainPage::OpenFileAsync(const IInspectable&, const RoutedEventArgs&)
	{
		// Take a strong reference to prevent *this* from being prematurely destructed
		auto lifetime{ get_strong() };

		// Open the file picker
		FileOpenPicker filePicker;
		filePicker.ViewMode(PickerViewMode::Thumbnail);
		filePicker.SuggestedStartLocation(PickerLocationId::VideosLibrary);
		filePicker.FileTypeFilter().Append(L"*");

		StorageFile file{ co_await filePicker.PickSingleFileAsync() };

		// Check if the user selected a file
		if (file != nullptr)
		{
			// Populate the URI box with the path of the file selected
			UriBox().Text(file.Path());

			OpenStream(co_await file.OpenReadAsync());
		}
	}

	void MainPage::UriBoxKeyUp(const IInspectable& sender, const KeyRoutedEventArgs& e)
	{
		hstring uri{ sender.as<TextBox>().Text() };

		// Only respond when the text box is not empty and after Enter key is pressed
		if (e.Key() == Windows::System::VirtualKey::Enter && !uri.empty())
		{
			OpenUri(uri);
		}
	}

	void MainPage::OpenUri(const hstring& uri)
	{
		OpenMedia([&uri](const MediaStreamSource& mss)
		{
			FFmpegInteropMSS::CreateFromUri(uri, mss);
		});
	}

	void MainPage::OpenStream(const IRandomAccessStream& stream)
	{
		// TODO: Validate this code path with full FFmpeg build
		OpenMedia([&stream](const MediaStreamSource& mss)
		{
			FFmpegInteropMSS::CreateFromStream(stream, mss);
		});
	}

	void MainPage::OpenMedia(function<void(const MediaStreamSource&)> createFunc)
	{
		// Stop any media currently playing
		MediaElement().Stop();

		try
		{
			// Create the media source
			auto mssFactory{ get_activation_factory<MediaStreamSource>() };
			auto mss{ mssFactory.ActivateInstance<MediaStreamSource>() };
			createFunc(mss);

			// Set the MSS as the media element's source
			MediaElement().SetMediaStreamSource(mss);

			// Close the control panel
			Splitter().IsPaneOpen(false);
		}
		catch (...)
		{
			OnError(L"Failed to open media");
		}
	}

	void MainPage::MediaFailed(const IInspectable&, const ExceptionRoutedEventArgs& e)
	{
		OnError(e.ErrorMessage());
	}

	void MainPage::OnError(const hstring& errMsg)
	{
		// Display an error message
		MessageDialog dialog{ errMsg };
		(void) dialog.ShowAsync();

		// Open the control panel
		Splitter().IsPaneOpen(true);
	}
}
