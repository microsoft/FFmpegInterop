//*****************************************************************************
//
//	Copyright 2015 Microsoft Corporation
//
//	Licensed under the Apache License, Version 2.0 (the "License");
//	you may not use this file except in compliance with the License.
//	You may obtain a copy of the License at
//
//	http ://www.apache.org/licenses/LICENSE-2.0
//
//	Unless required by applicable law or agreed to in writing, software
//	distributed under the License is distributed on an "AS IS" BASIS,
//	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//	See the License for the specific language governing permissions and
//	limitations under the License.
//
//*****************************************************************************

#include "pch.h"
#include "FFmpegInteropMSS.h"
#include "FFmpegInteropMSS.g.cpp"
#include "StreamFactory.h"
#include "SampleProvider.h"
#include "Metadata.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Foundation::Metadata;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Storage::Streams;
using namespace std;

namespace
{
	// Function to read from file stream. Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
	int FileStreamRead(void* ptr, uint8_t* buf, int bufSize)
	{
		IStream* fileStream{ reinterpret_cast<IStream*>(ptr) };
		ULONG bytesRead{ 0 };

		RETURN_IF_FAILED(fileStream->Read(buf, bufSize, &bytesRead));

		// Assume we've reached EOF if we didn't read any bytes
		RETURN_HR_IF(static_cast<HRESULT>(AVERROR_EOF), bytesRead == 0);

		return bytesRead;
	}

	// Function to seek in file stream. Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
	int64_t FileStreamSeek(void* ptr, int64_t pos, int whence)
	{
		IStream* fileStream{ reinterpret_cast<IStream*>(ptr) };
		LARGE_INTEGER in{ 0 };
		in.QuadPart = pos;
		ULARGE_INTEGER out{ 0 };

		RETURN_IF_FAILED(fileStream->Seek(in, whence, &out));

		return out.QuadPart;
	}
}

namespace winrt::FFmpegInterop::implementation
{
	void FFmpegInteropMSS::InitializeFromStream(_In_ const IRandomAccessStream& fileStream, _In_ const MediaStreamSource& mss, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		auto logger{ FFmpegInteropProvider::InitializeFromStream::Start() };
		[[maybe_unused]] wil::ThreadErrorContext errorContext; // Enable WIL's thread error cache for averror_to_hresult()

		(void) make<FFmpegInteropMSS>(fileStream, mss, config);

		logger.Stop();
	}

	void FFmpegInteropMSS::InitializeFromUri(_In_ const hstring& uri, _In_ const MediaStreamSource& mss, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		auto logger{ FFmpegInteropProvider::InitializeFromUri::Start() };
		[[maybe_unused]] wil::ThreadErrorContext errorContext; // Enable WIL's thread error cache for averror_to_hresult()

		(void) make<FFmpegInteropMSS>(uri, mss, config);

		logger.Stop();
	}

	FFmpegInteropMSS::FFmpegInteropMSS(_In_ const MediaStreamSource& mss) :
		m_mss(mss),
		m_formatContext(avformat_alloc_context()),
		m_reader(m_formatContext.get(), m_streamIdMap)
	{
		THROW_HR_IF_NULL(E_INVALIDARG, mss);
		THROW_IF_NULL_ALLOC(m_formatContext);
	}

	FFmpegInteropMSS::FFmpegInteropMSS(_In_ const IRandomAccessStream& fileStream, _In_ const MediaStreamSource& mss, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config) :
		FFmpegInteropMSS(mss)
	{
		try
		{
			OpenFile(fileStream, config);
			InitFFmpegContext(config);
		}
		catch (...)
		{
			// Notify the MSS that an error occurred
			mss.NotifyError(MediaStreamSourceErrorStatus::UnsupportedMediaFormat);
			throw;
		}
	}

	FFmpegInteropMSS::FFmpegInteropMSS(_In_ const hstring& uri, _In_ const MediaStreamSource& mss, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config) :
		FFmpegInteropMSS(mss)
	{
		try
		{
			string uriA{ to_string(uri) };

			OpenFile(uriA.c_str(), config);
			InitFFmpegContext(config);
		}
		catch (...)
		{
			// Notify the MSS that an error occurred
			mss.NotifyError(MediaStreamSourceErrorStatus::UnsupportedMediaFormat);
			throw;
		}
	}

