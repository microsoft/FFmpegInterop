#pragma once
#include "FFmpegInteropMSS.h"
#include "FFmpegInteropConfig.h"
#include <ppl.h>

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

		/// <summary>The duration of the video stream.</summary>
		property TimeSpan Duration
		{
			TimeSpan get()
			{
				return interopMSS->Duration;
			}
		}

		/// <summary>Creates a new FrameGrabber from the specified stream.</summary>
		static IAsyncOperation<FrameGrabber^>^ CreateFromStreamAsync(IRandomAccessStream^ stream)
		{
			return create_async([stream]
			{
				FFmpegInteropConfig^ config = ref new FFmpegInteropConfig();
				config->IsFrameGrabber = true;
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

		/// <summary>Extracts a video frame at the specififed position.</summary>
		/// <param name="position">The position of the requested frame.</param>
		/// <param name="exactSeek">If set to false, this will decode the closest previous key frame, which is faster but not as precise.</param>
		/// <param name="maxFrameSkip">If exactSeek=true, this limits the number of frames to decode after the key frame.</param>
		/// <remarks>The IAsyncOperation result supports cancellation, so long running frame requests (exactSeek=true) can be interrupted.</remarks>
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
					interruption_point();
					
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
					if (exactSeek && seekSucceeded && (position.Duration - sample->Timestamp.Duration > sample->Duration.Duration) &&
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

		/// <summary>Extracts a video frame at the specififed position.</summary>
		/// <param name="position">The position of the requested frame.</param>
		/// <param name="exactSeek">If set to false, this will decode the closest previous key frame, which is faster but not as precise.</param>
		/// <remarks>The IAsyncOperation result supports cancellation, so long running frame requests (exactSeek=true) can be interrupted.</remarks>
		IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(TimeSpan position, bool exactSeek) { return ExtractVideoFrameAsync(position, exactSeek, 0); };
		
		/// <summary>Extracts a video frame at the specififed position.</summary>
		/// <param name="position">The position of the requested frame.</param>
		/// <remarks>The IAsyncOperation result supports cancellation, so long running frame requests (exactSeek=true) can be interrupted.</remarks>
		IAsyncOperation<VideoFrame^>^ ExtractVideoFrameAsync(TimeSpan position) { return ExtractVideoFrameAsync(position, false, 0); };

	};
}



