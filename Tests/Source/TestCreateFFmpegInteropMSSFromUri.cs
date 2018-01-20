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
using Windows.Foundation.Collections;
using Windows.Media.Core;

namespace UnitTest.Windows
{
    [TestClass]
    public class CreateFFmpegInteropMSSFromUri
    {
        [TestMethod]
        public void CreateFromUri_Null()
        {
            // CreateFFmpegInteropMSSFromUri should return null if uri is blank with default parameter
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(string.Empty, false, false);
            Assert.IsNull(FFmpegMSS);

            // CreateFFmpegInteropMSSFromUri should return null if uri is blank with non default parameter
            FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(string.Empty, true, true);
            Assert.IsNull(FFmpegMSS);
        }

        [TestMethod]
        public void CreateFromUri_Bad_Input()
        {
            // CreateFFmpegInteropMSSFromUri should return null when given bad uri
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri("http://This.is.a.bad.uri", false, false);
            Assert.IsNull(FFmpegMSS);
        }

        [TestMethod]
        public void CreateFromUri_Default()
        {
            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(Constants.StreamingUriSource, false, false);
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioCodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoCodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public void CreateFromUri_Force_Audio()
        {
            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(Constants.StreamingUriSource, true, false);
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioCodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoCodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public void CreateFromUri_Force_Video()
        {
            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(Constants.StreamingUriSource, false, true);
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioCodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoCodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public void CreateFromUri_Force_Audio_Video()
        {
            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(Constants.StreamingUriSource, true, true);
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioCodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoCodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public void CreateFromUri_Options()
        {
            // Setup options PropertySet to configure FFmpeg
            PropertySet options = new PropertySet();
            options.Add("rtsp_flags", "prefer_tcp");
            options.Add("stimeout", 100000);
            Assert.IsNotNull(options);

            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(Constants.StreamingUriSource, false, false, options);
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioCodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoCodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public void CreateFromUri_Destructor()
        {
            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(Constants.StreamingUriSource, false, false);
            Assert.IsNotNull(FFmpegMSS);

            // Validate the metadata
            Assert.AreEqual(FFmpegMSS.AudioCodecName.ToLowerInvariant(), "aac");
            Assert.AreEqual(FFmpegMSS.VideoCodecName.ToLowerInvariant(), "h264");

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(Constants.StreamingUriLength, mss.Duration.TotalMilliseconds);

            // Keep original reference and ensure object are not destroyed until each reference is released by setting it to nullptr
            FFmpegInteropMSS OriginalFFmpegMSS = FFmpegMSS;
            MediaStreamSource Originalmss = mss;

            FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri("", false, false);
            Assert.IsNull(FFmpegMSS);
            Assert.IsNotNull(OriginalFFmpegMSS);
            Assert.IsNotNull(Originalmss);

            mss = null;
            Assert.IsNull(mss);
            Assert.IsNotNull(Originalmss);

            OriginalFFmpegMSS = null;
            Originalmss = null;
            Assert.IsNull(OriginalFFmpegMSS);
            Assert.IsNull(Originalmss);

        }
    }
}
