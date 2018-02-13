#pragma once
#include "AbstractEffectFactory.h"
#include "AudioFilter.h"

class AudioEffectFactory : public AbstractEffectFactory
{
	AVCodecContext* InputContext;

public:

	AudioEffectFactory(AVCodecContext* input_ctx)
	{
		InputContext = input_ctx;
	}

	IAvEffect* CreateEffect(IVectorView<AvEffectDefinition^>^ definitions) override
	{
		AudioFilter* filter = new AudioFilter(InputContext);
		auto hr = filter->AllocResources(definitions);
		if (SUCCEEDED(hr))
		{
			return filter;
		}
		else
		{
			delete filter;
			return NULL;
		}

	}
};