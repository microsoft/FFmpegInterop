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
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Media.Core;
using Windows.Storage;
using Windows.Storage.Pickers;
using Windows.Storage.Streams;
using Windows.UI.Popups;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

namespace MediaPlayerCS
{
    public sealed partial class MainPage : Page
    {
        private FFmpegInteropMSS FFmpegMSS;

        public MainPage()
        {
            this.InitializeComponent();

            // Show the control panel on startup so user can start opening media
            Splitter.IsPaneOpen = true;
        }

        private async void OpenLocalFile(object sender, RoutedEventArgs e)
        {
            FileOpenPicker filePicker = new FileOpenPicker();
            filePicker.ViewMode = PickerViewMode.Thumbnail;
            filePicker.SuggestedStartLocation = PickerLocationId.VideosLibrary;
            filePicker.FileTypeFilter.Add("*");

            // Show file picker so user can select a file
            StorageFile file = await filePicker.PickSingleFileAsync();

            if (file != null)
            {
                mediaElement.Stop();

                // Open StorageFile as IRandomAccessStream to be passed to FFmpegInteropMSS
                IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);

                try
                {
                    // Read toggle switches states and use them to setup FFmpeg MSS
                    bool forceDecodeAudio = toggleSwitchAudioDecode.IsOn;
                    bool forceDecodeVideo = toggleSwitchVideoDecode.IsOn;

					// Instantiate FFmpegInteropMSS using the opened local file stream
                    FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(readStream, forceDecodeAudio, forceDecodeVideo);
                    MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();

                    if (mss != null)
                    {
                        // Pass MediaStreamSource to Media Element
                        mediaElement.SetMediaStreamSource(mss);

                        // Close control panel after file open
                        Splitter.IsPaneOpen = false;
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

        private void URIBoxKeyUp(object sender, KeyRoutedEventArgs e)
        {
            var textBox = sender as TextBox;
            String uri = textBox.Text;

            // Only respond when the text box is not empty and after Enter key is pressed
            if (e.Key == Windows.System.VirtualKey.Enter && !String.IsNullOrWhiteSpace(uri))
            {
                // Mark event as handled to prevent duplicate event to re-triggered
                e.Handled = true;

                try
                {
                    // Read toggle switches states and use them to setup FFmpeg MSS
                    bool forceDecodeAudio = toggleSwitchAudioDecode.IsOn;
                    bool forceDecodeVideo = toggleSwitchVideoDecode.IsOn;

                    // Set FFmpeg specific options. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
                    PropertySet options = new PropertySet();

                    // Below are some sample options that you can set to configure RTSP streaming
                    // options.Add("rtsp_flags", "prefer_tcp");
                    // options.Add("stimeout", 100000);

                    // Instantiate FFmpegInteropMSS using the URI
                    mediaElement.Stop();
                    FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(uri, forceDecodeAudio, forceDecodeVideo, options);
                    if (FFmpegMSS != null)
                    {
                        MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();

                        if (mss != null)
                        {
                            // Pass MediaStreamSource to Media Element
                            mediaElement.SetMediaStreamSource(mss);

                            // Close control panel after opening media
                            Splitter.IsPaneOpen = false;
                        }
                        else
                        {
                            DisplayErrorMessage("Cannot open media");
                        }
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

        private void MediaFailed(object sender, ExceptionRoutedEventArgs e)
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
