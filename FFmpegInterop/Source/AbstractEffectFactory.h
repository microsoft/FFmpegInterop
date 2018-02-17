#pragma once
#include "AvEffectDefinition.h"
#include "IAvEffect.h"

using namespace Windows::Foundation::Collections;


namespace FFmpegInterop {
	ref class AbstractEffectFactory abstract
	{
	internal:
		virtual IAvEffect^ CreateEffect(IVectorView<AvEffectDefinition^>^ definitions) abstract;
	};
}


