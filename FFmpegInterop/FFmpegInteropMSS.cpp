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
#include "UncompressedAudioSampleProvider.h"
#include "UncompressedVideoSampleProvider.h"
#include "H264AVCSampleProvider.h"
#include "SubtitleSampleProvider.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::MediaProperties;
using namespace std;

namespace
{
	// Function to read from file stream. Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
	int FileStreamRead(void* ptr, uint8_t* buf, int bufSize)
	{
		IStream* fileStream = reinterpret_cast<IStream*>(ptr);
		ULONG bytesRead = 0;

		if (FAILED(fileStream->Read(buf, bufSize, &bytesRead)))
		{
			return AVERROR_EXTERNAL;
		}

		// Assume we've reached EOF if we didn't read any bytes
		if (bytesRead == 0)
		{
			return AVERROR_EOF;
		}

		return bytesRead;
	}

	// Function to seek in file stream. Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
	int64_t FileStreamSeek(void* ptr, int64_t pos, int whence)
	{
		IStream* fileStream = reinterpret_cast<IStream*>(ptr);
		LARGE_INTEGER in{ 0 };
		in.QuadPart = pos;
		ULARGE_INTEGER out{ 0 };

		if (FAILED(fileStream->Seek(in, whence, &out)))
		{
			return AVERROR_EXTERNAL;
		}

		return out.QuadPart;
	}
}

namespace winrt::FFmpegInterop::implementation
{
	FFmpegInterop::FFmpegInteropMSS FFmpegInteropMSS::CreateFromStream(const IRandomAccessStream& fileStream, const MediaStreamSource& mss)
	{
		auto logger{ CreateFromStreamActivity::Start() };

		FFmpegInterop::FFmpegInteropMSS ffmpegInteropMSS{ make<FFmpegInteropMSS>(fileStream, mss) };

		logger.Stop();
		return ffmpegInteropMSS;
	}

	FFmpegInterop::FFmpegInteropMSS FFmpegInteropMSS::CreateFromUri(const hstring& uri, const MediaStreamSource& mss)
	{
		auto logger{ CreateFromUriActivity::Start() };

		FFmpegInterop::FFmpegInteropMSS ffmpegInteropMSS{ make<FFmpegInteropMSS>(uri, mss) };

		logger.Stop();
		return ffmpegInteropMSS;
	}

	FFmpegInteropMSS::FFmpegInteropMSS(const MediaStreamSource& mss) :
		m_mss(mss),
		m_formatContext(avformat_alloc_context()),
		m_reader(m_formatContext.get(), m_streamIdMap)
	{
		THROW_HR_IF_NULL(E_INVALIDARG, mss);
		THROW_IF_NULL_ALLOC(m_formatContext);
	}

	FFmpegInteropMSS::FFmpegInteropMSS(const IRandomAccessStream& fileStream, const MediaStreamSource& mss) :
		FFmpegInteropMSS(mss)
	{
		try
		{
			OpenFile(fileStream);
			InitFFmpegContext();
		}
		catch (...)
		{
			// Notify the MSS that an error occurred
			mss.NotifyError(MediaStreamSourceErrorStatus::Other);
			throw;
		}
	}

	FFmpegInteropMSS::FFmpegInteropMSS(const hstring& uri, const MediaStreamSource& mss) :
		FFmpegInteropMSS(mss)
	{
		try
		{
			wstring_convert<codecvt_utf16<wchar_t>> conv;
			string uriA{ conv.to_bytes(uri.c_str()) };

			OpenFile(uriA.c_str());
			InitFFmpegContext();
		}
		catch (...)
		{
			// Notify the MSS that an error occurred
			mss.NotifyError(MediaStreamSourceErrorStatus::Other);
			throw;
		}
	}

	void FFmpegInteropMSS::OpenFile(const IRandomAccessStream& fileStream)
	{
		// Convert async IRandomAccessStream to sync IStream
		THROW_HR_IF_NULL(E_INVALIDARG, fileStream);
		THROW_IF_FAILED(CreateStreamOverRandomAccessStream(reinterpret_cast<::IUnknown*>(get_abi(fileStream)), __uuidof(m_fileStream), m_fileStream.put_void()));

		// Setup FFmpeg custom IO to access file as stream. This is necessary when accessing any file outside of app installation directory and appdata folder.
		// Credit to Philipp Sch http://www.codeproject.com/Tips/489450/Creating-Custom-FFmpeg-IO-Context
		constexpr int c_ioBufferSize = 16 * 1024;
		AVBlob_ptr ioBuffer{ av_malloc(c_ioBufferSize) };
		THROW_IF_NULL_ALLOC(ioBuffer);

		m_ioContext.reset(avio_alloc_context(reinterpret_cast<unsigned char*>(ioBuffer.get()), c_ioBufferSize, 0, m_fileStream.get(), FileStreamRead, 0, FileStreamSeek));
		THROW_IF_NULL_ALLOC(m_ioContext);
		ioBuffer.release(); // The IO context has taken ownership of the buffer

		m_formatContext->pb = m_ioContext.get();

		OpenFile(nullptr);
	}

