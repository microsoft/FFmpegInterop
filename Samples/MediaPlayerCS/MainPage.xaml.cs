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
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Media.Core;
using Windows.Storage;
using Windows.Storage.Pickers;
using Windows.Storage.Streams;
using Windows.UI.Popups;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Input;

namespace MediaPlayerCS
{
	public sealed partial class MainPage : Page
	{
		public MainPage()
		{
			InitializeComponent();

			// Show the control panel on startup so user can start opening media
			splitter.IsPaneOpen = true;
		}

		private async void OpenFilePicker(object _1, RoutedEventArgs _2)
		{
			// Open the file picker
			var filePicker = new FileOpenPicker
			{
				ViewMode = PickerViewMode.Thumbnail,
				SuggestedStartLocation = PickerLocationId.VideosLibrary
			};
			filePicker.FileTypeFilter.Add("*");

			StorageFile file = await filePicker.PickSingleFileAsync();

			// Check if the user selected a file
			if (file != null)
			{
				OpenFile(file);
			}
		}
		public void OnFileActivated(StorageFile file)
		{
			OpenFile(file);
		}

		private async void OpenFile(StorageFile file)
		{
			// Populate the URI box with the path of the file selected
			uriBox.Text = file.Path;

			IRandomAccessStream stream = await file.OpenReadAsync();

			OpenMedia((mss, config) =>
			{
				FFmpegInteropMSS.CreateFromStream(stream, mss, config);
			});
		}

		private void OnUriBoxKeyUp(object sender, KeyRoutedEventArgs args)
		{
			String uri = (sender as TextBox).Text;

			// Only respond when the text box is not empty and after Enter key is pressed
			if (args.Key == Windows.System.VirtualKey.Enter && !String.IsNullOrWhiteSpace(uri))
			{
				// Mark event as handled to prevent duplicate event to re-triggered
				args.Handled = true;

				OpenUri(uri);
			}
		}

		private void OpenUri(String uri)
		{
			// TODO: Validate this code path with full FFmpeg build
			OpenMedia((mss, config) =>
			{
				FFmpegInteropMSS.CreateFromUri(uri, mss, config);
			});
		}

		private void OpenMedia(Action<MediaStreamSource, FFmpegInteropMSSConfig> createFunc)
		{
			// Stop any media currently playing
			mediaElement.Stop();

			try
			{
				// Create the media source
				IActivationFactory mssFactory = WindowsRuntimeMarshal.GetActivationFactory(typeof(MediaStreamSource));
				MediaStreamSource mss = mssFactory.ActivateInstance() as MediaStreamSource;

				var config = new FFmpegInteropMSSConfig
				{
					ForceAudioDecode = toggleSwitchAudioDecode.IsOn,
					ForceVideoDecode = toggleSwitchVideoDecode.IsOn
				};

				createFunc(mss, config);

				// Set the MSS as the media element's source
				mediaElement.SetMediaStreamSource(mss);

				// Close the control panel
				splitter.IsPaneOpen = false;
			}
			catch (Exception)
			{
				OnError("Failed to open media");
			}
		}

		private void OnMediaFailed(object _1, ExceptionRoutedEventArgs args)
		{
			OnError(args.ErrorMessage);
		}

		private async void OnError(string errMsg)
		{
			// Display error message
			var dialog = new MessageDialog(errMsg);
			await dialog.ShowAsync();

			// Open the control panel
			splitter.IsPaneOpen = true;
		}
	}
}
