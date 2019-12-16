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

using namespace winrt;
using namespace winrt::FFmpegInterop::implementation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Storage::Streams;
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
		wstring_convert<codecvt_utf8<wchar_t>> conv;
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

	bool hasVideo{ false };
	bool hasAudio{ false };

	for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
	{
		const AVStream* stream{ m_formatContext->streams[i] };

		// Create the sample provider and stream descriptor
		unique_ptr<SampleProvider> sampleProvider;
		IMediaStreamDescriptor streamDescriptor{ nullptr };

		switch (stream->codecpar->codec_type)
		{
		case AVMEDIA_TYPE_AUDIO:
			tie(sampleProvider, streamDescriptor) = StreamFactory::CreateAudioStream(stream, m_reader);

			if (!hasAudio)
			{
				hasAudio = true;
				sampleProvider->Select(); // The first audio stream is selected
			}

			break;

		case AVMEDIA_TYPE_VIDEO:
			// FFmpeg identifies album/cover art from a music file as a video stream
			// Avoid creating unnecessarily video stream from this album/cover art
			if (stream->disposition == AV_DISPOSITION_ATTACHED_PIC)
			{
				continue;
			}

			tie(sampleProvider, streamDescriptor) = StreamFactory::CreateVideoStream(stream, m_reader);

			if (!hasVideo)
			{
				hasVideo = true;
				sampleProvider->Select(); // The first video stream is selected
			}

			break;

		case AVMEDIA_TYPE_SUBTITLE:
			try
			{
				tie(sampleProvider, streamDescriptor) = StreamFactory::CreateSubtitleStream(stream, m_reader);
			}
			catch (...)
			{
				// Stream creation failed. Just ignore this subtitle stream.
				continue;
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

void FFmpegInteropMSS::OnStarting(const MediaStreamSource& sender, const MediaStreamSourceStartingEventArgs& args)
{
	auto logger{ OnStartingActivity::Start() };

	const MediaStreamSourceStartingRequest request{ args.Request() };
	const IReference<TimeSpan> startPosition{ request.StartPosition() };

	// Check if the start position is null. A null start position indicates we're resuming playback from the current position.
	if (startPosition != nullptr)
	{
		const TimeSpan mfSeekTime{ startPosition.Value() };
			
		lock_guard<mutex> lock{ m_lock };

		try
		{
			// Convert the seek time from HNS to AV_TIME_BASE
			int64_t avSeekTime = static_cast<int64_t>(static_cast<double>(mfSeekTime.count())* AV_TIME_BASE / HNS_PER_SEC);

			if (m_formatContext->start_time != AV_NOPTS_VALUE)
			{
				// Adjust the seek time by the start time offset
				avSeekTime += m_formatContext->start_time;
			}

			THROW_HR_IF(MF_E_INVALID_TIMESTAMP, avSeekTime > m_formatContext->duration);
			THROW_IF_FFMPEG_FAILED(avformat_seek_file(m_formatContext.get(), -1, numeric_limits<int64_t>::min(), avSeekTime, avSeekTime, 0));

			for (auto& [streamId, stream] : m_streamIdMap)
			{
				stream->OnSeek(avSeekTime);
			}

			request.SetActualStartPosition(mfSeekTime);

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
		logger.Stop();
	}
}

void FFmpegInteropMSS::OnSampleRequested(const MediaStreamSource& sender, const MediaStreamSourceSampleRequestedEventArgs& args)
{
	auto logger{ OnSampleRequestedActivity::Start() };

	const MediaStreamSourceSampleRequest& request = args.Request();

	lock_guard<mutex> lock{ m_lock };

	try
	{
		m_streamDescriptorMap.at(request.StreamDescriptor())->GetSample(request);

		logger.Stop();
	}
	catch (...)
	{
		const HRESULT hr{ to_hresult() };
		if (hr != MF_E_END_OF_STREAM)
		{
			// Notify the MSS that an error occurred
			m_mss.NotifyError(MediaStreamSourceErrorStatus::Other);
		}
	}
}

void FFmpegInteropMSS::OnSwitchStreamsRequested(const MediaStreamSource& sender, const MediaStreamSourceSwitchStreamsRequestedEventArgs& args)
{
	auto logger{ OnSwitchStreamsRequestedActivity::Start() };

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
