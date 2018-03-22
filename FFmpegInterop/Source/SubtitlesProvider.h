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

		virtual void QueuePacket(AVPacket *packet) override
		{
			//MediaSampleProvider::QueuePacket(packet);
			TimedTextSample^ sample;
			IBuffer^ buffer;
			String^ timedText;
			TimeSpan position;
			TimeSpan duration;

			position.Duration = LONGLONG(av_q2d(m_pAvStream->time_base) * 10000000 * packet->pts) - m_startOffset;
			duration.Duration = LONGLONG(av_q2d(m_pAvStream->time_base) * 10000000 * packet->duration);


			if (m_pAvCodecCtx->codec_id == AV_CODEC_ID_SUBRIP)
			{
				timedText = ConvertString((char*)packet->buf->data);
			}
			else
			{
				buffer = NativeBuffer::NativeBufferFactory::CreateNativeBuffer(packet->buf->data, packet->buf->size);
			}

			sample = ref new TimedTextSample(timedText, buffer, position, duration, streamInfo);

			av_packet_free(&packet);
		}



	public:
		virtual ~SubtitlesProvider() {}
	};

	delegate void SubtitleSampleProcessed(SubtitlesProvider^ sender, TimedTextSample^ sample);

}