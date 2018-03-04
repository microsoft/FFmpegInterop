#pragma once
#include "AbstractEffectFactory.h"

namespace FFmpegInterop
{
	ref class VideoEffectFactory : public AbstractEffectFactory
	{
		AVCodecContext* InputContext;

	internal:

		VideoEffectFactory(AVCodecContext* input_ctx)
		{
			InputContext = input_ctx;
		}

		IAvEffect^ CreateEffect(IVectorView<AvEffectDefinition^>^ definitions) override
		{
			//implement this when someone has ideas
			return nullptr;

		}
	};
}