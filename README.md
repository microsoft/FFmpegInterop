# FFmpegInterop library for Windows

#### This project is licensed from Microsoft under the [Apache 2.0 License](http://www.apache.org/licenses/LICENSE-2.0)

## Welcome to FFmpegInterop library for Windows.

FFmpegInterop is an open-source project that aims to provide an easy way to use FFmpeg in Windows 10, Windows 8.1, and Windows Phone 8.1 applications for playback of a variety of media contents. FFmpegInterop implements a [MediaStreamSource](https://msdn.microsoft.com/en-us/library/windows/apps/windows.media.core.mediastreamsource.aspx) which leverages FFmpeg to process media and uses the Windows media pipeline for playback.

One of the advantages of this approach is that audio and video synchronization is handled by the Windows media pipeline. You can also use the Windows built-in audio and video decoders which allows for better power consumption mobile devices.

## Prerequisites
Getting a compatible build of FFmpeg is required for this to work.

You can simply use the embedded git submodule that points to the latest tested release of FFmpeg.

	git clone --recursive git://github.com/microsoft/FFmpegInterop.git

Alternatively, you can get the code for [FFmpeg on Github](http://github.com/FFmpeg) yourself by cloning [git://source.ffmpeg.org/ffmpeg.git](git://source.ffmpeg.org/ffmpeg.git) and replace existing default `ffmpeg` folder with it.

	git clone git://github.com/microsoft/FFmpegInterop.git
	cd FFmpegInterop
	git clone git://source.ffmpeg.org/ffmpeg.git

Your `FFmpegInterop` folder should look as follows

	FFmpegInterop\
	    ffmpeg\              - ffmpeg source code from the latest release in git://github.com/FFmpeg/FFmpeg.git
	    FFmpegInterop\       - FFmpegInterop WinRT component
	    Samples\             - Sample Media Player applications in C++, C#, and JavaScript
	    Tests\               - Unit tests for FFmpegInterop
	    BuildFFmpeg.bat      - Helper script to build FFmpeg libraries as described in https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT
	    FFmpegConfig.sh      - Internal script that contains FFmpeg configure options
	    FFmpegWin8.1.sln     - Microsoft Visual Studio 2013 solution file for Windows 8.1 and Windows Phone 8.1 apps development
	    FFmpegWin10.sln      - Microsoft Visual Studio 2015 solution file for Windows 10 apps development
	    LICENSE
	    README.md

Now that you have the FFmpeg source code, you can follow the instructions on how to [build FFmpeg for WinRT](https://trac.ffmpeg.org/wiki/CompilationGuide/WinRT) apps. Follow the setup instruction carefuly to avoid build issues. After completing the setup as instructed, you can invoke `BuildFFmpeg.bat` script to build or do it manually using the instructions in the compilation guide.

	BuildFFmpeg.bat win10                     - Build for Windows 10 ARM, x64, and x86
	BuildFFmpeg.bat phone8.1 ARM              - Build for Windows Phone 8.1 ARM only
	BuildFFmpeg.bat win8.1 x86 x64            - Build for Windows 8.1 x86 and x64 only
	BuildFFmpeg.bat phone8.1 win10 ARM        - Build for Windows 10 and Windows Phone 8.1 ARM only
	BuildFFmpeg.bat win8.1 phone8.1 win10     - Build all architecture for all target platform

If you use the build script or follow the Wiki instructions as is you should find the appropriate builds of FFmpeg libraries in the `ffmpeg/Build/<platform\>/<architecture\>` folders.

Simply open one of the Microsoft Visual Studio solution file (e.g. FFmpegWin10.sln), set one of the MediaPlayer as StartUp project, and run. FFmpegInterop should build cleanly giving you the interop object as well as the selected sample MediaPlayer (C++, C# or JS) that show how to connect the MediaStreamSource to a MediaElement or Video tag for playback.

### Using the FFmpegInterop object

Using the **FFmpegInterop** object is fairly straightforward and can be observed from the sample applications provided.

1. Get a stream for the media you want to playback.
2. Create a new FFmpegInteropObject using FFmpegInteropMSS.CreateFFmpegInteropMSSFromStream() passing it the stream and whether you want to force the decoding of the media (if you don't force decoding of the media, the MediaStreamSource will try to pass the compressed data for playback, this is currently enabled for mp3, aac and h.264 media).
3. Get the MediaStreamSource from the Interop object by invoking GetMediaStreamSource()
4. Assign the MediaStreamSource to your MediaElement or VideoTag for playback.

	##### You can try to use the method FFmepgInteropMSS.CreateFFmpegInteropMSSFromUri to create a MediaStreamSource on a streaming source (shoutcast for example).

This project is in an early stage and we look forward to engaging with the community and hearing your feedback to figure out where we can take this project.

### The Windows OSS Team.
