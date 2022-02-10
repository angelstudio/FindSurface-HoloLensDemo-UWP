#include "pch.h"
#include "PermissionHelper.h"
#include <sstream>

// #define MF_E_UNSUPPORTED_CAPTURE_DEVICE_PRESENT 0xC00DAFC8

using namespace HolographicFindSurfaceDemo;
using namespace concurrency;

using namespace winrt::Windows::Perception::People;
using namespace winrt::Windows::UI::Input;
using namespace winrt::Windows::Media::Capture;

// Eye Tracking
task<bool> PermissionHelper::RequestEyeTrackingPermissionAsync()
{
	if (EyesPose::IsSupported())
	{
		return create_task(
			[] {
				try {
					auto status = EyesPose::RequestAccessAsync().get();
					if (status == GazeInputAccessStatus::Allowed) {
						return true;
					}

					switch (status)
					{
					case GazeInputAccessStatus::DeniedBySystem:
						OutputDebugString(TEXT("EyesPose::RequestAccessAsync(): DeniedBySystem\n"));
						break;
					case GazeInputAccessStatus::DeniedByUser:
						OutputDebugString(TEXT("EyesPose::RequestAccessAsync(): DeniedByUser\n"));
						break;
					case GazeInputAccessStatus::Unspecified:
						OutputDebugString(TEXT("EyesPose::RequestAccessAsync(): Unspecified -> DeniedBySystem\n"));
						break;
					}
					return false;
				}
				catch (winrt::hresult_error const& ex)
				{
					std::wostringstream wss;
					wss << L"EyesPose::RequestAccessAsync() throw exception: " << ex.message().c_str() << std::endl;

					OutputDebugString(wss.str().c_str());
					return false;
				}
			}
		);
	}
	return create_task([] { return false; }, task_continuation_context::get_current_winrt_context());
}

// Voice Input
task<bool> PermissionHelper::RequestMicrophonePermissionAsyc()
{
	return create_task(
		[]
		{
			auto settings = MediaCaptureInitializationSettings();
			settings.StreamingCaptureMode(StreamingCaptureMode::Audio);
			settings.MediaCategory(MediaCategory::Speech);

			auto capture = MediaCapture();
			try
			{
				capture.InitializeAsync(settings).get();
			}
			catch (winrt::hresult_error const& ex)
			{
				std::wostringstream wss;
				wss << L"Request Microphone Permission: " << ex.message().c_str() << std::endl;

				OutputDebugString(wss.str().c_str());
				return false;
			}
			capture.Close(); // Explicit release
			
			return true;
		}
	);
}