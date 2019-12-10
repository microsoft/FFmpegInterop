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
#include "App.h"
#include "MainPage.h"

using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::MediaPlayerCPP::implementation
{
	/// <summary>
	/// Initializes the singleton application object.  This is the first line of authored code
	/// executed, and as such is the logical equivalent of main() or WinMain().
	/// </summary>
	App::App()
	{
		InitializeComponent();
		Suspending({ this, &App::OnSuspending });

		// FFmpegInterop::FFmpegInteropLogging::SetLogLevel(FFmpegInterop::LogLevel::Info);
		// FFmpegInterop::FFmpegInteropLogging::SetLogProvider(this);

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
		UnhandledException([this](IInspectable const&, UnhandledExceptionEventArgs const& e)
			{
				if (IsDebuggerPresent())
				{
					auto errorMessage = e.Message();
					__debugbreak();
				}
			});
#endif
	}

	/*
	void App::Log(FFmpegInterop::LogLevel level, String^ message)
	{
		OutputDebugString(message->Data());
	}
	*/

	/// <summary>
	/// Invoked when the application is launched normally by the end user.  Other entry points
	/// will be used such as when the application is launched to open a specific file.
	/// </summary>
	/// <param name="e">Details about the launch request and process.</param>
	void App::OnLaunched(const LaunchActivatedEventArgs& e)
	{
		Frame rootFrame{ nullptr };
		auto content = Window::Current().Content();
		if (content)
		{
			rootFrame = content.try_as<Frame>();
		}

		// Do not repeat app initialization when the Window already has content,
		// just ensure that the window is active
		if (rootFrame == nullptr)
		{
			// Create a Frame to act as the navigation context and associate it with
			// a SuspensionManager key
			rootFrame = Frame();

			rootFrame.NavigationFailed({ this, &App::OnNavigationFailed });

			if (e.PreviousExecutionState() == ApplicationExecutionState::Terminated)
			{
				// Restore the saved session state only when appropriate, scheduling the
				// final launch steps after the restore is complete
			}

			if (e.PrelaunchActivated() == false)
			{
				if (rootFrame.Content() == nullptr)
				{
					// When the navigation stack isn't restored navigate to the first page,
					// configuring the new page by passing required information as a navigation
					// parameter
					rootFrame.Navigate(xaml_typename<MediaPlayerCPP::MainPage>(), box_value(e.Arguments()));
				}
				// Place the frame in the current Window
				Window::Current().Content(rootFrame);
				// Ensure the current window is active
				Window::Current().Activate();
			}
		}
		else
		{
			if (e.PrelaunchActivated() == false)
			{
				if (rootFrame.Content() == nullptr)
				{
					// When the navigation stack isn't restored navigate to the first page,
					// configuring the new page by passing required information as a navigation
					// parameter
					rootFrame.Navigate(xaml_typename<MediaPlayerCPP::MainPage>(), box_value(e.Arguments()));
				}
				// Ensure the current window is active
				Window::Current().Activate();
			}
		}
	}

	/// <summary>
	/// Invoked when application execution is being suspended.  Application state is saved
	/// without knowing whether the application will be terminated or resumed with the contents
	/// of memory still intact.
	/// </summary>
	/// <param name="sender">The source of the suspend request.</param>
	/// <param name="e">Details about the suspend request.</param>
	void App::OnSuspending(const IInspectable&, const SuspendingEventArgs&)
	{
		// Save application state and stop any background activity
	}

	/// <summary>
	/// Invoked when Navigation to a certain page fails
	/// </summary>
	/// <param name="sender">The Frame which failed navigation</param>
	/// <param name="e">Details about the navigation failure</param>
	void App::OnNavigationFailed(const IInspectable&, const NavigationFailedEventArgs& e)
	{
		throw hresult_error(E_FAIL, hstring(L"Failed to load Page ") + e.SourcePageType().Name);
	}
}
