#pragma once

#include <FindSurface.hpp>
#include "Content/ShaderStructures.h"

namespace HolographicFindSurfaceDemo
{
	class FindSurfaceHelper
	{
	public:
		enum ErrorLevel
		{
			ERROR_LEVEL_NORMAL,
			ERROR_LEVEL_HIGH,
			ERROR_LEVEL_LOW
		};

	public:
		static void FillFindSurfaceParameter(FindSurface* pContext, float shortestDistanceToSeedPointThroughHeadForwardDirection, ErrorLevel errLv = ERROR_LEVEL_NORMAL);

		static void FillConstantBufferFromResult(
			InstanceConstantBuffer& out, 
			const FindSurfaceResult* pResult,
			const winrt::Windows::Foundation::Numerics::float3& headFowardDirection,
			const winrt::Windows::Foundation::Numerics::float3& headUpDirection,
			const DirectX::XMFLOAT4X4* pointCloudModel = nullptr
		);
		
	};
};
