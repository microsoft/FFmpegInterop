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
using System.Threading.Tasks;
using Windows.Foundation.Collections;
using Windows.Media.Core;
using Windows.Storage;
using Windows.Storage.Streams;

namespace UnitTest.Windows
{
    [TestClass]
    public class CreateFromStreamAsync
    {
        [TestMethod]
        public async Task CreateFromStream_Null()
        {
            // CreateFromStreamAsync should throw if stream is null with default parameter
            try
            {
                await FFmpegInteropMSS.CreateFromStreamAsync(null);
                Assert.Fail("Expected exception");
            }
            catch (Exception)
            {
            }
        }

        [TestMethod]
        public async Task CreateFromStream_Bad_Input()
        {
            Uri uri = new Uri("ms-appx:///test.txt");
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.GetFileFromApplicationUriAsync(uri);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // CreateFromStreamAsync should throw since test.txt is not a valid media file
            try
            {
                await FFmpegInteropMSS.CreateFromStreamAsync(readStream);
                Assert.Fail("Expected exception");
            }
            catch (Exception)
            {
            }
        }


        [TestMethod]
        public async Task CreateFromStream_Default()
        {
            Uri uri = new Uri(Constants.DownloadUriSource);
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync(Constants.DownloadStreamedFileName, uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // CreateFromStreamAsync should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromStreamAsync(readStream);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.DownloadUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromStream_Passthrough_Audio()
        {
            Uri uri = new Uri(Constants.DownloadUriSource);
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync(Constants.DownloadStreamedFileName, uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // CreateFromStreamAsync should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromStreamAsync(readStream, new FFmpegInteropConfig { PassthroughAudioAAC = true });
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.DownloadUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromStream_Force_Video()
        {
            Uri uri = new Uri(Constants.DownloadUriSource);
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync(Constants.DownloadStreamedFileName, uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // CreateFromStreamAsync should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromStreamAsync(readStream, new FFmpegInteropConfig { PassthroughVideoH264 = false });
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.DownloadUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromStream_Options()
        {
            Uri uri = new Uri(Constants.DownloadUriSource);
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync(Constants.DownloadStreamedFileName, uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // Setup options PropertySet to configure FFmpeg
            PropertySet options = new PropertySet();
            options.Add("rtsp_flags", "prefer_tcp");
            options.Add("stimeout", 100000);
            Assert.IsNotNull(options);

            // CreateFromStreamAsync should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromStreamAsync(readStream, new FFmpegInteropConfig { FFmpegOptions = options });
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioStreams[0].CodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoStream.CodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.DownloadUriLength, mss.Duration.TotalMilliseconds);
        }
    }
}
