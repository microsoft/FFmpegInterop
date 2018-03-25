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
			return S_OK;
		}

		virtual void QueuePacket(AVPacket *packet) override
		{
			// always process text subtitles to allow faster subtitle switching
			/*if (!m_isEnabled)
			{
				av_packet_free(&packet);
				return;
			}*/

			String^ timedText;
			TimeSpan position;
			TimeSpan duration;

			position.Duration = LONGLONG(av_q2d(m_pAvStream->time_base) * 10000000 * packet->pts) - m_startOffset;
			duration.Duration = LONGLONG(av_q2d(m_pAvStream->time_base) * 10000000 * packet->duration);


			if (m_pAvCodecCtx->codec_id == AV_CODEC_ID_SUBRIP)
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
				try
				{
					TimedTextCue^ cue = ref new TimedTextCue();

					cue->Duration = duration;
					cue->StartTime = position;
					cue->CueRegion = m_config->SubtitleRegion;
					cue->CueStyle = m_config->SubtitleStyle;

					TimedTextLine^ textLine = ref new TimedTextLine();
					textLine->Text = timedText;
					cue->Lines->Append(textLine);

					SubtitleTrack->AddCue(cue);
				}
				catch (...)
				{
					OutputDebugString(L"Failed to add subtitle cue.");
				}
			}

			av_packet_free(&packet);
		}

	public:
		virtual ~SubtitlesProvider() {}
	};

}