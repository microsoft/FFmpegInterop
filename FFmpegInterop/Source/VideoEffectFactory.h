#pragma once
#include "AbstractEffectFactory.h"


class VideoEffectFactory : public AbstractEffectFactory
{
	AVCodecContext* InputContext;

public:

	VideoEffectFactory(AVCodecContext* input_ctx)
	{
		InputContext = input_ctx;
	}

	IAvEffect* CreateEffect(IVectorView<AvEffectDefinition^>^ definitions) override
	{
		//implement this when someone has ideas
		return NULL;

	}
};