	void FFmpegInteropMSS::OpenFile(const char* uri)
	{
		// TODO: Add ffmpeg config options

		// Open the format context for the stream
		AVFormatContext* formatContext = m_formatContext.release();
		THROW_IF_FFMPEG_FAILED(avformat_open_input(&formatContext, uri, nullptr, nullptr)); // The format context is freed on failure
		m_formatContext.reset(exchange(formatContext, nullptr));
	}

	void FFmpegInteropMSS::InitFFmpegContext()
	{
		THROW_IF_FFMPEG_FAILED(avformat_find_stream_info(m_formatContext.get(), nullptr));

		bool hasVideo = false;
		bool hasAudio = false;

		for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
		{
			const AVStream* stream = m_formatContext->streams[i];
			IMediaStreamDescriptor streamDescriptor{ nullptr };
			unique_ptr<MediaSampleProvider> sampleProvider;

			switch (stream->codecpar->codec_type)
			{
			case AVMEDIA_TYPE_AUDIO:
				tie(streamDescriptor, sampleProvider) = CreateAudioStream(stream);

				if (!hasAudio)
				{
					hasAudio = true;

					// The first audio stream is selected
					sampleProvider->Select();
				}
				break;

			case AVMEDIA_TYPE_VIDEO:
				// FFmpeg identifies album/cover art from a music file as a video stream
				// Avoid creating unnecessarily video stream from this album/cover art
				if (stream->disposition == AV_DISPOSITION_ATTACHED_PIC)
				{
					continue;
				}

				tie(streamDescriptor, sampleProvider) = CreateVideoStream(stream);

				if (!hasVideo)
				{
					hasVideo = true;

					// The first video stream is selected
					sampleProvider->Select();
				}
				break;

			case AVMEDIA_TYPE_SUBTITLE:
				try
				{
					tie(streamDescriptor, sampleProvider) = CreateSubtitleStream(stream);
				}
				catch (...)
				{
					if (to_hresult() == MF_E_INVALIDMEDIATYPE)
					{
						// We don't support the subtitle codec. Just ignore this stream.
						continue;
					}

					throw;
				}
				break;

			default:
				// Ignore this stream
				continue;
			}

			// Add the stream to the MSS
			m_mss.AddStreamDescriptor(streamDescriptor);

			// Add the stream to our maps
			m_streamIdMap[i] = sampleProvider.get();
			m_streamDescriptorMap[move(streamDescriptor)] = move(sampleProvider);
		}

		if (m_formatContext->duration > 0)
		{
			// Set the duration
			m_mss.Duration(TimeSpan{ static_cast<int64_t>(static_cast<double>(m_formatContext->duration) * HNS_PER_SEC / AV_TIME_BASE) });
			m_mss.CanSeek(true);
		}
		else
		{
			// Set buffer time to 0 for realtime streaming to reduce latency
			m_mss.BufferTime(TimeSpan{ 0 });
		}

		// Register event handlers. The delegates hold strong references to tie the lifetime of this object to the MSS.
		m_startingEventToken = m_mss.Starting({ get_strong(), &FFmpegInteropMSS::OnStarting });
		m_sampleRequestedEventToken = m_mss.SampleRequested({ get_strong(), &FFmpegInteropMSS::OnSampleRequested });
		m_switchStreamsRequestedEventToken = m_mss.SwitchStreamsRequested({ get_strong(), &FFmpegInteropMSS::OnSwitchStreamsRequested });
		m_closedEventToken = m_mss.Closed({ get_strong(), &FFmpegInteropMSS::OnClosed });
	}
	
