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

//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace FFmpeg;
using namespace FFMPEGMediaStreamSource;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Media::Core;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Popups;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

MainPage::MainPage() :
forceDecodeAudio(false),
forceDecodeVideo(false)
{
	InitializeComponent();
}

/// <summary>
/// Handle the returned files from file picker
/// This method is triggered by ContinuationManager based on ActivationKind
/// </summary>
/// <param name="args">File open picker continuation activation argment. It cantains the list of files user selected with file open picker </param>
void FFMPEGMediaStreamSource::MainPage::ContinueFileOpenPicker(Windows::ApplicationModel::Activation::FileOpenPickerContinuationEventArgs^ args)
{
	if (args->Files->Size > 0)
	{
		media->Stop();

		StorageFile^ file = args->Files->GetAt(0);
		create_task(file->OpenAsync(FileAccessMode::Read)).then([this, file](task<IRandomAccessStream^> stream)
		{
			try
			{
				// Instantiate FFmpeg object and pass the stream from opened file
				IRandomAccessStream^ readStream = stream.get();
				FFMPEGLib = ref new FFmpegLibrary(readStream, forceDecodeAudio, forceDecodeVideo);
				MediaStreamSource^ mss = FFMPEGLib->GetMediaStreamSource();

				if (mss)
				{
					// Pass MediaStreamSource to Media Element
					media->SetMediaStreamSource(mss);
				}
				else
				{
					// Display error message
					auto errorDialog = ref new MessageDialog("Cannot open media");
					errorDialog->ShowAsync();
				}
			}
			catch (COMException^ ex)
			{
				// Display error message
				auto errorDialog = ref new MessageDialog("Cannot open file");
				errorDialog->ShowAsync();
			}
		});
	}
}

void FFMPEGMediaStreamSource::MainPage::AppBarButton_Browse_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	FileOpenPicker^ filePicker = ref new FileOpenPicker();
	filePicker->ViewMode = PickerViewMode::Thumbnail;
	filePicker->SuggestedStartLocation = PickerLocationId::VideosLibrary;
	filePicker->FileTypeFilter->Append("*");

	filePicker->PickSingleFileAndContinue();
}

void FFMPEGMediaStreamSource::MainPage::AppBarButton_Audio_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto button = dynamic_cast<AppBarToggleButton^>(sender);
	forceDecodeAudio = button->IsChecked->Value;

	if (button->IsChecked->Value)
	{
		button->Label = "Enable audio auto decoding";
	}
	else
	{
		button->Label = "Disable audio auto decoding";
	}
}

void FFMPEGMediaStreamSource::MainPage::AppBarButton_Video_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto button = dynamic_cast<AppBarToggleButton^>(sender);
	forceDecodeVideo = button->IsChecked->Value;

	if (button->IsChecked->Value)
	{
		button->Label = "Enable video auto decoding";
	}
	else
	{
		button->Label = "Disable video auto decoding";
	}
}

void FFMPEGMediaStreamSource::MainPage::CommandBar_Opened(Platform::Object^ sender, Platform::Object^ e)
{
	this->BottomAppBar->Opacity = 1.0;
}

void FFMPEGMediaStreamSource::MainPage::CommandBar_Closed(Platform::Object^ sender, Platform::Object^ e)
{
	this->BottomAppBar->Opacity = 0.0;
}

void FFMPEGMediaStreamSource::MainPage::media_MediaEnded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
}

void FFMPEGMediaStreamSource::MainPage::media_MediaFailed(Platform::Object^ sender, Windows::UI::Xaml::ExceptionRoutedEventArgs^ args)
{
	// Display error message
	auto errorDialog = ref new MessageDialog(args->ErrorMessage);
	errorDialog->ShowAsync();
}
