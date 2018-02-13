#pragma once
using namespace Platform;

namespace FFmpegInterop
{
	public ref class AvEffectDefinition sealed
	{
		String ^filterName, ^configString;
		~AvEffectDefinition()
		{
			delete filterName;
			delete configString;
		}



	public:
		AvEffectDefinition(String^ _filterName, String^ _configString)
		{
			this->configString = _configString;
			this->filterName = _filterName;
		}

		property String^ FilterName
		{

		public: String ^ get()
		{
			return filterName;
		}
		}


		property String^ Configuration
		{
		public: String ^ get()
		{
			return configString;
		}
		}
	};
}
