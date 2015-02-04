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

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

MainPage::MainPage()
{
	InitializeComponent();
}

void MainPage::OnStarting(Windows::Media::Core::MediaStreamSource ^sender, Windows::Media::Core::MediaStreamSourceStartingEventArgs ^args)
{
}

void FFMPEGMediaStreamSource::MainPage::AppBarButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	FileOpenPicker^ filePicker = ref new FileOpenPicker();
	filePicker->ViewMode = PickerViewMode::Thumbnail;
	filePicker->SuggestedStartLocation = PickerLocationId::VideosLibrary;
	filePicker->FileTypeFilter->Append("*");

	create_task(filePicker->PickSingleFileAsync()).then([this](StorageFile^ file)
	{
		if (file != nullptr)
		{
			media->Stop();

			create_task(file->OpenAsync(FileAccessMode::Read)).then([this, file](task<IRandomAccessStream^> stream)
			{
				try
				{
					IRandomAccessStream^ readStream = stream.get();
					FFMPEGLib = ref new FFmpegLibrary(readStream, false, false);
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
	});
}


void FFMPEGMediaStreamSource::MainPage::media_BufferingProgressChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void FFMPEGMediaStreamSource::MainPage::media_CurrentStateChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void FFMPEGMediaStreamSource::MainPage::media_DataContextChanged(Windows::UI::Xaml::FrameworkElement^ sender, Windows::UI::Xaml::DataContextChangedEventArgs^ args)
{

}


void FFMPEGMediaStreamSource::MainPage::media_DoubleTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs^ e)
{

}


void FFMPEGMediaStreamSource::MainPage::media_DownloadProgressChanged(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void FFMPEGMediaStreamSource::MainPage::media_LayoutUpdated(Platform::Object^ sender, Platform::Object^ e)
{

}


void FFMPEGMediaStreamSource::MainPage::media_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

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


void FFMPEGMediaStreamSource::MainPage::media_MediaOpened(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void FFMPEGMediaStreamSource::MainPage::media_RateChanged(Platform::Object^ sender, Windows::UI::Xaml::Media::RateChangedRoutedEventArgs^ e)
{

}


void FFMPEGMediaStreamSource::MainPage::media_SeekCompleted(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{

}


void FFMPEGMediaStreamSource::MainPage::media_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e)
{

}


void FFMPEGMediaStreamSource::MainPage::media_Tapped(Platform::Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e)
{

}
