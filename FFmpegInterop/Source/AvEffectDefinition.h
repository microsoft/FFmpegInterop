#pragma once
using namespace Platform;

namespace FFmpegInterop
{
	public ref class AvEffectDefinition sealed
	{
		String ^filterName, ^configString;

	public:
		AvEffectDefinition(String^ _filterName, String^ _configString)
		{
			this->configString = _configString;
			this->filterName = _filterName;
		}

		property String^ FilterName
		{
			String^ get() { return filterName; }
		}

		property String^ Configuration
		{
			String^ get() { return configString; }
		}
	};
}
