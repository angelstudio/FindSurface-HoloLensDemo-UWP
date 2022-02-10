#include "pch.h"

#include "PrimitiveFactory.h"
#include <cmath>

#define DBL_PI 3.14159265358979323846
#define FLT_PI static_cast<float>(DBL_PI)

using namespace HolographicFindSurfaceDemo;

void PrimitiveFactory::CreateUnitPlaneXZ(std::vector<float>& outViertices, std::vector<uint16_t>& outIndices, uint32_t segXDir, uint32_t segZDir)
{
	//assert(segXDir > 0 && segZDir > 0);
	const uint32_t xVtxCnt = segXDir + 1;
	const uint32_t zVtxCnt = segZDir + 1;

	const uint32_t xHalf = xVtxCnt / 2;
	const uint32_t zHalf = zVtxCnt / 2;

	const uint32_t totalVtxCnt = xVtxCnt * zVtxCnt;

	const float xStep = 1.0f / static_cast<float>(segXDir);
	const float zStep = 1.0f / static_cast<float>(segZDir);

	std::vector<float> tmpX(xVtxCnt);
	std::vector<float> tmpZ(zVtxCnt);

	// Calculate X
	tmpX[0] = -0.5f; tmpX[segXDir] = 0.5f;
	if (segXDir % 2 == 0) { tmpX[xHalf] = 0.0f; }
	
	for (uint32_t i = 1; i < xHalf; i++)
	{
		float d = xStep * static_cast<float>(i);

		tmpX[i] = -0.5f + d;
		tmpX[segXDir - i] = 0.5f - d;
	}
	
	// Calculate Z
	tmpZ[0] = -0.5f; tmpZ[segZDir] = 0.5f;
	if (segZDir % 2 == 0) { tmpZ[zHalf] = 0.0f; }

	for (uint32_t i = 1; i < zHalf; i++)
	{
		float d = zStep * static_cast<float>(i);

		tmpZ[i] = -0.5f + d;
		tmpZ[segXDir - i] = 0.5f - d;
	}

	// MixUp
	std::vector<float> vertices(3 * totalVtxCnt);
	float* pVtxDst = vertices.data();

	for (uint32_t v = 0; v < zVtxCnt; v++) 
	{
		for (uint32_t u = 0; u < xVtxCnt; u++) 
		{
			pVtxDst[0] = tmpX[u];
			pVtxDst[1] = 0.0f;
			pVtxDst[2] = tmpZ[v];

			pVtxDst += 3;
		}
	}

	// Indices
	const uint32_t totalIndices = 3 * 2 * segXDir * segZDir;
	std::vector<uint16_t> indices(totalIndices * 2);

	uint16_t* pIdxDst = indices.data();
	uint16_t* pIdxDstBack = indices.data() + totalIndices;

	for (uint32_t v = 0; v < segZDir; v++)
	{
		for (uint32_t u = 0; u < segXDir; u++)
		{
			uint32_t i = v * xVtxCnt + u;

			// Front Face
			pIdxDst[0] = static_cast<uint16_t>(i);
			pIdxDst[1] = static_cast<uint16_t>(i + xVtxCnt);
			pIdxDst[2] = static_cast<uint16_t>(i + 1);

			pIdxDst[3] = static_cast<uint16_t>(i + 1);
			pIdxDst[4] = static_cast<uint16_t>(i + xVtxCnt);
			pIdxDst[5] = static_cast<uint16_t>(i + xVtxCnt + 1);

			// Back Face
			pIdxDstBack[0] = static_cast<uint16_t>(i);
			pIdxDstBack[1] = static_cast<uint16_t>(i + 1);
			pIdxDstBack[2] = static_cast<uint16_t>(i + xVtxCnt);

			pIdxDstBack[3] = static_cast<uint16_t>(i + 1);
			pIdxDstBack[4] = static_cast<uint16_t>(i + xVtxCnt + 1);
			pIdxDstBack[5] = static_cast<uint16_t>(i + xVtxCnt);

			pIdxDst += 6;
			pIdxDstBack += 6;
		}
	}

	// Output
	outViertices.swap(vertices);
	outIndices.swap(indices);
}

