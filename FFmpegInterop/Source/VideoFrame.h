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

		VideoFrame(IBuffer^ pixelData, unsigned int width, unsigned int height)
		{
			this->pixelData = pixelData;
			this->width = width;
			this->height = height;
		}

		IAsyncOperation<IRandomAccessStream^>^ EncodeAsBmpAsync()
		{
			return create_async([this]
			{
				return this->Encode(BitmapEncoder::BmpEncoderId);
			});
		}

		IAsyncOperation<IRandomAccessStream^>^ EncodeAsJpegAsync()
		{
			return create_async([this]
			{
				return this->Encode(BitmapEncoder::JpegEncoderId);
			});
		}

		IAsyncOperation<IRandomAccessStream^>^ EncodeAsPngAsync()
		{
			return create_async([this]
			{
				return this->Encode(BitmapEncoder::PngEncoderId);
			});
		}

	private:
		IBuffer^ pixelData;
		unsigned int width;
		unsigned int height;

		concurrency::task<IRandomAccessStream^> Encode(Guid encoderGuid)
		{
			// Query the IBufferByteAccess interface.  
			Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> bufferByteAccess;
			reinterpret_cast<IInspectable*>(pixelData)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));

			// Retrieve the buffer data.  
			byte* pixels = nullptr;
			bufferByteAccess->Buffer(&pixels);

			IRandomAccessStream^ outputStream = ref new Windows::Storage::Streams::InMemoryRandomAccessStream();
			auto encoder = co_await BitmapEncoder::CreateAsync(encoderGuid, outputStream);
			encoder->SetPixelData(
				BitmapPixelFormat::Rgba8,
				BitmapAlphaMode::Ignore,
				width,
				height,
				72,
				72,
				ArrayReference<byte>(pixels, pixelData->Length));

			co_await encoder->FlushAsync();

			outputStream->Seek(0);
			co_return outputStream;
		}

	};
}