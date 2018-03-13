#pragma once
#include "FFmpegInteropMSS.h"
#include "FFmpegInteropConfig.h"
using namespace concurrency;

namespace FFmpegInterop {
	public ref class FrameGrabber sealed
	{

		FFmpegInteropMSS^ frameSource;

	internal:
		FrameGrabber(FFmpegInteropMSS^ m_frameSource) {
			this->frameSource = m_frameSource;
		}



	public:

		virtual ~FrameGrabber() {
			if (frameSource)
				delete frameSource;
		}

		property TimeSpan Duration
		{
			TimeSpan get()
			{
				return frameSource->Duration;
			}
		}

		static IAsyncOperation<FrameGrabber^>^ CreateFrameGrabberFromStreamAsync(IRandomAccessStream^ stream)
		{
			return create_async([stream]
			{
				return create_task([stream]
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
					return ref new FrameGrabber(result);
				});
			});
		}


		IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(TimeSpan position, bool exactSeek, int maxFrameSkip)
		{
			return create_async([this, position, exactSeek, maxFrameSkip]
			{
				return create_task([this, position, exactSeek, maxFrameSkip]
				{
					auto interopMSS = this->frameSource;
					if (interopMSS->VideoStream == nullptr)
					{
						throw ref new Exception(E_FAIL, "No video stream found in file (or no suitable decoder available).");
					}

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
								throw ref new Exception(E_FAIL, "Failed to decode video frame.");
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
							interopMSS->VideoStream->PixelHeight);
						interopMSS = nullptr;
						return result;

					}

				});

			});

		}


		IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(TimeSpan position, bool exactSeek) { return ExtractVideoFrameAsync(position, exactSeek, 0); };
		IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(TimeSpan position) { return ExtractVideoFrameAsync(position, false, 0); };
		IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync() { return ExtractVideoFrameAsync({ 0 }, false, 0); };

	};
}



