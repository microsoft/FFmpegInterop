#pragma once

#include <robuffer.h>
#include <pplawait.h>

using namespace Windows::Foundation;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Storage::Streams;
using namespace Concurrency;

namespace FFmpegInterop
{
	public ref class VideoFrame sealed
	{
	public:
		property IBuffer^ PixelData { IBuffer^ get() { return pixelData; }}
		property unsigned int Width { unsigned int get() { return width; }}
		property unsigned int Height { unsigned int get() { return height; }}
		property TimeSpan Timestamp { TimeSpan get() { return timestamp; }}

		VideoFrame(IBuffer^ pixelData, unsigned int width, unsigned int height, TimeSpan timestamp)
		{
			this->pixelData = pixelData;
			this->width = width;
			this->height = height;
			this->timestamp = timestamp;
		}

		virtual ~VideoFrame()
		{
			
		}

		IAsyncAction^ EncodeAsBmpAsync(IRandomAccessStream^ stream)
		{
			return create_async([this, stream]
			{
				return this->Encode(stream, BitmapEncoder::BmpEncoderId);
			});
		}

		IAsyncAction^ EncodeAsJpegAsync(IRandomAccessStream^ stream)
		{
			return create_async([this, stream]
			{
				return this->Encode(stream, BitmapEncoder::JpegEncoderId);
			});
		}

		IAsyncAction^ EncodeAsPngAsync(IRandomAccessStream^ stream)
		{
			return create_async([this, stream]
			{
				return this->Encode(stream, BitmapEncoder::PngEncoderId);
			});
		}

	private:
		IBuffer ^ pixelData;
		unsigned int width;
		unsigned int height;
		TimeSpan timestamp;

		task<void> Encode(IRandomAccessStream^ stream, Guid encoderGuid)
		{
			// Query the IBufferByteAccess interface.  
			Microsoft::WRL::ComPtr<IBufferByteAccess> bufferByteAccess;
			reinterpret_cast<IInspectable*>(pixelData)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));

			// Retrieve the buffer data.  
			byte* pixels = nullptr;
			bufferByteAccess->Buffer(&pixels);
			auto length = pixelData->Length;

			return create_task(BitmapEncoder::CreateAsync(encoderGuid, stream)).then([this, pixels, length](task<BitmapEncoder^> encoder)
			{
				auto encoderValue = encoder.get();
				encoderValue->SetPixelData(
					BitmapPixelFormat::Bgra8,
					BitmapAlphaMode::Ignore,
					width,
					height,
					72,
					72,
					ArrayReference<byte>(pixels, length));

				return encoderValue->FlushAsync();
			});
		}

	};
}