	void FFmpegInteropMSS::OpenFile(_In_ const IRandomAccessStream& fileStream, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		// Convert async IRandomAccessStream to sync IStream
		THROW_HR_IF_NULL(E_INVALIDARG, fileStream);
		THROW_IF_FAILED(CreateStreamOverRandomAccessStream(winrt::get_unknown(fileStream), __uuidof(m_fileStream), m_fileStream.put_void()));

		// Setup FFmpeg custom IO to access file as stream. This is necessary when accessing any file outside of app installation directory and appdata folder.
		// Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
		constexpr int c_ioBufferSize = 16 * 1024;
		AVBlob_ptr ioBuffer{ av_malloc(c_ioBufferSize) };
		THROW_IF_NULL_ALLOC(ioBuffer);

		m_ioContext.reset(avio_alloc_context(reinterpret_cast<unsigned char*>(ioBuffer.get()), c_ioBufferSize, 0, m_fileStream.get(), FileStreamRead, nullptr, FileStreamSeek));
		THROW_IF_NULL_ALLOC(m_ioContext);
		ioBuffer.release(); // The IO context has taken ownership of the buffer

		m_formatContext->pb = m_ioContext.get();

		OpenFile("", config);
	}

	void FFmpegInteropMSS::OpenFile(_In_z_ const char* uri, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		// Parse the FFmpeg options in config if present
		AVDictionary_ptr options;
		if (config != nullptr)
		{
			StringMap ffmpegOptions{ config.FFmpegOptions() };
			for (const auto& iter : ffmpegOptions)
			{
				string key{ to_string(iter.Key()) };
				string value{ to_string(iter.Value()) };

				AVDictionary* optionsRaw{ options.release() };
				int result{ av_dict_set(&optionsRaw, key.c_str(), value.c_str(), 0) };
				options.reset(exchange(optionsRaw, nullptr));
				THROW_HR_IF_FFMPEG_FAILED(result);
			}
		}

		// Open the format context for the stream
		AVFormatContext* formatContextRaw{ m_formatContext.release() };
		AVDictionary* optionsRaw{ options.release() };
		int result{ avformat_open_input(&formatContextRaw, uri, nullptr, &optionsRaw) }; // The format context is freed on failure
		options.reset(exchange(optionsRaw, nullptr));
		THROW_HR_IF_FFMPEG_FAILED(result);
		m_formatContext.reset(exchange(formatContextRaw, nullptr));

		if (options != nullptr)
		{
			// Options is not null if there was an issue with the provided FFmpeg options such as an invalid key or value.
			WINRT_ASSERT(options == nullptr);
			// TODO: Log options that weren't found
		}
	}

