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
using FFmpegInterop;

using System;
using System.Linq;
using Windows.Media.Core;
using Windows.Storage;
using Windows.Storage.Pickers;
using Windows.Storage.Streams;
using Windows.UI.Popups;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Navigation;

namespace MediaPlayerCS
{
    public sealed partial class MainPage : Page
    {
        private FFmpegInteropMSS FFmpegMSS;
		private bool forceDecodeAudio = false;
		private bool forceDecodeVideo = false;

        public MainPage()
        {
            this.InitializeComponent();

            this.NavigationCacheMode = NavigationCacheMode.Required;
        }

        // Handle the returned files from file picker
        public async void ContinueFileOpenPicker(Windows.ApplicationModel.Activation.FileOpenPickerContinuationEventArgs args)
        {
            if (args.Files.Count > 0)
            {
                mediaElement.Stop();

                try
                {
                    // Open StorageFile as IRandomAccessStream to be passed to FFmpegInteropMSS
                    StorageFile file = args.Files.FirstOrDefault();
                    IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);

                    // Instantiate FFmpeg object and pass the stream from opened file
                    FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(readStream, forceDecodeAudio, forceDecodeVideo);
                    MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();

                    if (mss != null)
                    {
                        // Pass MediaStreamSource to Media Element
                        mediaElement.SetMediaStreamSource(mss);
                    }
                    else
                    {
                        DisplayErrorMessage("Cannot open media");
                    }
                }
                catch (Exception ex)
                {
                    DisplayErrorMessage(ex.Message);
                }
            }
        }

        private void AppBarButton_Browse_Click(object sender, RoutedEventArgs e)
        {
            FileOpenPicker filePicker = new FileOpenPicker();
            filePicker.ViewMode = PickerViewMode.Thumbnail;
            filePicker.SuggestedStartLocation = PickerLocationId.VideosLibrary;
            filePicker.FileTypeFilter.Add("*");

            // Launch file picker that will be handled by ContinueFileOpenPicker after application is re-activated
            filePicker.PickSingleFileAndContinue();
        }

        private void AppBarButton_Audio_Click(object sender, RoutedEventArgs e)
        {
            var button = sender as AppBarToggleButton;
	        forceDecodeAudio = button.IsChecked.Value;

            if (button.IsChecked.Value)
            {
                button.Label = "Enable audio auto decoding";
            }
            else
            {
                button.Label = "Disable audio auto decoding";
            }
        }

        private void AppBarButton_Video_Click(object sender, RoutedEventArgs e)
        {
            var button = sender as AppBarToggleButton;
            forceDecodeVideo = button.IsChecked.Value;

            if (button.IsChecked.Value)
            {
                button.Label = "Enable video auto decoding";
            }
            else
            {
                button.Label = "Disable video auto decoding";
            }
        }

        void CommandBar_Opened(object sender, object e)
        {
	        this.BottomAppBar.Opacity = 1.0;
        }

        void CommandBar_Closed(object sender, object e)
        {
	        this.BottomAppBar.Opacity = 0.0;
        }

        private void MediaElement_MediaEnded(object sender, RoutedEventArgs e)
        {
        }

        private void MediaElement_MediaFailed(object sender, ExceptionRoutedEventArgs e)
        {
            DisplayErrorMessage(e.ErrorMessage);
        }

        private async void DisplayErrorMessage(string message)
        {
            // Display error message
            var errorDialog = new MessageDialog(message);
            var x = await errorDialog.ShowAsync();
        }
    }
}
