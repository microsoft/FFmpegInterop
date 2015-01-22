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

using namespace FFMPEGMediaStreamSource;

using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage::Pickers;
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

	filePicker->PickSingleFileAndContinue();
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
				IRandomAccessStream^ readStream = stream.get();
				FFMPEGLib = ref new FFMPEG(readStream, false, false);
				MediaStreamSource^ mss = FFMPEGLib->GetMSS();

				if (mss)
				{
					media->SetMediaStreamSource(mss);
				}
			}
			catch (COMException^ ex)
			{
			}
		});
	}
	else
	{
		// Display error message
	}
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
	std::wstring wStringPath(args->ErrorMessage->Begin());
	OutputDebugString(wStringPath.c_str());
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