	void FFmpegInteropMSS::InitFFmpegContext(_In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config)
	{
		THROW_HR_IF_FFMPEG_FAILED(avformat_find_stream_info(m_formatContext.get(), nullptr));

		int audioStreamId{ av_find_best_stream(m_formatContext.get(), AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0) };
		int videoStreamId{ av_find_best_stream(m_formatContext.get(), AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0) };
		int thumbnailStreamId{ -1 };
		vector<IMediaStreamDescriptor> pendingAudioStreamDescriptors;
		vector<IMediaStreamDescriptor> pendingVideoStreamDescriptors;

		// Make sure the preferred video stream isn't album/cover art
		if (videoStreamId >= 0 && m_formatContext->streams[videoStreamId]->disposition == AV_DISPOSITION_ATTACHED_PIC)
		{
			videoStreamId = -1;
		}

		for (int i{ 0 }; i < static_cast<int>(m_formatContext->nb_streams); i++)
		{
			AVStream* stream{ m_formatContext->streams[i] };

			// Discard all samples for this stream until it is selected
			stream->discard = AVDISCARD_ALL;

			// Create the sample provider and stream descriptor
			unique_ptr<SampleProvider> sampleProvider;
			IMediaStreamDescriptor streamDescriptor{ nullptr };

			switch (stream->codecpar->codec_type)
			{
			case AVMEDIA_TYPE_AUDIO:
				tie(sampleProvider, streamDescriptor) = StreamFactory::CreateAudioStream(m_formatContext.get(), stream, m_reader, config);

				if (i >= audioStreamId)
				{
					// Add the stream to the MSS
					m_mss.AddStreamDescriptor(streamDescriptor);

					// Check if this is the first audio stream added to the MSS
					if (i == audioStreamId || audioStreamId < 0)
					{
						audioStreamId = i;
						sampleProvider->Select(); // The first audio stream is selected by default

						// Add any audio streams we already enumerated
						for (auto& audioStreamDescriptor : pendingAudioStreamDescriptors)
						{
							m_mss.AddStreamDescriptor(move(audioStreamDescriptor));
						}
						pendingAudioStreamDescriptors.clear();
					}
				}
				else
				{
					// We'll add this stream to the MSS after the preferred audio stream has been enumerated
					pendingAudioStreamDescriptors.push_back(streamDescriptor);
				}

				break;

			case AVMEDIA_TYPE_VIDEO:
				// FFmpeg identifies album/cover art from a music file as a video stream
				if (stream->disposition == AV_DISPOSITION_ATTACHED_PIC)
				{
					if (thumbnailStreamId < 0)
					{
						try
						{
							SetThumbnail(m_mss, stream, config);
							thumbnailStreamId = i;
						}
						CATCH_LOG_MSG("Stream %d: Failed to set thumbnail", stream->index);
					}

					continue;
				}

				tie(sampleProvider, streamDescriptor) = StreamFactory::CreateVideoStream(m_formatContext.get(), stream, m_reader, config);

				if (i >= videoStreamId)
				{
					// Add the stream to the MSS
					m_mss.AddStreamDescriptor(streamDescriptor);

					// Check if this is the first video stream added to the MSS
					if (i == videoStreamId || videoStreamId < 0)
					{
						videoStreamId = i;
						sampleProvider->Select(); // The first video stream is selected by default

						// Add any video streams we already enumerated
						for (auto& videoStreamDescriptor : pendingVideoStreamDescriptors)
						{
							m_mss.AddStreamDescriptor(move(videoStreamDescriptor));
						}
						pendingVideoStreamDescriptors.clear();
					}
				}
				else
				{
					// We'll add this stream to the MSS after the preferred video stream has been enumerated
					pendingVideoStreamDescriptors.push_back(streamDescriptor);
				}

				break;

			case AVMEDIA_TYPE_SUBTITLE:
				// Subtitle streams use TimedMetadataStreamDescriptor which was added in 17134. Check if this type is present.
				// Note: MSS didn't expose subtitle streams in media engine scenarios until 19041.
				if (!ApiInformation::IsTypePresent(L"Windows.Media.Core.TimedMetadataStreamDescriptor"))
				{
					FFMPEG_INTEROP_TRACE("Stream %d: No subtitle support. AVCodec Name = %hs",
						stream->index, avcodec_get_name(stream->codecpar->codec_id));
					continue;
				}

				try
				{
					tie(sampleProvider, streamDescriptor) = StreamFactory::CreateSubtitleStream(m_formatContext.get(), stream, m_reader);
				}
				catch (...)
				{
					// Unsupported subtitle stream. Just ignore.
					FFMPEG_INTEROP_TRACE("Stream %d: Unsupported subtitle stream. AVCodec Name = %hs",
						stream->index, avcodec_get_name(stream->codecpar->codec_id));
					continue;
				}

				// Add the stream to the MSS
				m_mss.AddStreamDescriptor(streamDescriptor);

				break;

			default:
				// Ignore this stream
				FFMPEG_INTEROP_TRACE("Stream %d: Unsupported. AVMediaType = %hs, AVCodec Name = %hs",
					stream->index, av_get_media_type_string(stream->codecpar->codec_type), avcodec_get_name(stream->codecpar->codec_id));
				continue;
			}

			// Add the stream to our maps
			m_streamIdMap[i] = sampleProvider.get();
			m_streamDescriptorMap[move(streamDescriptor)] = move(sampleProvider);
		}

		WINRT_ASSERT(pendingAudioStreamDescriptors.empty());
		WINRT_ASSERT(pendingVideoStreamDescriptors.empty());

		if (m_formatContext->duration > 0)
		{
			// Set the duration
			m_mss.Duration(TimeSpan{ ConvertFromAVTime(m_formatContext->duration, av_get_time_base_q(), HNS_PER_SEC) });
			m_mss.CanSeek(true);
		}
		else
		{
			// Set buffer time to 0 for realtime streaming to reduce latency
			m_mss.BufferTime(TimeSpan{ 0 });
		}

		// Populate metadata
		if (audioStreamId >= 0)
		{
			FFMPEG_INTEROP_TRACE("Stream %d: Populating audio metadata", m_formatContext->streams[audioStreamId]->index);
			PopulateMetadata(m_mss, m_formatContext->streams[audioStreamId]->metadata);
		}

		if (videoStreamId >= 0)
		{
			FFMPEG_INTEROP_TRACE("Stream %d: Populating video metadata", m_formatContext->streams[videoStreamId]->index);
			PopulateMetadata(m_mss, m_formatContext->streams[videoStreamId]->metadata);
		}

		FFMPEG_INTEROP_TRACE("Populating format metadata");
		PopulateMetadata(m_mss, m_formatContext->metadata);

		// Register event handlers. The delegates hold strong references to tie the lifetime of this object to the MSS.
		m_startingRevoker = m_mss.Starting(auto_revoke, { get_strong(), &FFmpegInteropMSS::OnStarting });
		m_sampleRequestedRevoker = m_mss.SampleRequested(auto_revoke, { get_strong(), &FFmpegInteropMSS::OnSampleRequested });
		m_switchStreamsRequestedRevoker = m_mss.SwitchStreamsRequested(auto_revoke, { get_strong(), &FFmpegInteropMSS::OnSwitchStreamsRequested });
		m_closedRevoker = m_mss.Closed(auto_revoke, { get_strong(), &FFmpegInteropMSS::OnClosed });
	}

