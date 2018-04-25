#pragma once

using namespace Windows::Foundation::Collections;
using namespace Windows::Media::Core;

namespace FFmpegInterop
{
	public ref class FFmpegInteropConfig sealed
	{
	public:
		FFmpegInteropConfig()
		{
			PassthroughAudioMP3 = false;
			PassthroughAudioAAC = false;

			PassthroughVideoH264 = true;
			PassthroughVideoH264Hi10P = false; // neither Windows codecs nor known HW decoders support Hi10P
			PassthroughVideoHEVC = true;

			VideoOutputAllowIyuv = false;
			VideoOutputAllow10bit = false;
			VideoOutputAllowBgra8 = false;
			VideoOutputAllowNv12 = true;

			SkipErrors = 50;
			MaxAudioThreads = 2;

			MaxSupportedPlaybackRate = 4.0;
			StreamBufferSize = 16384;

			FFmpegOptions = ref new PropertySet();

			AutoSelectForcedSubtitles = true;

			SubtitleRegion = ref new TimedTextRegion();

			TimedTextSize extent;
			extent.Unit = TimedTextUnit::Percentage;
			extent.Width = 100;
			extent.Height = 88;
			SubtitleRegion->Extent = extent;
			TimedTextPoint position;
			position.Unit = TimedTextUnit::Pixels;
			position.X = 0;
			position.Y = 0;
			SubtitleRegion->Position = position;
			SubtitleRegion->DisplayAlignment = TimedTextDisplayAlignment::After;
			SubtitleRegion->Background = Windows::UI::Colors::Transparent;
			SubtitleRegion->ScrollMode = TimedTextScrollMode::Rollup;
			SubtitleRegion->TextWrapping = TimedTextWrapping::Wrap;
			SubtitleRegion->WritingMode = TimedTextWritingMode::LeftRightTopBottom;
			SubtitleRegion->IsOverflowClipped = false;
			SubtitleRegion->ZIndex = 0;
			TimedTextDouble LineHeight;
			LineHeight.Unit = TimedTextUnit::Percentage;
			LineHeight.Value = 100;
			SubtitleRegion->LineHeight = LineHeight;
			TimedTextPadding padding;
			padding.Unit = TimedTextUnit::Percentage;
			padding.Start = 0;
			SubtitleRegion->Padding = padding;
			SubtitleRegion->Name = "";

			SubtitleStyle = ref new TimedTextStyle();

			SubtitleStyle->FontFamily = "default";
			TimedTextDouble fontSize;
			fontSize.Unit = TimedTextUnit::Percentage;
			fontSize.Value = 100;
			SubtitleStyle->FontSize = fontSize;
			SubtitleStyle->LineAlignment = TimedTextLineAlignment::Center;
			SubtitleStyle->FontStyle = TimedTextFontStyle::Normal;
			SubtitleStyle->FontWeight = TimedTextWeight::Normal;
			SubtitleStyle->Foreground = Windows::UI::Colors::White;
			SubtitleStyle->Background = Windows::UI::Colors::Transparent;
			//OutlineRadius = new TimedTextDouble { Unit = TimedTextUnit.Percentage, Value = 10 },
			TimedTextDouble outlineThickness;
			outlineThickness.Unit = TimedTextUnit::Percentage;
			outlineThickness.Value = 3;
			SubtitleStyle->OutlineThickness = outlineThickness;
			SubtitleStyle->FlowDirection = TimedTextFlowDirection::LeftToRight;
			SubtitleStyle->OutlineColor = Windows::UI::Colors::Black;
		};

		property bool PassthroughAudioMP3;
		property bool PassthroughAudioAAC;

		property bool PassthroughVideoH264;
		property bool PassthroughVideoH264Hi10P;
		property bool PassthroughVideoHEVC;

		property bool VideoOutputAllowIyuv;
		property bool VideoOutputAllow10bit;
		property bool VideoOutputAllowBgra8;
		property bool VideoOutputAllowNv12;

		property unsigned int SkipErrors;

		property unsigned int MaxVideoThreads;
		property unsigned int MaxAudioThreads;

		property double MaxSupportedPlaybackRate;
		property unsigned int StreamBufferSize;

		property PropertySet^ FFmpegOptions;

		property TimedTextRegion^ SubtitleRegion;
		property TimedTextStyle^ SubtitleStyle;
		property bool AutoSelectForcedSubtitles;

	internal:
		property bool IsFrameGrabber;
	};
}