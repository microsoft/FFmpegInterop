#pragma once
#include "AvEffectDefinition.h"
#include "IAvEffect.h"

using namespace Windows::Foundation::Collections;
using namespace FFmpegInterop;

class AbstractEffectFactory
{
public:
	virtual IAvEffect* CreateEffect(IVectorView<AvEffectDefinition^>^ definitions) = 0;
};


