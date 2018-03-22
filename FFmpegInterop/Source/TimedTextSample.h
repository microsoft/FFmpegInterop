#pragma once
#include "StreamInfo.h"

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;


namespace FFmpegInterop
{
	public ref class TimedTextSample sealed
	{
		IBuffer^ buffer;
		TimeSpan position;
		TimeSpan duration;
		String^ textCue;
		SubtitleStreamInfo^ streamInfo;

	public:
		TimedTextSample(String^ textCue, IBuffer^ buf, TimeSpan pos, TimeSpan dur, SubtitleStreamInfo^ streamInfo)
		{
			this->buffer = buf;
			this->position = pos;
			this->duration = dur;
			this->textCue = textCue;
		}

		property IBuffer^ Buffer
		{
			IBuffer^ get()
			{
				return buffer;
			}
		}

		property TimeSpan Position
		{
			TimeSpan get()
			{
				return position;
			}
		}

		property TimeSpan Duration
		{
			TimeSpan get()
			{
				return duration;
			}
		}

		property String^ Text
		{
			String^ get()
			{
				return textCue;
			}
		}		

		property SubtitleStreamInfo^ SubtitleStream
		{
			SubtitleStreamInfo^ get()
			{
				return streamInfo;
			}
		}
	};
}