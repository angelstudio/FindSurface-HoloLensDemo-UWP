#pragma once

namespace HolographicFindSurfaceDemo
{
	inline int pickPoint(
        const winrt::Windows::Foundation::Numerics::float3 const& gazeOrigin, 
        const winrt::Windows::Foundation::Numerics::float3 const& gazeDirection,
        const DirectX::XMFLOAT3* pPtList, 
        size_t count, 
        DirectX::XMMATRIX pointCloudModel
    )
	{
		constexpr float PROBE_RADIUS_ON_METER = 0.015f; // 1.5 cm
		constexpr float PR_SQ_PLUS_ONE = 1.0f + (PROBE_RADIUS_ON_METER * PROBE_RADIUS_ON_METER);

		DirectX::XMVECTOR det = DirectX::XMMatrixDeterminant(pointCloudModel);
		pointCloudModel = DirectX::XMMatrixInverse(&det, pointCloudModel);

		DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&gazeOrigin);
		DirectX::XMVECTOR dir = DirectX::XMLoadFloat3(&gazeDirection);

        // Transform Spatial Coordinate System to Point Cloud Local Coordinate System
		pos = DirectX::XMVector3TransformCoord(pos, pointCloudModel);
		dir = DirectX::XMVector3TransformNormal(dir, pointCloudModel);

        int pickIdx = -1;
        int pickIdxExt = -1;

        float minLen = FLT_MAX;
        float maxCos = -FLT_MAX;

        for (size_t i = 0; i < count; i++) {
            DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&pPtList[i]);
            v = DirectX::XMVectorSubtract(v, pos);

            float len1 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(v, dir));
            if (len1 < FLT_EPSILON) {
                continue;
            }

            // Get Square Length of Vector (Origin to Point)
            v = DirectX::XMVector3LengthSq(v);

            float len1Sq = len1 * len1;
            float distSq = DirectX::XMVectorGetX(v);
            if (distSq < (PR_SQ_PLUS_ONE * len1Sq)) // 1. Inside Probe Radius
            {
#if 1
                // find most closest point to camera ray
                float len2Sq = distSq - len1Sq;
                if (len2Sq < minLen) {
                    minLen = len2Sq;
                    pickIdx = static_cast<int>(i);
                }
#else
                // find most closest point to camera (in z-direction distance)
                if (len1 < minLen) {
                    minLen = len1;
                    pickIdx = static_cast<int>(i);
                }
#endif

            }
            else { // 2. Outside ProbeRadius
                v = DirectX::XMVectorSqrt(v);
                float c = len1 / DirectX::XMVectorGetX(v);
                if (c > maxCos) {
                    maxCos = c;
                    pickIdxExt = static_cast<int>(i);
                }
            }
        }

        return pickIdx < 0 ? pickIdxExt : pickIdx;
	}
};