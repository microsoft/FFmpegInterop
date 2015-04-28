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
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"

namespace MediaPlayerCPP
{
	public ref class MainPage sealed
	{
	public:
		MainPage();
		void ContinueFileOpenPicker(Windows::ApplicationModel::Activation::FileOpenPickerContinuationEventArgs^ args);

	private:
		void AppBarButton_Browse_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void AppBarButton_Audio_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void AppBarButton_Video_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void CommandBar_Opened(Platform::Object^ sender, Platform::Object^ e);
		void CommandBar_Closed(Platform::Object^ sender, Platform::Object^ e);

		void MediaElement_MediaEnded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void MediaElement_MediaFailed(Platform::Object^ sender, Windows::UI::Xaml::ExceptionRoutedEventArgs^ e);
		void DisplayErrorMessage(Platform::String^ message);

		FFmpegInterop::FFmpegInteropMSS^ FFmpegMSS;
		bool forceDecodeAudio;
		bool forceDecodeVideo;
	};
}
