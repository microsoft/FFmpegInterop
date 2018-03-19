#pragma once
#include "FFmpegInteropMSS.h"
#include "FFmpegInteropConfig.h"
using namespace concurrency;

namespace FFmpegInterop {
	public ref class FrameGrabber sealed
	{

		FFmpegInteropMSS^ interopMSS;

	internal:
		FrameGrabber(FFmpegInteropMSS^ interopMSS) {
			this->interopMSS = interopMSS;
		}

	public:

		virtual ~FrameGrabber() {
			if (interopMSS)
				delete interopMSS;
		}

		property TimeSpan Duration
		{
			TimeSpan get()
			{
				return interopMSS->Duration;
			}
		}

		static IAsyncOperation<FrameGrabber^>^ CreateFromStreamAsync(IRandomAccessStream^ stream)
		{
			return create_async([stream]
			{
				FFmpegInteropConfig^ config = ref new FFmpegInteropConfig();
				config->IsFrameGrabber = true;
				config->PassthroughVideoH264 = false;
				config->PassthroughVideoH264 = false;
				config->PassthroughVideoH264Hi10P = false;
				config->PassthroughVideoHEVC = false;

				auto result = FFmpegInteropMSS::CreateFromStream(stream, config, nullptr);
				if (result == nullptr)
				{
					throw ref new Exception(E_FAIL, "Could not create MediaStreamSource.");
				}
				if (result->VideoStream == nullptr)
				{
					throw ref new Exception(E_FAIL, "No video stream found in file (or no suitable decoder available).");
				}
				if (result->VideoSampleProvider == nullptr)
				{
					throw ref new Exception(E_FAIL, "No video stream found in file (or no suitable decoder available).");
				}
				return ref new FrameGrabber(result);
			});
		}

		IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(TimeSpan position, bool exactSeek, int maxFrameSkip)
		{
			return create_async([this, position, exactSeek, maxFrameSkip]
			{
				bool seekSucceeded = false;
				if (interopMSS->Duration.Duration > position.Duration)
				{
					seekSucceeded = SUCCEEDED(interopMSS->Seek(position));
				}

				int framesSkipped = 0;

				MediaStreamSample^ lastSample = nullptr;

				while (true)
				{

					auto sample = interopMSS->VideoSampleProvider->GetNextSample();
					if (sample == nullptr)
					{
						// if we hit end of stream, use last decoded sample (if any), otherwise fail
						if (lastSample != nullptr)
						{
							sample = lastSample;
							seekSucceeded = false;
						}
						else
						{
							throw ref new Exception(E_FAIL, "Failed to decode video frame, or end of stream.");
						}
					}
					else
					{
						lastSample = sample;
					}

					// if exact seek, continue decoding until we have the right sample
					if (exactSeek && seekSucceeded && (position.Duration - sample->Timestamp.Duration > sample->Duration.Duration / 2) &&
						(maxFrameSkip <= 0 || framesSkipped++ < maxFrameSkip))
					{
						continue;
					}

					auto result = ref new VideoFrame(sample->Buffer,
						interopMSS->VideoStream->PixelWidth,
						interopMSS->VideoStream->PixelHeight,
						sample->Timestamp);
					return result;

				}
			});
		}

		IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(TimeSpan position, bool exactSeek) { return ExtractVideoFrameAsync(position, exactSeek, 0); };
		IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(TimeSpan position) { return ExtractVideoFrameAsync(position, false, 0); };

	};
}



