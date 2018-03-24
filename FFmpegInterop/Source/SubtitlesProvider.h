#pragma once
#include "CompressedSampleProvider.h"
#include "TimedTextSample.h"
#include "StreamInfo.h"
#include "NativeBufferFactory.h"


namespace FFmpegInterop
{
	ref class SubtitlesProvider : CompressedSampleProvider
	{
		SubtitleStreamInfo^ streamInfo;

	internal:
		SubtitlesProvider(FFmpegReader^ reader,
			AVFormatContext* avFormatCtx,
			AVCodecContext* avCodecCtx,
			FFmpegInteropConfig^ config,
			int index,
			SubtitleStreamInfo^ info)
			: CompressedSampleProvider(reader, avFormatCtx, avCodecCtx, config, index)
		{
			this->streamInfo = info;
		}

		Platform::String ^ convertFromString(const std::string & input)
		{
			std::wstring w_str = std::wstring(input.begin(), input.end());
			return ref new Platform::String(w_str.c_str(), (unsigned int)w_str.length());
		}

		virtual void QueuePacket(AVPacket *packet) override
		{
			if (!m_isEnabled)
			{
				av_packet_free(&packet);
				return;
			}

			TimedTextSample^ sample;
			IBuffer^ buffer;
			String^ timedText;
			TimeSpan position;
			TimeSpan duration;

			position.Duration = LONGLONG(av_q2d(m_pAvStream->time_base) * 10000000 * packet->pts) - m_startOffset;
			duration.Duration = LONGLONG(av_q2d(m_pAvStream->time_base) * 10000000 * packet->duration);


			if (m_pAvCodecCtx->codec_id == AV_CODEC_ID_SUBRIP)
			{
				auto str = std::string((char*)packet->buf->data);

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
				auto str = std::string((char*)packet->buf->data);
				
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
				buffer = NativeBuffer::NativeBufferFactory::CreateNativeBuffer(packet->buf->data, packet->buf->size);
			}

			sample = ref new TimedTextSample(timedText, buffer, position, duration);
			streamInfo->ParseSubtitleSample(sample);
			av_packet_free(&packet);
		}



	public:
		virtual ~SubtitlesProvider() {}
	};

	delegate void SubtitleSampleProcessed(SubtitlesProvider^ sender, TimedTextSample^ sample);

}