	tuple<AudioStreamDescriptor, unique_ptr<MediaSampleProvider>> FFmpegInteropMSS::CreateAudioStream(const AVStream* stream)
	{
		AudioEncodingProperties audioEncProp{ nullptr };
		unique_ptr<MediaSampleProvider> audioSampleProvider;

		switch (stream->codecpar->codec_id)
		{
		case AV_CODEC_ID_AAC:
			if (stream->codecpar->extradata_size == 0)
			{
				audioEncProp = AudioEncodingProperties::CreateAacAdts(stream->codecpar->sample_rate, stream->codecpar->channels, static_cast<uint32_t>(stream->codecpar->bit_rate));
			}
			else
			{
				audioEncProp = AudioEncodingProperties::CreateAac(stream->codecpar->sample_rate, stream->codecpar->channels, static_cast<uint32_t>(stream->codecpar->bit_rate));
			}

			audioSampleProvider = make_unique<MediaSampleProvider>(stream, m_reader);
			break;

		case AV_CODEC_ID_MP3:
			audioEncProp = AudioEncodingProperties::CreateMp3(stream->codecpar->sample_rate, stream->codecpar->channels, static_cast<uint32_t>(stream->codecpar->bit_rate));
			audioSampleProvider = make_unique<MediaSampleProvider>(stream, m_reader);
			break;

		case AV_CODEC_ID_OPUS:
			WINRT_ASSERT(c_codecMap.find(stream->codecpar->codec_id) != c_codecMap.end());

			audioEncProp = AudioEncodingProperties::AudioEncodingProperties();
			audioEncProp.Subtype(c_codecMap.at(stream->codecpar->codec_id));
			audioEncProp.SampleRate(stream->codecpar->sample_rate);
			audioEncProp.ChannelCount(stream->codecpar->channels);
			audioEncProp.Bitrate(static_cast<uint32_t>(stream->codecpar->bit_rate));

			audioSampleProvider = make_unique<MediaSampleProvider>(stream, m_reader);
			break;

		default:
			constexpr uint32_t bitsPerSample = 16;
			audioEncProp = AudioEncodingProperties::CreatePcm(stream->codecpar->sample_rate, stream->codecpar->channels, bitsPerSample);
			audioSampleProvider = make_unique<UncompressedAudioSampleProvider>(stream, m_reader);
			break;
		}

		AudioStreamDescriptor audioStreamDescriptor{ audioEncProp };
		SetStreamDescriptorProperties(stream, audioStreamDescriptor);

		return { move(audioStreamDescriptor), move(audioSampleProvider) };
	}

	tuple<VideoStreamDescriptor, unique_ptr<MediaSampleProvider>> FFmpegInteropMSS::CreateVideoStream(const AVStream* stream)
	{
		VideoEncodingProperties videoEncProp{ nullptr };
		unique_ptr<MediaSampleProvider> videoSampleProvider;
		bool isUncompressedFormat = false;

		switch (stream->codecpar->codec_id)
		{
		case AV_CODEC_ID_H264:
			videoEncProp = VideoEncodingProperties::CreateH264();
			videoSampleProvider = make_unique<H264AVCSampleProvider>(stream, m_reader);
			break;

		default:
			isUncompressedFormat = true;
			videoEncProp = VideoEncodingProperties::CreateUncompressed(MediaEncodingSubtypes::Nv12(), stream->codecpar->width, stream->codecpar->height);
			videoSampleProvider = make_unique<UncompressedVideoSampleProvider>(stream, m_reader);

			// TODO: Update framerate from AVCodecContext
			break;
		}

		MediaPropertySet videoProp{ videoEncProp.Properties() };

		if (isUncompressedFormat)
		{
			videoProp.Insert(MF_MT_INTERLACE_MODE, PropertyValue::CreateUInt32(MFVideoInterlace_MixedInterlaceOrProgressive));
		}
		else
		{
			videoEncProp.ProfileId(stream->codecpar->profile);
			videoEncProp.Height(stream->codecpar->height);
			videoEncProp.Width(stream->codecpar->width);
		}

		if (stream->codecpar->sample_aspect_ratio.num > 0 && stream->codecpar->sample_aspect_ratio.den != 0)
		{
			MediaRatio pixelAspectRatio{ videoEncProp.PixelAspectRatio() };
			pixelAspectRatio.Numerator(stream->codecpar->sample_aspect_ratio.num);
			pixelAspectRatio.Denominator(stream->codecpar->sample_aspect_ratio.den);
		}

		const AVDictionaryEntry* rotateTag = av_dict_get(stream->metadata, "rotate", nullptr, 0);
		if (rotateTag != nullptr)
		{
			videoProp.Insert(MF_MT_VIDEO_ROTATION, PropertyValue::CreateUInt32(atoi(rotateTag->value)));
		}

		if (stream->avg_frame_rate.num != 0 || stream->avg_frame_rate.den != 0)
		{
			MediaRatio frameRate{ videoEncProp.FrameRate() };
			frameRate.Numerator(stream->avg_frame_rate.num);
			frameRate.Denominator(stream->avg_frame_rate.den);
		}

		videoEncProp.Bitrate(static_cast<uint32_t>(stream->codecpar->bit_rate));

		VideoStreamDescriptor videoStreamDescriptor{ videoEncProp };
		SetStreamDescriptorProperties(stream, videoStreamDescriptor);
		
		return { move(videoStreamDescriptor), move(videoSampleProvider) };
	}

