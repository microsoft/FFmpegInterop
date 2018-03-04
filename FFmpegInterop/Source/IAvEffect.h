#pragma once
extern "C"
{
#include <libavformat/avformat.h>
}
namespace FFmpegInterop
{
	ref class IAvEffect abstract
	{
	public:
		virtual	~IAvEffect() {}

	internal:
		
		virtual HRESULT AddFrame(AVFrame* frame) abstract;
		virtual HRESULT GetFrame(AVFrame* frame) abstract;
	};
}