	void FFmpegInteropMSS::OnStarting(_In_ const MediaStreamSource&, _In_ const MediaStreamSourceStartingEventArgs& args)
	{
		auto logger{ FFmpegInteropProvider::OnStarting::Start() };
		[[maybe_unused]] wil::ThreadErrorContext errorContext; // Enable WIL's thread error cache for averror_to_hresult()

		const MediaStreamSourceStartingRequest request{ args.Request() };
		const IReference<TimeSpan> startPosition{ request.StartPosition() };

		// Check if the start position is null. A null start position indicates we're resuming playback from the current position.
		if (startPosition != nullptr)
		{
			lock_guard<mutex> lock{ m_lock };

			const TimeSpan hnsSeekTime{ startPosition.Value() };
			FFMPEG_INTEROP_TRACE("Seek to %I64d hns", hnsSeekTime.count());

			try
			{
				// Convert the seek time from HNS to AV_TIME_BASE
				int64_t avSeekTime{ ConvertToAVTime(hnsSeekTime.count(), HNS_PER_SEC, av_get_time_base_q()) };
				THROW_HR_IF(MF_E_INVALID_TIMESTAMP, avSeekTime > m_formatContext->duration);

				if (m_formatContext->start_time != AV_NOPTS_VALUE)
				{
					// Adjust the seek time by the start time offset
					avSeekTime += m_formatContext->start_time;
				}

				THROW_HR_IF_FFMPEG_FAILED(avformat_seek_file(m_formatContext.get(), -1, numeric_limits<int64_t>::min(), avSeekTime, avSeekTime, 0));

				for (auto& [streamId, stream] : m_streamIdMap)
				{
					stream->OnSeek(hnsSeekTime.count());
				}

				request.SetActualStartPosition(hnsSeekTime);

				logger.Stop();
			}
			catch (...)
			{
				// Notify the MSS that an error occurred
				m_mss.NotifyError(MediaStreamSourceErrorStatus::Other);
			}
		}
		else
		{
			FFMPEG_INTEROP_TRACE("Resume");
			logger.Stop();
		}
	}