	tuple<TimedMetadataStreamDescriptor, unique_ptr<MediaSampleProvider>> FFmpegInteropMSS::CreateSubtitleStream(const AVStream* stream)
	{
		// Check if we recognize the codec
		auto subtypeIter = c_codecMap.find(stream->codecpar->codec_id);
		THROW_HR_IF(MF_E_INVALIDMEDIATYPE, subtypeIter == c_codecMap.end());

		// Create encoding properties
		TimedMetadataEncodingProperties subtitleEncProp;

		subtitleEncProp.Subtype(subtypeIter->second);

		if (stream->codecpar->extradata != nullptr && stream->codecpar->extradata_size > 0)
		{
			subtitleEncProp.SetFormatUserData({ stream->codecpar->extradata, stream->codecpar->extradata + stream->codecpar->extradata_size });
		}

		// Create stream descriptor
		TimedMetadataStreamDescriptor subtitleStreamDescriptor{ subtitleEncProp };
		SetStreamDescriptorProperties(stream, subtitleStreamDescriptor);

		return { move(subtitleStreamDescriptor), make_unique<SubtitleSampleProvider>(stream, m_reader) };
	}

	void FFmpegInteropMSS::SetStreamDescriptorProperties(const AVStream* stream, const IMediaStreamDescriptor& streamDescriptor)
	{
		wstring_convert<codecvt_utf16<wchar_t>> conv;

		const AVDictionaryEntry* titleTag = av_dict_get(stream->metadata, "title", nullptr, 0);
		if (titleTag != nullptr)
		{
			streamDescriptor.Name(conv.from_bytes(titleTag->value));
		}

		const AVDictionaryEntry* languageTag = av_dict_get(stream->metadata, "language", nullptr, 0);
		if (languageTag != nullptr)
		{
			streamDescriptor.Language(conv.from_bytes(languageTag->value));
		}
	}

	void FFmpegInteropMSS::OnStarting(const MediaStreamSource& sender, const MediaStreamSourceStartingEventArgs& args)
	{
		const MediaStreamSourceStartingRequest request{ args.Request() };
		const TimeSpan startPosition{ request.StartPosition().Value() };

		auto logger{ OnStartingActivity::Start() };
		lock_guard<mutex> lock{ m_lock };

		try
		{
			// Convert the seek time from HNS to AV_TIME_BASE
			int64_t seekTime = static_cast<int64_t>(static_cast<double>(startPosition.count()) * AV_TIME_BASE / HNS_PER_SEC);

			if (m_formatContext->start_time != AV_NOPTS_VALUE)
			{
				// Adjust the seek time by the start time offset
				seekTime += m_formatContext->start_time;
			}

			THROW_HR_IF(MF_E_INVALID_TIMESTAMP, seekTime > m_formatContext->duration);
			THROW_IF_FFMPEG_FAILED(avformat_seek_file(m_formatContext.get(), -1, numeric_limits<int64_t>::min(), seekTime, seekTime, 0));

			for (auto& [streamId, stream] : m_streamIdMap)
			{
				stream->OnSeek(seekTime);
			}

			request.SetActualStartPosition(startPosition);

			logger.Stop();
		}
		catch (...)
		{
			// Notify the MSS that an error occurred
			m_mss.NotifyError(MediaStreamSourceErrorStatus::Other);
		}
	}

	void FFmpegInteropMSS::OnSampleRequested(const MediaStreamSource& sender, const MediaStreamSourceSampleRequestedEventArgs& args)
	{
		const MediaStreamSourceSampleRequest& request = args.Request();

		auto logger{ OnSampleRequestedActivity::Start() };
		lock_guard<mutex> lock{ m_lock };

		try
		{
			m_streamDescriptorMap.at(request.StreamDescriptor())->GetSample(request);

			logger.Stop();
		}
		catch (...)
		{
			// Notify the MSS that an error occurred
			m_mss.NotifyError(MediaStreamSourceErrorStatus::Other);
		}
	}

