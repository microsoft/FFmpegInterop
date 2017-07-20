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
using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using System;
using System.IO;
using System.Runtime.InteropServices.WindowsRuntime;
using System.Threading.Tasks;
using Windows.Graphics.Imaging;
using Windows.Storage;
using Windows.Storage.Streams;

namespace UnitTest.Windows
{
    [TestClass]
    public class TestExtractThumbnail
    {
        [TestMethod]
        public async Task GetThumbnailFromMedia()
        {
            // Create a stream from the resource URI that we have
            var uri = new Uri("ms-appx:///silence with album art.mp3");
            var file = await StorageFile.GetFileFromApplicationUriAsync(uri);
            var stream = await file.OpenAsync(FileAccessMode.Read);

            // CreateFFmpegInteropMSSFromUri should return null if uri is blank with default parameter
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(stream, false, false);
            Assert.IsNotNull(FFmpegMSS);

            var thumbnailData = FFmpegMSS.ExtractThumbnail();
            Assert.IsNotNull(thumbnailData);

            // Verify that we have a valid bitmap
            using (IRandomAccessStream thumbnailstream = thumbnailData.Buffer.AsStream().AsRandomAccessStream())
            {
                BitmapDecoder decoder = await BitmapDecoder.CreateAsync(thumbnailstream);

                var bitmap = await decoder.GetFrameAsync(0);

                Assert.IsNotNull(bitmap);
            }
        }
    }
}
