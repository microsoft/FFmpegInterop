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
using Windows.Media.Core;

namespace UnitTest.Windows
{
    [TestClass]
    public class CreateFFmpegInteropMSSFromUri
    {
        [TestMethod]
        public void CreateFromUri_Null()
        {
            // CreateFFmpegInteropMSSFromUri should throw if uri is blank with default parameter
            try
            {
                FFmpegInteropMSS.CreateFromUri(string.Empty, null, null);
                Assert.IsTrue(false);
            }
            catch (Exception)
            {
                // Call threw as expected
            }

            // CreateFFmpegInteropMSSFromUri should throw if uri is blank with non default parameter
            try
            {
                var config = new FFmpegInteropMSSConfig
                {
                    ForceAudioDecode = true,
                    ForceVideoDecode = true
                };

                FFmpegInteropMSS.CreateFromUri(string.Empty, null, config);
                Assert.IsTrue(false);
            }
            catch (Exception)
            {
                // Call threw as expected
            }
        }

        [TestMethod]
        public void CreateFromUri_Bad_Input()
        {
            // CreateFFmpegInteropMSSFromUri should throw when given bad uri
            try
            {
                FFmpegInteropMSS.CreateFromUri("http://This.is.a.bad.uri", null, null);
                Assert.IsTrue(false);
            }
            catch (Exception)
            {
                // Call threw as expected
            }
        }

        [TestMethod]
        public void CreateFromUri_Default()
        {
            // Create the MSS
            MediaStreamSource mss = FFmpegInteropMSS.CreateFromUri(Constants.StreamingUriSource, null, null);

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);

            // TODO: Verify expected stream types (aac, h264)
        }

        [TestMethod]
        public void CreateFromUri_Force_Audio()
        {
            // Create the MSS with forced audio decode
            var config = new FFmpegInteropMSSConfig
            {
                ForceAudioDecode = true
            };

            MediaStreamSource mss = FFmpegInteropMSS.CreateFromUri(Constants.StreamingUriSource, null, config);

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);

            // TODO: Verify expected stream types (pcm, h264)
        }

        [TestMethod]
        public void CreateFromUri_Force_Video()
        {
            // Create the MSS with forced video decode
            var config = new FFmpegInteropMSSConfig
            {
                ForceVideoDecode = true
            };

            MediaStreamSource mss = FFmpegInteropMSS.CreateFromUri(Constants.StreamingUriSource, null, config);

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);

            // TODO: Verify expected stream types (aac, NV12)
        }

        [TestMethod]
        public void CreateFromUri_Force_Audio_Video()
        {
            // Create the MSS with forced audio and video decode
            var config = new FFmpegInteropMSSConfig
            {
                ForceAudioDecode = true,
                ForceVideoDecode = true
            };

            MediaStreamSource mss = FFmpegInteropMSS.CreateFromUri(Constants.StreamingUriSource, null, config);

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);

            // TODO: Verify expected stream types (pcm, NV12)
        }

        [TestMethod]
        public void CreateFromUri_Options()
        {
            // Create the MSS with the set FFmpeg options
            var config = new FFmpegInteropMSSConfig();
            var options = config.FFmpegOptions;
            options.Add("rtsp_flags", "prefer_tcp");
            options.Add("stimeout", "100000");

            MediaStreamSource mss = FFmpegInteropMSS.CreateFromUri(Constants.StreamingUriSource, null, config);

            // Based on the provided media, check if the following properties are set correctly
            Assert.IsTrue(mss.CanSeek);
            Assert.IsTrue(mss.BufferTime.TotalMilliseconds > 0);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);

            // TODO: Verify expected stream types (aac, h264)
        }
    }
}
