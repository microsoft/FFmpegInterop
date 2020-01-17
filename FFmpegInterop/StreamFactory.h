//*****************************************************************************
//
//	Copyright 2019 Microsoft Corporation
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

#pragma once

namespace winrt::FFmpegInterop
{
	struct FFmpegInteropMSSConfig;
}

namespace winrt::FFmpegInterop::implementation
{
	class Reader;
	class SampleProvider;

	class StreamFactory
	{
	public:
		static std::tuple<std::unique_ptr<SampleProvider>, Windows::Media::Core::AudioStreamDescriptor> CreateAudioStream(_In_ const AVStream* stream, _In_ Reader& reader, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);
		static std::tuple<std::unique_ptr<SampleProvider>, Windows::Media::Core::VideoStreamDescriptor> CreateVideoStream(_In_ const AVStream* stream, _In_ Reader& reader, _In_opt_ const FFmpegInterop::FFmpegInteropMSSConfig& config);
		static std::tuple<std::unique_ptr<SampleProvider>, Windows::Media::Core::TimedMetadataStreamDescriptor> CreateSubtitleStream(_In_ const AVStream* stream, _In_ Reader& reader);

	private:
		StreamFactory() = delete;

		static void SetStreamDescriptorProperties(_In_ const AVStream* stream, _Inout_ const Windows::Media::Core::IMediaStreamDescriptor& streamDescriptor);
	};
}
