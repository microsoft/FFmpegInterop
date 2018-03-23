#pragma once

#include "pch.h"
#include "TimedTextSample.h"

using namespace Platform;

namespace FFmpegInterop
{
	public interface class IStreamInfo
	{
		property String^ Name { String^ get(); }
		property String^ Language { String^ get(); }
		property String^ CodecName { String^ get(); }
		property int64 Bitrate { int64 get(); }
		property bool IsDefault { bool get(); }
	};

	public ref class AudioStreamInfo sealed : public IStreamInfo
	{
	public:
		AudioStreamInfo(String^ name, String^ language, String^ codecName, int64 bitrate, bool isDefault,
			int channels, int sampleRate, int bitsPerSample)
		{
			this->name = name;
			this->language = language;
			this->codecName = codecName;
			this->bitrate = bitrate;
			this->isDefault = isDefault;

			this->channels = channels;
			this->sampleRate = sampleRate;
			this->bitsPerSample = bitsPerSample;
		}

		virtual property String^ Name { String^ get() { return name; } }
		virtual property String^ Language { String^ get() { return language; } }
		virtual property String^ CodecName { String^ get() { return codecName; } }
		virtual property int64 Bitrate { int64 get() { return bitrate; } }
		virtual property bool IsDefault { bool get() { return isDefault; } }

		property int Channels { int get() { return channels; } }
		property int SampleRate { int get() { return sampleRate; } }
		property int BitsPerSample { int get() { return bitsPerSample; } }

	private:
		String ^ name;
		String^ language;
		String^ codecName;
		int64 bitrate;
		bool isDefault;

		int channels;
		int sampleRate;
		int bitsPerSample;
	};

	public ref class VideoStreamInfo sealed : public IStreamInfo
	{
	public:
		VideoStreamInfo(String^ name, String^ language, String^ codecName, int64 bitrate, bool isDefault,
			int pixelWidth, int pixelHeight, int bitsPerSample)
		{
			this->name = name;
			this->language = language;
			this->codecName = codecName;
			this->bitrate = bitrate;
			this->isDefault = isDefault;

			this->pixelWidth = pixelWidth;
			this->pixelHeight = pixelHeight;
			this->bitsPerSample = bitsPerSample;
		}

		virtual property String^ Name { String^ get() { return name; } }
		virtual property String^ Language { String^ get() { return language; } }
		virtual property String^ CodecName { String^ get() { return codecName; } }
		virtual property int64 Bitrate { int64 get() { return bitrate; } }
		virtual property bool IsDefault { bool get() { return isDefault; } }

		property int PixelWidth { int get() { return pixelWidth; } }
		property int PixelHeight { int get() { return pixelHeight; } }
		property int BitsPerSample { int get() { return bitsPerSample; } }

	private:
		String ^ name;
		String^ language;
		String^ codecName;
		int64 bitrate;
		bool isDefault;

		int pixelWidth;
		int pixelHeight;
		int bitsPerSample;
	};


	public ref class SubtitleStreamInfo sealed : public IStreamInfo
	{
	public:
		SubtitleStreamInfo(String^ name, String^ language, String^ codecName, bool isDefault, bool isForced)
		{
			this->name = name;
			this->language = language;
			this->codecName = codecName;
			this->isDefault = isDefault;
			this->isForced = isForced;
			subtitleTrack = ref new TimedMetadataTrack(name, language, TimedMetadataKind::Subtitle);
			subtitleTrack->Label = name;
		}

		void ParseSubtitleSample(TimedTextSample^ sample)
		{
			// do not add cues twice (when seeking)
			if (sample->Position.Duration > maxCuePosition.Duration)
			{
				//this is a shortcut so we do not have to seek all cues if playing from start
				maxCuePosition = sample->Position;
			}
			else
			{
				for each (auto cue in subtitleTrack->Cues)
				{
					if (cue->StartTime.Duration == sample->Position.Duration &&
						cue->Duration.Duration == sample->Duration.Duration)
					{
						return;
					}
				}
			}

			OutputDebugString(sample->Text->Data());

			TimedTextCue^ cue = ref new TimedTextCue();

			cue->Duration = sample->Duration;
			cue->StartTime = sample->Position;

			auto CueRegion = ref new TimedTextRegion();

			TimedTextSize extent;
			extent.Unit = TimedTextUnit::Percentage;
			extent.Width = 100;
			extent.Height = 88;
			CueRegion->Extent = extent;
			TimedTextPoint position;
			position.Unit = TimedTextUnit::Pixels;
			position.X = 0;
			position.Y = 0;
			CueRegion->Position = position;
			CueRegion->DisplayAlignment = TimedTextDisplayAlignment::After;
			CueRegion->Background = Windows::UI::Colors::Transparent;
			CueRegion->ScrollMode = TimedTextScrollMode::Rollup;
			CueRegion->TextWrapping = TimedTextWrapping::Wrap;
			CueRegion->WritingMode = TimedTextWritingMode::LeftRightTopBottom;
			CueRegion->IsOverflowClipped = true;
			CueRegion->ZIndex = 0;
			TimedTextDouble LineHeight;
			LineHeight.Unit = TimedTextUnit::Percentage;
			LineHeight.Value = 100;
			CueRegion->LineHeight = LineHeight;

			auto CueStyle = ref new TimedTextStyle();

			CueStyle->FontFamily = "default";
			TimedTextDouble fontSize;
			fontSize.Unit = TimedTextUnit::Percentage;
			fontSize.Value = 100;
			CueStyle->FontSize = fontSize;
			CueStyle->LineAlignment = TimedTextLineAlignment::Center;
			CueStyle->FontStyle = TimedTextFontStyle::Normal;
			CueStyle->FontWeight = TimedTextWeight::Normal;
			CueStyle->Foreground = Windows::UI::Colors::White;
			CueStyle->Background = Windows::UI::Colors::Transparent;
			/*TimedTextDouble outlineThickness;
			outlineThickness.Unit = TimedTextUnit::Percentage;
			outlineThickness.Value = 5;
			CueStyle->OutlineThickness = outlineThickness;*/
			CueStyle->FlowDirection = TimedTextFlowDirection::LeftToRight;
			CueStyle->OutlineColor = Windows::UI::Colors::Black;

			cue->CueRegion = CueRegion;
			cue->CueStyle = CueStyle;

			if (sample->Text != nullptr) {
				TimedTextLine^ textLine = ref new TimedTextLine();
				textLine->Text = sample->Text;
				cue->Lines->Append(textLine);
			}
			else if (sample->Buffer != nullptr)
			{

			}
			else
			{
				//???is this possible???
				return;
			}
			SubtitleTrack->AddCue(cue);
		}

		virtual property String^ Name { String^ get() { return name; } }
		virtual property String^ Language { String^ get() { return language; } }
		virtual property String^ CodecName { String^ get() { return codecName; } }
		virtual property int64 Bitrate { int64 get() { return 0; } }
		virtual property bool IsDefault { bool get() { return isDefault; } }

		property bool IsForced { bool get() { return isForced; } }
		property TimedMetadataTrack^ SubtitleTrack { TimedMetadataTrack^ get() {
			return subtitleTrack;
		}}


	private:
		String ^ name;
		String^ language;
		String^ codecName;
		bool isDefault;
		TimedMetadataTrack^ subtitleTrack;
		bool isForced;
		TimeSpan maxCuePosition;
	};
}