void PrimitiveFactory::CreateUnitSphere(std::vector<float>& outViertices, std::vector<uint16_t>& outIndices, uint32_t segRadius, uint32_t segVertical)
{
	// assert( segRadius > 2 && segHeight > 1 )
	const uint32_t vtxRadius = segRadius + 1;
	const uint32_t vtxVertical = segVertical + 1;

	const uint32_t totalVertices = vtxRadius * (vtxVertical - 2) + 2;

	const uint32_t yHalf = vtxVertical / 2;

	const float stepThetaR = FLT_PI * 2.0f / static_cast<float>(segRadius);
	const float stepThetaV = FLT_PI / static_cast<float>(segVertical);

	std::vector<float> tmpRx(vtxRadius);
	std::vector<float> tmpRz(vtxRadius);

	std::vector<float> tmpHr(vtxVertical);
	std::vector<float> tmpHy(vtxVertical);

	// Calculate X-Z values
	tmpRx[0] = 0.0f; tmpRx[segRadius] = 0.0f;
	tmpRz[0] = 1.0f; tmpRz[segRadius] = 1.0f;

	for (uint32_t i = 1; i < segRadius; i++) {
		float theta = stepThetaR * static_cast<float>(i);

		tmpRx[i] = sinf(theta);
		tmpRz[i] = cosf(theta);
	}

	// Calculate Y and Radius values
	tmpHr[0] = 0.0f; tmpHr[segVertical] = 0.0f; // Not-used
	tmpHy[0] = 1.0f; tmpHy[segVertical] = -1.0f;
	if (segVertical % 2 == 0) { tmpHr[yHalf] = 1.0f; tmpHy[yHalf] = 0.0f; }

	for (uint32_t i = 1; i < yHalf; i++) {
		float theta = stepThetaV * static_cast<float>(i);
		float s = sinf(theta);
		float c = cosf(theta);

		tmpHr[i] = s; tmpHr[segVertical - i] = s;
		tmpHy[i] = c; tmpHy[segVertical - i] = -c;
	}

	// Mix-up
	std::vector<float> vertices(3 * totalVertices);
	float* pVtxDst = vertices.data();

	pVtxDst[0] = 0.0f; pVtxDst[1] = 1.0f; pVtxDst[2] = 0.0f;
	pVtxDst += 3;
	for (uint32_t v = 1; v < vtxVertical - 1; v++)
	{
		for (uint32_t u = 0; u < vtxRadius; u++)
		{
			pVtxDst[0] = tmpHr[v] * tmpRx[u];
			pVtxDst[1] = tmpHy[v];
			pVtxDst[2] = tmpHr[v] * tmpRz[u];

			pVtxDst += 3;
		}
	}
	pVtxDst[0] = 0.0f; pVtxDst[1] = -1.0f; pVtxDst[2] = 0.0f;

	// Indices
	const uint32_t totalIndices = 3 * 2 * segRadius * (segVertical - 1);
	std::vector<uint16_t> indices(totalIndices);

	uint16_t* pIdxDst = indices.data();

	// Head
	for (uint32_t u = 0; u < segRadius; u++) {
		pIdxDst[0] = static_cast<uint16_t>(0);
		pIdxDst[1] = static_cast<uint16_t>(u);
		pIdxDst[2] = static_cast<uint16_t>(u + 1);

		pIdxDst += 3;
	}
	// Body
	for (uint32_t v = 0; v < segVertical - 2; v++)
	{
		for (uint32_t u = 0; u < segRadius; u++)
		{
			uint32_t i = v * vtxRadius + u + 1;

			pIdxDst[0] = static_cast<uint16_t>(i);
			pIdxDst[1] = static_cast<uint16_t>(i + vtxRadius);
			pIdxDst[2] = static_cast<uint16_t>(i + 1);

			pIdxDst[3] = static_cast<uint16_t>(i + 1);
			pIdxDst[4] = static_cast<uint16_t>(i + vtxRadius);
			pIdxDst[5] = static_cast<uint16_t>(i + vtxRadius + 1);

			pIdxDst += 6;
		}
	}
	// Tail
	uint32_t b1 = totalVertices - 1;
	for (uint32_t u = 0; u < segRadius; u++) {
		uint32_t i = b1 - vtxRadius + u;

		pIdxDst[0] = static_cast<uint16_t>(i);
		pIdxDst[1] = static_cast<uint16_t>(b1);
		pIdxDst[2] = static_cast<uint16_t>(i + 1);

		pIdxDst += 3;
	}

	// Output
	outViertices.swap(vertices);
	outIndices.swap(indices);

}

