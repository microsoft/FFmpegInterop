#pragma once

using namespace Windows::Foundation::Collections;

namespace FFmpegInterop
{
	public ref class FFmpegInteropConfig sealed
	{
	public:
		FFmpegInteropConfig()
		{
			VideoOutputAllowIyuv = false;
			VideoOutputAllowBgra8 = false;

			PassthroughAudioMP3 = true;
			PassthroughAudioAAC = true;
			PassthroughVideoH264 = true;
			PassthroughVideoH264Hi10P = false; // neither Windows codecs nor known HW decoders support Hi10P
			PassthroughVideoHEVC = true;

			SkipErrors = 50;
			MaxAudioThreads = 2;

			FFmpegOptions = ref new PropertySet();
		};

		property bool PassthroughAudioMP3;
		property bool PassthroughAudioAAC;

		property bool PassthroughVideoH264;
		property bool PassthroughVideoH264Hi10P;
		property bool PassthroughVideoHEVC;

		property bool VideoOutputAllowIyuv;
		property bool VideoOutputAllowBgra8;
		property bool VideoOutputAllowNv12;

		property unsigned int SkipErrors;

		property unsigned int MaxVideoThreads;
		property unsigned int MaxAudioThreads;

		property PropertySet^ FFmpegOptions;

	internal:
		property bool IsFrameGrabber;
	};
}