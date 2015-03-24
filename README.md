# FFmpegInterop library for Windows

####This project is licensed from Microsoft under the [Apache 2.0 License](http://www.apache.org/licenses/LICENSE-2.0)

##Welcome to FFmpegInterop library for Windows.

FFmpegInterop is an open-source project that aims to provide a easy way to use FFmpeg in a Windows modern application for playback of different media content. FFmpegInterop implements a [MediaStreamSource](https://msdn.microsoft.com/en-us/library/windows/apps/windows.media.core.mediastreamsource.aspx) which leverages FFmpeg to process the media and leverages the Windows media pipeline for the playback.

Some of the advantages of this approach is that synchronizing audio and video is handled by the Windows media pipeline. You can leverage the Windows built-in audio and video decoders if you want, allowing for better power consumption on a mobile device.

##Prerequisites
Getting a compatible build of FFmpeg is required for this to work.

You can get the code for [FFmpeg on Github](http://github.com/FFmpeg) by cloning [git://source.ffmpeg.org/ffmpeg.git](git://source.ffmpeg.org/ffmpeg.git).

You can follow the instruction on how to [build FFmpeg for WinRT](https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT) (Windows Phone and Windows Store) apps. This project is configured to use have FFmpeg in a folder next to the FFmpegInterop project.

	git clone git://source.ffmpeg.org/ffmpeg.git
	git clone git://github.com/microsoft/FFmpegInterop.git

If you have followed the Wiki instructions and have the appropriate builds of FFmpeg in the *FFmpeg/Build/<platform\>/<architecture\>* folders, FFmpegInterop should build cleanly giving you the interop object as well as 3 sample MediaPlayers (C++, C# and JS) that show how to connect the MediaStreamSource to a MediaElement or Video tag for playback.

###Using the FFmpegInterop object

Using the **FFmpegInterop** object is fairly straightforward and can be observed from the sample applications provided.

1. Get a stream for the media you want to playback.
2. Create a new FFmpegInteropObject using FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream() passing it the stream and whether you want to force the decoding of the media (if you don't force decoding of the media, the MediaStreamSource will try to pass the compressed data for playback, this is currently enabled for mp3, aac and h.264 media).
3. Get the MediaStreamSource from the Interop object by invoking GetMediaStreamSource()
4. Assign the MediaStreamSource to your MediaElement or VideoTag for playback.

	#####You can try to use the method FFmepgInteropMSS.CreateFFmpegInteropMSSFromUri to create a MediaStreamSource on a streaming source (shoutcast for example).

This project is in an early stage and we look forward to engaging with the community and figure out where we can take this project.

###The Windows OSS Team.