	void FFmpegInteropMSS::OnSwitchStreamsRequested(const MediaStreamSource& sender, const MediaStreamSourceSwitchStreamsRequestedEventArgs& args)
	{
		const MediaStreamSourceSwitchStreamsRequest request{ args.Request() };
		const IMediaStreamDescriptor oldStreamDescriptor{ request.OldStreamDescriptor() };
		const IMediaStreamDescriptor newStreamDescriptor{ request.NewStreamDescriptor() };

		// The old/new stream descriptors should not be the same and at least one should be valid.
		WINRT_ASSERT(oldStreamDescriptor != newStreamDescriptor);

		auto logger{ OnSwitchStreamsRequestedActivity::Start() };
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
			// Notify the MSS that an error occurred
			m_mss.NotifyError(MediaStreamSourceErrorStatus::Other);
		}
	}

	void FFmpegInteropMSS::OnClosed(const MediaStreamSource& sender, const MediaStreamSourceClosedEventArgs& args)
	{
		auto logger{ OnClosedActivity::Start() };

		lock_guard<mutex> lock{ m_lock };

		// Unregister event handlers
		m_mss.Starting(m_startingEventToken);
		m_mss.SampleRequested(m_sampleRequestedEventToken);
		m_mss.SwitchStreamsRequested(m_switchStreamsRequestedEventToken);
		m_mss.Closed(m_closedEventToken);

		// Release all references and clear all data structures
		m_mss = nullptr;
		m_fileStream = nullptr;
		m_ioContext.reset();
		m_formatContext.reset();
		m_streamDescriptorMap.clear();
		m_streamIdMap.clear();

		logger.Stop();
	}
}

/*
HRESULT FFmpegInteropMSS::ParseOptions(PropertySet^ ffmpegOptions)
{
	HRESULT hr = S_OK;

	// Convert FFmpeg options given in PropertySet to AVDictionary. List of options can be found in https://www.ffmpeg.org/ffmpeg-protocols.html
	if (ffmpegOptions != nullptr)
	{
		auto options = ffmpegOptions->First();

		while (options->HasCurrent)
		{
			String^ key = options->Current->Key;
			std::wstring keyW(key->Begin());
			std::string keyA(keyW.begin(), keyW.end());
			const char* keyChar = keyA.c_str();

			// Convert value from Object^ to const char*. avformat_open_input will internally convert value from const char* to the correct type
			String^ value = options->Current->Value->ToString();
			std::wstring valueW(value->Begin());
			std::string valueA(valueW.begin(), valueW.end());
			const char* valueChar = valueA.c_str();

			// Add key and value pair entry
			if (av_dict_set(&m_ffmpegOptions, keyChar, valueChar, 0) < 0)
			{
				hr = E_INVALIDARG;
				break;
			}

			options->MoveNext();
		}
	}

	return hr;
}

MediaThumbnailData ^ FFmpegInteropMSS::ExtractThumbnail()
{
	if (thumbnailStreamIndex != AVERROR_STREAM_NOT_FOUND)
	{
		// FFmpeg identifies album/cover art from a music file as a video stream
		// Avoid creating unnecessarily video stream from this album/cover art
		if (m_formatContext->streams[thumbnailStreamIndex]->disposition == AV_DISPOSITION_ATTACHED_PIC)
		{
			auto imageStream = m_formatContext->streams[thumbnailStreamIndex];
			//save album art to file.
			String^ extension = ".jpeg";
			switch (imageStream->codecpar->codec_id)
			{
			case AV_CODEC_ID_MJPEG:
			case AV_CODEC_ID_MJPEGB:
			case AV_CODEC_ID_JPEG2000:
			case AV_CODEC_ID_JPEGLS: extension = ".jpeg"; break;
			case AV_CODEC_ID_PNG: extension = ".png"; break;
			case AV_CODEC_ID_BMP: extension = ".bmp"; break;
			}


			auto vector = ref new Array<uint8_t>(imageStream->attached_pic.data, imageStream->attached_pic.size);
			DataWriter^ writer = ref new DataWriter();
			writer->WriteBytes(vector);

			return (ref new MediaThumbnailData(writer->DetachBuffer(), extension));
		}
	}

	return nullptr;
}

HRESULT FFmpegInteropMSS::ConvertCodecName(const char* codecName, String^ *outputCodecName)
{
	HRESULT hr = S_OK;

	// Convert codec name from const char* to Platform::String
	auto codecNameChars = codecName;
	size_t newsize = strlen(codecNameChars) + 1;
	wchar_t * wcstring = new(std::nothrow) wchar_t[newsize];
	if (wcstring == nullptr)
	{
		hr = E_OUTOFMEMORY;
	}

	if (SUCCEEDED(hr))
	{
		size_t convertedChars = 0;
		mbstowcs_s(&convertedChars, wcstring, newsize, codecNameChars, _TRUNCATE);
		*outputCodecName = ref new Platform::String(wcstring);
		delete[] wcstring;
	}

	return hr;
}
*/
