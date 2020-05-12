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
using Windows.Media.Core;
using Windows.Media.Playback;
using Windows.Storage;
using Windows.Storage.Streams;

namespace UnitTest.Windows
{
    [TestClass]
    public class CreateFFmpegInteropMSSFromStream
    {
        [TestMethod]
        public void CreateFromStream_Null()
        {
            // FFmpegInteropMSS.CreateFromStream should throw if stream is null with default parameter
            try
            {
                FFmpegInteropMSS ffmpegInteropMss = FFmpegInteropMSS.CreateFromStream(null, null, null);
                ffmpegInteropMss.Shutdown();
                Assert.IsTrue(false);
            }
            catch (Exception)
            {
                // Call threw as expected
            }

            // FFmpegInteropMSS.CreateFromStream should return null if stream is null with non default parameter
            try
            {
                var config = new FFmpegInteropMSSConfig
                {
                    ForceAudioDecode = true,
                    ForceVideoDecode = true
                };

                FFmpegInteropMSS ffmpegInteropMss = FFmpegInteropMSS.CreateFromStream(null, null, config);
                ffmpegInteropMss.Shutdown();
                Assert.IsTrue(false);
            }
            catch (Exception)
            {
                // Call threw as expected
            }
        }

        [TestMethod]
        public async Task CreateFromStream_Bad_Input()
        {
            Uri uri = new Uri("ms-appx:///TestFiles//test.txt");
            StorageFile file = await StorageFile.GetFileFromApplicationUriAsync(uri);
            IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.Read);

            // FFmpegInteropMSS.CreateFromStream should throw since test.txt is not a valid media file
            try
            {
                FFmpegInteropMSS ffmpegInteropMss = FFmpegInteropMSS.CreateFromStream(stream, null, null);
                ffmpegInteropMss.Shutdown();
                Assert.IsTrue(false);
            }
            catch (Exception)
            {
                // Call threw as expected
            }
        }

        [TestMethod]
        public async Task CreateFromStream_Default()
        {
            Uri uri = new Uri(Constants.DownloadUriSource);
            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync(Constants.DownloadStreamedFileName, uri, null);
            IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.Read);

            // Create the MSS
            FFmpegInteropMSS ffmpegInteropMss = FFmpegInteropMSS.CreateFromStream(stream, null, null);
            MediaStreamSource mss = ffmpegInteropMss.GetMediaStreamSource();

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.DownloadUriLength, mss.Duration.TotalMilliseconds);

            ffmpegInteropMss.Shutdown();
        }

        [TestMethod]
        public async Task CreateFromStream_Force_Audio()
        {
            Uri uri = new Uri(Constants.DownloadUriSource);
            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync(Constants.DownloadStreamedFileName, uri, null);
            IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.Read);

            // Create the MSS with forced audio decode
            var config = new FFmpegInteropMSSConfig
            {
                ForceAudioDecode = true
            };

            FFmpegInteropMSS ffmpegInteropMss = FFmpegInteropMSS.CreateFromStream(stream, null, config);
            MediaStreamSource mss = ffmpegInteropMss.GetMediaStreamSource();

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.DownloadUriLength, mss.Duration.TotalMilliseconds);

            ffmpegInteropMss.Shutdown();
        }

        [TestMethod]
        public async Task CreateFromStream_Force_Video()
        {
            Uri uri = new Uri(Constants.DownloadUriSource);
            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync(Constants.DownloadStreamedFileName, uri, null);
            IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.Read);

            // Create the MSS with forced video decode
            var config = new FFmpegInteropMSSConfig
            {
                ForceVideoDecode = true
            };

            FFmpegInteropMSS ffmpegInteropMss = FFmpegInteropMSS.CreateFromStream(stream, null, config);
            MediaStreamSource mss = ffmpegInteropMss.GetMediaStreamSource();

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.DownloadUriLength, mss.Duration.TotalMilliseconds);

            ffmpegInteropMss.Shutdown();
        }

        [TestMethod]
        public async Task CreateFromStream_Force_Audio_Video()
        {
            Uri uri = new Uri(Constants.DownloadUriSource);
            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync(Constants.DownloadStreamedFileName, uri, null);
            IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.Read);

            // Create the MSS with forced audio and video decode
            var config = new FFmpegInteropMSSConfig
            {
                ForceAudioDecode = true,
                ForceVideoDecode = true
            };

            FFmpegInteropMSS ffmpegInteropMss = FFmpegInteropMSS.CreateFromStream(stream, null, config);
            MediaStreamSource mss = ffmpegInteropMss.GetMediaStreamSource();

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.DownloadUriLength, mss.Duration.TotalMilliseconds);

            ffmpegInteropMss.Shutdown();
        }

        [TestMethod]
        public async Task CreateFromStream_Options()
        {
            Uri uri = new Uri(Constants.DownloadUriSource);
            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync(Constants.DownloadStreamedFileName, uri, null);
            IRandomAccessStream stream = await file.OpenAsync(FileAccessMode.Read);

            // Create the MSS with FFmpeg options set
            var config = new FFmpegInteropMSSConfig();
            var options = config.FFmpegOptions;
            options.Add("rtsp_flags", "prefer_tcp");
            options.Add("stimeout", "100000");

            FFmpegInteropMSS ffmpegInteropMss = FFmpegInteropMSS.CreateFromStream(stream, null, config);
            MediaStreamSource mss = ffmpegInteropMss.GetMediaStreamSource();

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.DownloadUriLength, mss.Duration.TotalMilliseconds);

            // TODO: Verify expected stream types (aac, h264)

            ffmpegInteropMss.Shutdown();
        }
    }
}
