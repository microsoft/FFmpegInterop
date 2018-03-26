#pragma once

#include <string>
#include <codecvt>

#include "CompressedSampleProvider.h"
#include "StreamInfo.h"
#include "NativeBufferFactory.h"


namespace FFmpegInterop
{
	ref class SubtitlesProvider : CompressedSampleProvider
	{
	internal:
		SubtitlesProvider(FFmpegReader^ reader,
			AVFormatContext* avFormatCtx,
			AVCodecContext* avCodecCtx,
			FFmpegInteropConfig^ config,
			int index)
			: CompressedSampleProvider(reader, avFormatCtx, avCodecCtx, config, index)
		{
		}

		property TimedMetadataTrack^ SubtitleTrack;

		// convert UTF-8 string to wstring
		std::wstring utf8_to_wstring(const std::string& str)
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
			return myconv.from_bytes(str);
		}

		Platform::String ^ convertFromString(const std::wstring & input)
		{
			return ref new Platform::String(input.c_str(), (unsigned int)input.length());
		}

		HRESULT Initialize() override
		{
			InitializeNameLanguageCodec();
			SubtitleTrack = ref new TimedMetadataTrack(Name, Language, TimedMetadataKind::Subtitle);
			SubtitleTrack->Label = Name != nullptr ? Name : Language;
			cueExitedToken = SubtitleTrack->CueExited += ref new Windows::Foundation::TypedEventHandler<Windows::Media::Core::TimedMetadataTrack ^, Windows::Media::Core::MediaCueEventArgs ^>(this, &FFmpegInterop::SubtitlesProvider::OnCueExited);
			return S_OK;
		}

		virtual void QueuePacket(AVPacket *packet) override
		{
			if (packet->pos > maxCuePosition)
			{
				maxCuePosition = packet->pos;
			}
			else if (addedCues.find(packet->pos) != addedCues.end())
			{
				av_packet_free(&packet);
				return;
			}

			addedCues[packet->pos] = packet->pos;

			String^ timedText;
			TimeSpan position;
			TimeSpan duration;

			position.Duration = LONGLONG(av_q2d(m_pAvStream->time_base) * 10000000 * packet->pts) - m_startOffset;
			duration.Duration = LONGLONG(av_q2d(m_pAvStream->time_base) * 10000000 * packet->duration);


			if (m_pAvCodecCtx->codec_id == AV_CODEC_ID_SUBRIP || 
				m_pAvCodecCtx->codec_id == AV_CODEC_ID_SRT || 
				m_pAvCodecCtx->codec_id == AV_CODEC_ID_TEXT ||
				m_pAvCodecCtx->codec_id == AV_CODEC_ID_WEBVTT)
			{
				auto str = utf8_to_wstring(std::string((char*)packet->buf->data));

				// TODO we could try to forward some font style tags (if whole text is wrapped in <i> or <b>)
				// TODO we might also have to look for &nbsp; and others?

				// strip html tags from string
				while (true)
				{
					auto nextEffect = str.find('<');
					if (nextEffect >= 0)
					{
						auto endEffect = str.find('>', nextEffect);
						if (endEffect > nextEffect)
						{
							if (endEffect < str.length() - 1)
							{
								str = str.substr(0, nextEffect).append(str.substr(endEffect + 1));
							}
							else
							{
								str = str.substr(0, nextEffect);
							}
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}

				timedText = convertFromString(str);
			}
			else if (m_pAvCodecCtx->codec_id == AV_CODEC_ID_ASS || m_pAvCodecCtx->codec_id == AV_CODEC_ID_SSA)
			{
				auto str = utf8_to_wstring(std::string((char*)packet->buf->data));

				// strip effects from string
				while (true)
				{
					auto nextEffect = str.find('{');
					if (nextEffect >= 0)
					{
						auto endEffect = str.find('}', nextEffect);
						if (endEffect > nextEffect)
						{
							if (endEffect < str.length() - 1)
							{
								str = str.substr(0, nextEffect).append(str.substr(endEffect + 1));
							}
							else
							{
								str = str.substr(0, nextEffect);
							}
						}
						else
						{
							break;
						}
					}
					else
					{
						break;
					}
				}

				// now text should be last element of comma separated list
				auto lastComma = str.find_last_of(',');
				if (lastComma > 0 && lastComma < str.length() - 1)
				{
					str = str.substr(lastComma + 1);
					timedText = convertFromString(str);
				}
			}
			else
			{
				// not supported
			}

			if (timedText)
			{
				TimedTextCue^ cue = ref new TimedTextCue();

				cue->Duration = duration;
				cue->StartTime = position;
				cue->CueRegion = m_config->SubtitleRegion;
				cue->CueStyle = m_config->SubtitleStyle;

				TimedTextLine^ textLine = ref new TimedTextLine();
				textLine->Text = timedText;
				cue->Lines->Append(textLine);

				AddCue(cue);
			}

			av_packet_free(&packet);
		}

	private:

		void AddCue(IMediaCue^ cue)
		{
			mutex.lock();
			try
			{
				// to avoid flicker, we try to add new cues only after active cues are finished
				if (SubtitleTrack->ActiveCues->Size > 0)
				{
					bool addToPending = true;
					for each (auto active in SubtitleTrack->ActiveCues)
					{
						if (active->StartTime.Duration + active->Duration.Duration > cue->StartTime.Duration)
						{
							addToPending = false;
							break;
						}
					}
					if (addToPending)
					{
						pendingCues.push_back(cue);
					}
					else
					{
						SubtitleTrack->AddCue(cue);
					}
				}
				else
				{
					SubtitleTrack->AddCue(cue);
				}
			}
			catch (...)
			{
				OutputDebugString(L"Failed to add subtitle cue.");
			}
			mutex.unlock();
		}

		void OnCueExited(TimedMetadataTrack ^sender, MediaCueEventArgs ^args)
		{
			mutex.lock();
			try
			{
				for each (auto cue in pendingCues)
				{
					SubtitleTrack->AddCue(cue);
				}
			}
			catch (...)
			{
				OutputDebugString(L"Failed to add subtitle cue.");
			}
			pendingCues.clear();
			mutex.unlock();
		}

		std::mutex mutex;
		std::vector<IMediaCue^> pendingCues;
		std::map<int64,int64> addedCues;
		int64 maxCuePosition;
		EventRegistrationToken cueExitedToken;

	public:
		virtual ~SubtitlesProvider() 
		{
			if (SubtitleTrack)
			{
				SubtitleTrack->CueExited -= cueExitedToken;
				SubtitleTrack = nullptr;
			}
		}
};

}


