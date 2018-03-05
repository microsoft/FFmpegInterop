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

namespace UnitTest.Windows
{
    [TestClass]
    public class CreateFromUriAsync
    {
        [TestMethod]
        public async Task CreateFromUri_Null()
        {
            // CreateFromUriAsync should throw if uri is blank with default parameter
            try
            {
                FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromUriAsync(string.Empty);
                Assert.Fail("Expected exception");
            }
            catch (Exception)
            {
            }
        }

        [TestMethod]
        public async Task CreateFromUri_Bad_Input()
        {
            // CreateFromUriAsync should throw when given bad uri
            try
            {
                FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromUriAsync("http://This.is.a.bad.uri");
                Assert.Fail("Expected exception");
            }
            catch (Exception)
            {
            }
        }

        [TestMethod]
        public async Task CreateFromUri_Default()
        {
            // CreateFromUriAsync should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromUriAsync(Constants.StreamingUriSource);
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioStreams[0].CodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoStream.CodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromUri_Passthrough_Audio()
        {
            // CreateFromUriAsync should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromUriAsync(Constants.StreamingUriSource, new FFmpegInteropConfig { PassthroughAudioAAC = true });
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioStreams[0].CodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoStream.CodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromUri_Force_Video()
        {
            // CreateFromUriAsync should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromUriAsync(Constants.StreamingUriSource, new FFmpegInteropConfig { PassthroughVideoH264 = false });
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioStreams[0].CodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoStream.CodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromUri_Options()
        {
            // Setup options PropertySet to configure FFmpeg
            PropertySet options = new PropertySet();
            options.Add("rtsp_flags", "prefer_tcp");
            options.Add("stimeout", 100000);
            Assert.IsNotNull(options);

            // CreateFromUriAsync should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = await FFmpegInteropMSS.CreateFromUriAsync(Constants.StreamingUriSource, new FFmpegInteropConfig { FFmpegOptions = options });
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioStreams[0].CodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoStream.CodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);
        }

    }
}
