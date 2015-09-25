using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Foundation.Collections;
using Windows.Media.Core;
using Windows.Storage;
using Windows.Storage.Streams;
using Microsoft.VisualStudio.TestPlatform.UnitTestFramework;
using FFmpegInterop;

namespace UnitTest.Windows
{
    [TestClass]
    public class CreateFFmpegInteropMSSFromStream
    {
        [TestMethod]
        public void CreateFromStream_Null()
        {
            // CreateFFmpegInteropMSSFromStream should return null if stream is null with default parameter
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(null, false, false);
            Assert.IsNull(FFmpegMSS);

            // CreateFFmpegInteropMSSFromStream should return null if stream is null with non default parameter
            FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(null, true, true);
            Assert.IsNull(FFmpegMSS);
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

            // CreateFFmpegInteropMSSFromStream should return null since test.txt is not a valid media file
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(readStream, false, false);
            Assert.IsNull(FFmpegMSS);
        }


        [TestMethod]
        public async Task CreateFromStream_Default()
        {
            Uri uri = new Uri("http://go.microsoft.com/fwlink/p/?LinkID=272585");
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync("d8c317bd-9fbb-4c5f-94ed-501f09841917.mp4", uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // CreateFFmpegInteropMSSFromStream should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(readStream, false, false);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(113750, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromStream_Force_Audio()
        {
            Uri uri = new Uri("http://go.microsoft.com/fwlink/p/?LinkID=272585");
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync("d8c317bd-9fbb-4c5f-94ed-501f09841917.mp4", uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // CreateFFmpegInteropMSSFromStream should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(readStream, true, false);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(113750, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromStream_Force_Video()
        {
            Uri uri = new Uri("http://go.microsoft.com/fwlink/p/?LinkID=272585");
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync("d8c317bd-9fbb-4c5f-94ed-501f09841917.mp4", uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // CreateFFmpegInteropMSSFromStream should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(readStream, false, true);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(113750, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromStream_Force_Audio_Video()
        {
            Uri uri = new Uri("http://go.microsoft.com/fwlink/p/?LinkID=272585");
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync("d8c317bd-9fbb-4c5f-94ed-501f09841917.mp4", uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // CreateFFmpegInteropMSSFromStream should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(readStream, true, true);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(113750, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromStream_Options()
        {
            Uri uri = new Uri("http://go.microsoft.com/fwlink/p/?LinkID=272585");
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync("d8c317bd-9fbb-4c5f-94ed-501f09841917.mp4", uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // Setup options PropertySet to configure FFmpeg
            PropertySet options = new PropertySet();
            options.Add("rtsp_flags", "prefer_tcp");
            options.Add("stimeout", 100000);
            Assert.IsNotNull(options);

            // CreateFFmpegInteropMSSFromStream should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(readStream, false, false, options);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(113750, mss.Duration.TotalMilliseconds);
        }

        [TestMethod]
        public async Task CreateFromStream_Destructor()
        {
            Uri uri = new Uri("http://go.microsoft.com/fwlink/p/?LinkID=272585");
            Assert.IsNotNull(uri);

            StorageFile file = await StorageFile.CreateStreamedFileFromUriAsync("d8c317bd-9fbb-4c5f-94ed-501f09841917.mp4", uri, null);
            Assert.IsNotNull(file);

            IRandomAccessStream readStream = await file.OpenAsync(FileAccessMode.Read);
            Assert.IsNotNull(readStream);

            // CreateFFmpegInteropMSSFromStream should return valid FFmpegInteropMSS object which generates valid MediaStreamSource object
            FFmpegInteropMSS FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(readStream, false, false);
            Assert.IsNotNull(FFmpegMSS);

            MediaStreamSource mss = FFmpegMSS.GetMediaStreamSource();
            Assert.IsNotNull(mss);

            // Based on the provided media, check if the following properties are set correctly
            Assert.AreEqual(true, mss.CanSeek);
            Assert.AreNotEqual(0, mss.BufferTime.TotalMilliseconds);
            Assert.AreEqual(113750, mss.Duration.TotalMilliseconds);

            // Keep original reference and ensure object are not destroyed until each reference is released by setting it to nullptr
            FFmpegInteropMSS OriginalFFmpegMSS = FFmpegMSS;
            MediaStreamSource Originalmss = mss;

            FFmpegMSS = FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream(null, false, false);
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