	void FFmpegInteropMSS::OnSampleRequested(_In_ const MediaStreamSource&, _In_ const MediaStreamSourceSampleRequestedEventArgs& args)
	{
		auto logger{ FFmpegInteropProvider::OnSampleRequested::Start() };
		[[maybe_unused]] wil::ThreadErrorContext errorContext; // Enable WIL's thread error cache for averror_to_hresult()

		const MediaStreamSourceSampleRequest request{ args.Request() };

		lock_guard<mutex> lock{ m_lock };

		try
		{
			// Get the next sample for the stream
			m_streamDescriptorMap.at(request.StreamDescriptor())->GetSample(request);

			logger.Stop();
		}
		catch (...)
		{
			const hresult hr{ to_hresult() };
			if (hr == MF_E_END_OF_STREAM)
			{
				// Notify all streams we're at EOF
				for (auto& [streamId, sampleProvider] : m_streamIdMap)
				{
					sampleProvider->NotifyEOF();
				}

				logger.Stop(); // This is an expected error. No need to log it.
			}
			else
			{
				// Notify the MSS that an error occurred
				m_mss.NotifyError(MediaStreamSourceErrorStatus::Other);
			}
		}
	}

	void FFmpegInteropMSS::OnSwitchStreamsRequested(_In_ const MediaStreamSource&, _In_ const MediaStreamSourceSwitchStreamsRequestedEventArgs& args)
	{
		auto logger{ FFmpegInteropProvider::OnSwitchStreamsRequested::Start() };

		const MediaStreamSourceSwitchStreamsRequest request{ args.Request() };
		const IMediaStreamDescriptor oldStreamDescriptor{ request.OldStreamDescriptor() };
		const IMediaStreamDescriptor newStreamDescriptor{ request.NewStreamDescriptor() };

		// The old/new stream descriptors should not be the same and at least one should be valid.
		WINRT_ASSERT(oldStreamDescriptor != newStreamDescriptor);

		lock_guard<mutex> lock{ m_lock };

		try
		{
			if (oldStreamDescriptor != nullptr)
			{
				m_streamDescriptorMap.at(oldStreamDescriptor)->Deselect();
			}

			if (newStreamDescriptor != nullptr)
			{
				m_streamDescriptorMap.at(newStreamDescriptor)->Select();
			}

			logger.Stop();
		}
		catch (...)
		{
			WINRT_ASSERT(false);

			// Notify the MSS that an error occurred
			m_mss.NotifyError(MediaStreamSourceErrorStatus::Other);
		}
	}

	void FFmpegInteropMSS::OnClosed(_In_ const MediaStreamSource&, _In_ const MediaStreamSourceClosedEventArgs& args)
	{
		auto logger{ FFmpegInteropProvider::OnClosed::Start() };

		FFMPEG_INTEROP_TRACE("MediaStreamSourceClosedReason: %d", args.Request().Reason());

		lock_guard<mutex> lock{ m_lock };

		// Unregister event handlers
		// This is critically important to do for the media source app service scenario! If we don't unregister these event handlers, then
		// they'll be released when the MSS is destroyed. That kicks off a race condition between COM releasing the remote interfaces and
		// the remote app process being suspended. If the remote app process is suspended first, then COM may cause a hang until the 
		// remote app process is terminated.
		m_startingRevoker.revoke();
		m_sampleRequestedRevoker.revoke();
		m_switchStreamsRequestedRevoker.revoke();
		m_closedRevoker.revoke();

		// Release the MSS and file stream
		// This is critically important to do for the media source app service scenario! The remote app process may be suspended anytime after 
		// this Closed event is processed. If we don't release the file stream now, then we'll effectively leak the file handle which could 
		// cause file related issues until the remote app process is terminated.
		m_mss = nullptr;
		m_fileStream = nullptr;

		logger.Stop();
	}
}
