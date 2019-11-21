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

#pragma once

namespace winrt::FFmpegInterop::implementation
{
	class FFmpegReader;

	class MediaSampleProvider
	{
	public:
		MediaSampleProvider(_In_ const AVStream* stream, _Inout_ FFmpegReader& reader);

		virtual ~MediaSampleProvider() = default;

		void Select() noexcept;
		void Deselect() noexcept;
		void OnSeek(_In_ int64_t seekTime) noexcept;

		virtual void GetSample(_Inout_ const Windows::Media::Core::MediaStreamSourceSampleRequest& request);
		virtual void QueuePacket(_In_ AVPacket_ptr packet);

	protected:
		AVPacket_ptr GetPacket();
		bool HasPacket() const noexcept { return !m_packetQueue.empty(); }

		virtual void Flush() noexcept;
		virtual std::tuple<Windows::Storage::Streams::IBuffer, int64_t, int64_t> GetSampleData();
		virtual void SetSampleProperties(const Windows::Media::Core::MediaStreamSample& sample) { }

		const AVStream* m_stream;

	private:
		bool m_isSelected{ false };
		FFmpegReader& m_reader;
		bool m_isDiscontinuous{ true };
		std::deque<AVPacket_ptr> m_packetQueue;
		int64_t m_startOffset;
		int64_t m_nextSamplePts;
	};
}
