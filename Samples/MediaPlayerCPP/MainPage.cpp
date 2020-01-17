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

using namespace winrt::MediaPlayerCPP::implementation;
using namespace winrt;
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
using namespace winrt::Windows::Media::Playback;
using namespace winrt::FFmpegInterop;
using namespace std;

MainPage::MainPage()
{
	InitializeComponent();

	// Show the control panel on startup so user can start opening media
	splitter().IsPaneOpen(true);
}

fire_and_forget MainPage::OpenFilePicker(_In_ const IInspectable&, _In_ const RoutedEventArgs&)
{
	// Open the file picker
	FileOpenPicker filePicker;
	filePicker.ViewMode(PickerViewMode::Thumbnail);
	filePicker.SuggestedStartLocation(PickerLocationId::VideosLibrary);
	filePicker.FileTypeFilter().Append(L"*");

	StorageFile file{ co_await filePicker.PickSingleFileAsync() };

	// Check if the user selected a file
	if (file != nullptr)
	{
		OpenFile(file);
	}
}

void MainPage::OnFileActivated(_In_ const StorageFile& file)
{
	OpenFile(file);
}

fire_and_forget MainPage::OpenFile(_In_ const StorageFile& file)
{
	// Populate the URI box with the path of the file selected
	uriBox().Text(file.Path());

	IRandomAccessStream stream{ co_await file.OpenReadAsync() };

	OpenMedia([&stream](const MediaStreamSource& mss, const FFmpegInteropMSSConfig& config)
	{
		FFmpegInteropMSS::CreateFromStream(stream, mss, config);
	});
}

void MainPage::OnUriBoxKeyUp(_In_ const IInspectable& sender, _In_ const KeyRoutedEventArgs& args)
{
	hstring uri{ sender.as<TextBox>().Text() };

	// Only respond when the text box is not empty and after Enter key is pressed
	if (args.Key() == Windows::System::VirtualKey::Enter && !uri.empty())
	{
		// Mark event as handled to prevent duplicate event to re-triggered
		args.Handled(true);

		OpenUri(uri);
	}
}

void MainPage::OpenUri(_In_ const hstring& uri)
{
	// TODO: Validate this code path with full FFmpeg build
	OpenMedia([&uri](const MediaStreamSource& mss, const FFmpegInteropMSSConfig& config)
	{
		FFmpegInteropMSS::CreateFromUri(uri, mss, config);
	});
}

void MainPage::OpenMedia(_In_ function<void(const MediaStreamSource&, const FFmpegInteropMSSConfig&)> createFunc)
{
	// Stop any media currently playing
	mediaElement().Stop();

	try
	{
		// Create the media source
		IActivationFactory mssFactory{ get_activation_factory<MediaStreamSource>() };
		MediaStreamSource mss{ mssFactory.ActivateInstance<MediaStreamSource>() };
		
		FFmpegInteropMSSConfig config;
		config.ForceAudioDecode(toggleSwitchAudioDecode().IsOn());
		config.ForceVideoDecode(toggleSwitchVideoDecode().IsOn());
		
		createFunc(mss, config);

		// Set the MSS as the media element's source
		mediaElement().SetMediaStreamSource(move(mss));

		// Close the control panel
		splitter().IsPaneOpen(false);
	}
	catch (...)
	{
		OnError(L"Failed to open media");
	}
}

void MainPage::OnMediaFailed(_In_ const IInspectable&, _In_ const ExceptionRoutedEventArgs& args)
{
	OnError(args.ErrorMessage());
}

fire_and_forget MainPage::OnError(_In_ const hstring& errMsg)
{
	// Display an error message
	MessageDialog dialog{ errMsg };
	co_await dialog.ShowAsync();

	// Open the control panel
	splitter().IsPaneOpen(true);
}
