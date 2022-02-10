#pragma once

namespace HolographicFindSurfaceDemo
{
	class PermissionHelper
	{
	public:
		// Eye Tracking
		static concurrency::task<bool> RequestEyeTrackingPermissionAsync();
		// Voice Input
		static concurrency::task<bool> RequestMicrophonePermissionAsyc();
	};
};
