#pragma once

#include <cstdint>

namespace HolographicFindSurfaceDemo
{
	class PrimitiveFactory
	{
	public:
		static void CreateUnitPlaneXZ(std::vector<float>& outViertices, std::vector<uint16_t>& outIndices, uint32_t segXDir = 1, uint32_t segZDir = 1);
		static void CreateUnitSphere(std::vector<float>& outViertices, std::vector<uint16_t>& outIndices, uint32_t segRadius = 16, uint32_t segVertical = 16);
		static void CreateUnitCylinder(std::vector<float>& outViertices, std::vector<uint16_t>& outIndices, uint32_t segRadius = 32, uint32_t segHeight = 1);
	};
};
