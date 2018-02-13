#pragma once
#include "IAvEffect.h"
#include "AvEffectDefinition.h"
#include "AbstractEffectFactory.h"
#include <mutex>

extern "C"
{
#include <libavformat/avformat.h>
}

using namespace Windows::Foundation::Collections;

namespace FFmpegInterop
{
	ref class UncompressedFrameProvider sealed
	{
		IAvEffect* filter;
		AVFormatContext* m_pAvFormatCtx;
		AVCodecContext* m_pAvCodecCtx;
		AbstractEffectFactory* m_effectFactory;
		boolean m_PresetFilterPending;
		std::mutex lock_mutex;

		~UncompressedFrameProvider() {
			delete filter;
			delete m_effectFactory;
		}

	internal:



		UncompressedFrameProvider(AVFormatContext* p_pAvFormatCtx, AVCodecContext* p_pAvCodecCtx, AbstractEffectFactory* p_effectFactory)
		{
			m_pAvCodecCtx = p_pAvCodecCtx;
			m_pAvFormatCtx = p_pAvFormatCtx;
			m_effectFactory = p_effectFactory;
		}



		void UpdateFilter(IVectorView<AvEffectDefinition^>^ effects)
		{
			lock_mutex.lock();

			//if (m_PresetFilterPending)
			{
				int64 inChannelLayout = m_pAvCodecCtx->channel_layout ? m_pAvCodecCtx->channel_layout : av_get_default_channel_layout(m_pAvCodecCtx->channels);

				delete filter;
				filter = m_effectFactory->CreateEffect(effects);
				m_PresetFilterPending = false;
			}
			lock_mutex.unlock();

		}



		void DisableFilter()
		{
			lock_mutex.lock();

			delete filter;
			filter = nullptr;
			lock_mutex.unlock();

		}

		HRESULT GetFrameFromCodec(AVFrame *avFrame)
		{
			HRESULT hr = avcodec_receive_frame(m_pAvCodecCtx, avFrame);
			if (SUCCEEDED(hr))
			{
				lock_mutex.lock();
				if (filter) {
					hr = filter->AddFrame(avFrame);
					if (SUCCEEDED(hr))
					{
						hr = filter->GetFrame(avFrame);
					}
				}
				lock_mutex.unlock();
			}
			return hr;

		}
	};

}