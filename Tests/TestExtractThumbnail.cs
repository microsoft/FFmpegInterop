//*****************************************************************************
//
//	Copyright 2017 Microsoft Corporation
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
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Graphics.Imaging;
using Windows.Media.Core;
using Windows.Storage;
using Windows.Storage.Streams;

namespace UnitTest.Windows
{
    [TestClass]
    public class TestExtractThumbnail
    {
        private MediaStreamSource CreateMSSFromStream(IRandomAccessStream stream, FFmpegInteropMSSConfig config)
        {
            // Create the MSS
            IActivationFactory mssFactory = WindowsRuntimeMarshal.GetActivationFactory(typeof(MediaStreamSource));
            MediaStreamSource mss = mssFactory.ActivateInstance() as MediaStreamSource;

            // Create the FFmpegInteropMSS from the provided stream
            FFmpegInteropMSS.CreateFromStream(stream, mss, config);

            return mss;
        }

        [TestMethod]
        public async Task GetThumbnailFromMedia()
        {
            // Create a stream from the resource URI that we have
            var uri = new Uri("ms-appx:///TestFiles//silence with album art.mp3");
            StorageFile file = await StorageFile.GetFileFromApplicationUriAsync(uri);
            IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.Read);

            // CreateFFmpegInteropMSSFromUri should return null if uri is blank with default parameter
            MediaStreamSource mss = CreateMSSFromStream(stream, null);

            // Verify that we have a valid bitmap
            using (IRandomAccessStream thumbnailStream = await mss.Thumbnail.OpenReadAsync())
            {
                BitmapDecoder decoder = await BitmapDecoder.CreateAsync(thumbnailStream);
                BitmapFrame bitmap = await decoder.GetFrameAsync(0);
            }
        }
    }
}