void PrimitiveFactory::CreateUnitCylinder(std::vector<float>& outViertices, std::vector<uint16_t>& outIndices, uint32_t segRadius, uint32_t segHeight)
{
	// assert( segRadius > 2 && segHeight > 0 );

	const uint32_t vtxRadius = segRadius + 1;
	const uint32_t vtxHeight = segHeight + 1;

	const uint32_t yHalf = vtxHeight / 2;

	const uint32_t totalVertices = vtxRadius * vtxHeight;

	const float stepTheta = FLT_PI * 2.0f / static_cast<float>(segRadius);
	const float stepH = 1.0f / static_cast<float>(segHeight);

	std::vector<float> tmpRx(vtxRadius);
	std::vector<float> tmpRz(vtxRadius);
	std::vector<float> tmpY(vtxHeight);

	// Calculate X-Z values
	tmpRx[0] = 0.0f; tmpRx[segRadius] = 0.0f;
	tmpRz[0] = 1.0f; tmpRz[segRadius] = 1.0f;

	for (uint32_t i = 1; i < segRadius; i++) {
		float theta = stepTheta * static_cast<float>(i);

		tmpRx[i] = sinf(theta);
		tmpRz[i] = cosf(theta);
	}

	// Calculate Y values
	tmpY[0] = 0.5f; tmpY[segHeight] = -0.5f;
	if (segHeight % 2 == 0) { tmpY[yHalf] = 0.0f; }

	for (uint32_t i = 1; i < yHalf; i++)
	{
		float d = stepH * static_cast<float>(i);

		tmpY[i] = 0.5f - d;
		tmpY[segHeight - i] = -0.5f + d;
	}

	// Mix-up
	std::vector<float> vertices(3 * totalVertices);
	float* pVtxDst = vertices.data();

	for (uint32_t v = 0; v < vtxHeight; v++)
	{
		for (uint32_t u = 0; u < vtxRadius; u++)
		{
			pVtxDst[0] = tmpRx[u];
			pVtxDst[1] = tmpY[v];
			pVtxDst[2] = tmpRz[u];

			pVtxDst += 3;
		}
	}

	// Indices
	const uint32_t totalIndices = 3 * 2 * segRadius * segHeight;
	std::vector<uint16_t> indices(totalIndices);
	uint16_t* pIdxDst = indices.data();

	for (uint32_t v = 0; v < segHeight; v++)
	{
		for (uint32_t u = 0; u < segRadius; u++)
		{
			uint32_t i = v * vtxRadius + u;

			pIdxDst[0] = static_cast<uint16_t>(i);
			pIdxDst[1] = static_cast<uint16_t>(i + vtxRadius);
			pIdxDst[2] = static_cast<uint16_t>(i + 1);

			pIdxDst[3] = static_cast<uint16_t>(i + 1);
			pIdxDst[4] = static_cast<uint16_t>(i + vtxRadius);
			pIdxDst[5] = static_cast<uint16_t>(i + vtxRadius + 1);

			pIdxDst += 6;
		}
	}

	// Output
	outViertices.swap(vertices);
	outIndices.swap(indices);
}