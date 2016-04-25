using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation.Collections;
using Windows.Media.Core;
using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using FFmpegInterop;

namespace UnitTest.Windows
{
    [TestClass]
    public class CreateFFmpegInteropMSSFromUri
    {
        static string _UriSource = "rtsp://184.72.239.149/vod/mp4:BigBuckBunny_175k.mov";
        static int _UriLength = 596458;

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
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(_UriSource, false, false);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(_UriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public void CreateFromUri_Force_Audio()
        {
            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(_UriSource, true, false);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(_UriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public void CreateFromUri_Force_Video()
        {
            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(_UriSource, false, true);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(_UriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public void CreateFromUri_Force_Audio_Video()
        {
            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(_UriSource, true, true);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(_UriLength, mss.Duration.TotalMilliseconds);
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
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(_UriSource, false, false, options);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(_UriLength, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public void CreateFromUri_Destructor()
        {
            // CreateFFmpegInteropMSSFromUri should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromUri(_UriSource, false, false);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(_UriLength, mss.Duration.TotalMilliseconds);

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
