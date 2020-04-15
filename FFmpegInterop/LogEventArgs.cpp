//*****************************************************************************
//
//	Copyright 2020 Microsoft Corporation
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
#include "LogEventArgs.h"
#include "LogEventArgs.g.cpp"

using namespace std;

namespace winrt::FFmpegInterop::implementation
{
    LogEventArgs::LogEventArgs(FFmpegInterop::LogLevel level, hstring message) :
        m_level(level),
        m_message(move(message))
    {

    }

    FFmpegInterop::LogLevel LogEventArgs::Level()
    {
        return m_level;
    }

    hstring LogEventArgs::Message()
    {
        return m_message;
    }
}
