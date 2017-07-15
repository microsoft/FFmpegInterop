#pragma once
using namespace Platform;
using namespace Windows::Storage::Streams;

namespace FFmpegInterop
{
	public ref class MediaThumbnailData sealed
	{
		IBuffer^ _buffer;
		String^ _extension;

	public:

		property IBuffer^ Buffer
		{
			IBuffer^ get()
			{
				return _buffer;
			}
		}
		property String^ Extension
		{
			String^ get()
			{
				return _extension;
			}
		}

		MediaThumbnailData(IBuffer^ buffer, String^ extension)
		{
			this->_buffer = buffer;
			this->_extension = extension;
		}
	};
}