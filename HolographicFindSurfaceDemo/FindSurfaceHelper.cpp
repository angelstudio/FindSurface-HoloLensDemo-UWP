#include "pch.h"
#include "FindSurfaceHelper.h"

#include "Content/MeshRenderer.h"

//#define FLT_LERP(a,b,r) (static_cast<float>(a)+static_cast<float>(r)*(static_cast<float>(b)-static_cast<float>(a)))

using namespace HolographicFindSurfaceDemo;
using namespace DirectX;

void FindSurfaceHelper::FillFindSurfaceParameter(FindSurface* pContext, float distance, ErrorLevel errLv)
{
	constexpr float _baseError = 0.002f;  // 2 mm at meter
	float errorDistance = distance > 1.0f ? (distance - 1.0f) : 0.0f;

	// Adjust Error & Mean Distance via distance
	float adjustError = _baseError + (0.0012f * errorDistance); // increase 1.2 mm per meter
	float adjustMeanDist = (0.0052f * distance) * 5.0f; // increase 5.2 mm per meter (including error distance) and up to five time of base.

	// Add extra error adjust value.
	switch (errLv)
	{
	case ERROR_LEVEL_NORMAL:
		adjustError += 0.001f; // add 1mm
		break;
	case ERROR_LEVEL_HIGH:
		adjustError += 0.003f; // add 3mm
		break;
	case ERROR_LEVEL_LOW:
		adjustError -= 0.001f; // sub 1mm
		break;
	}

	pContext->setMeasurementAccuracy(adjustError);
	pContext->setMeanDistance(adjustMeanDist);
	pContext->setLateralExtension(FS_SEARCH_LEVEL::FS_LEVEL_DEFAULT);
	pContext->setRadialExpansion(FS_SEARCH_LEVEL::FS_LEVEL_DEFAULT);
}

static const XMFLOAT4 Y_DIR = { 0.0f, 1.0f, 0.0f, 0.0f };
static const XMFLOAT4 HALF = { 0.5f, 0.5f, 0.5f, 0.5f };
static const XMFLOAT4 QUARTER = { 0.25f, 0.25f, 0.25f, 0.25f };

void FindSurfaceHelper::FillConstantBufferFromResult(
	InstanceConstantBuffer& out, 
	const FindSurfaceResult* pResult,
	const winrt::Windows::Foundation::Numerics::float3& headFowardDirection,
	const winrt::Windows::Foundation::Numerics::float3& headUpDirection,
	const XMFLOAT4X4* pointCloudModel
)
{
	XMMATRIX baseModel = pointCloudModel ? XMLoadFloat4x4(pointCloudModel) : XMMatrixIdentity();
	XMVECTOR lookAt = XMVector3Normalize( XMLoadFloat3(&headFowardDirection) );
	XMVECTOR upDir = XMVector3Normalize( XMLoadFloat3(&headUpDirection) );

	if (pointCloudModel) {
		XMVECTOR det = XMMatrixDeterminant(baseModel);
		XMMATRIX invBaseModel = XMMatrixInverse(&det, baseModel);

		// Transform to Point Cloud Coordinate System
		lookAt = XMVector3TransformNormal(lookAt, invBaseModel);
		upDir = XMVector3TransformNormal(upDir, invBaseModel);
	}

	switch (pResult->getType())
	{
		case FS_TYPE_PLANE:
		{
			XMVECTOR lowerLeft = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->plane_param.ll));
			XMVECTOR lowerRight = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->plane_param.lr));
			XMVECTOR upperRight = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->plane_param.ur));
			XMVECTOR upperLeft = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->plane_param.ul));

			// Center Position
			XMVECTOR pos = XMVectorAdd(XMVectorAdd(lowerLeft, lowerRight), XMVectorAdd(upperLeft, upperRight));
			pos = XMVectorMultiply(pos, XMLoadFloat4(&QUARTER));

			XMVECTOR scaledRight = XMVectorSubtract(upperRight, upperLeft);
			XMVECTOR scaledFront = XMVectorSubtract(lowerLeft, upperLeft);
			XMVECTOR normal = XMVector3Normalize( XMVector3Cross(scaledFront, scaledRight) );

			if (XMVectorGetX(XMVector3Dot(normal, lookAt)) > 0.0f) {
				// Flip Plane
				normal = XMVectorNegate(normal);
				scaledFront = XMVectorNegate(scaledFront);
			}

			scaledRight = XMVectorSetW(scaledRight, 0.0f);
			scaledFront = XMVectorSetW(scaledFront, 0.0f);
			normal = XMVectorSetW(normal, 0.0f);
			pos = XMVectorSetW(pos, 1.0f);

			XMMATRIX mat = { scaledRight, normal, scaledFront, pos };
			mat = XMMatrixMultiply(mat, baseModel);
			XMStoreFloat4x4(&out.model, XMMatrixTranspose(mat));

			XMVECTOR detV = XMVector3TransformNormal(normal, baseModel);
			detV = XMVector3Dot(detV, XMLoadFloat4(&Y_DIR));

			float det = XMVectorGetX(detV);
			float absDet = abs(det);

			if (absDet < 0.15f) // horizontal normal -> Vertical Wall (less than about 9 degree)
			{
				// Wall
				out.color = XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f);
			}
			else if (absDet > 0.99f) // vertical normal -> Floor or Ceil 
			{
				// det > 0 ? Floor : Ceil
				out.color = det > 0 ? XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) : XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
			}
			else // Slope
			{
				// det > 0 ? FloorSlope : CeilSlope
				out.color = det > 0 ? XMFLOAT4(0.8f, 0.75f, 1.0f, 1.0f) : XMFLOAT4(1.0f, 0.75f, 0.8f, 1.0f);
			}

			out.modelIndex = MODEL_INDEX_PLANE;
			out.modelType = MODEL_TYPE_GENERAL;
			out.param1 = 0.0f;
			out.param2 = 0.0f;
		}
		return;

		case FS_TYPE_SPHERE:
		{
			XMVECTOR front = XMVectorNegate(lookAt);
			XMVECTOR right = XMVector3Normalize(XMVector3Cross(upDir, front));
			XMVECTOR pos = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->sphere_param.c));

			XMVECTOR r = XMVectorSet(pResult->sphere_param.r, pResult->sphere_param.r, pResult->sphere_param.r, 0.0f);
			front = XMVectorMultiply(r, front);
			right = XMVectorMultiply(r, right);

			XMVECTOR normal = XMVectorMultiply(r, upDir);

			XMMATRIX mat = {
				XMVectorSetW(right, 0.0f),
				XMVectorSetW(normal, 0.0f),
				XMVectorSetW(front, 0.0f),
				XMVectorSetW(pos, 1.0f)
			};
			mat = XMMatrixMultiply(mat, baseModel);

			XMStoreFloat4x4(&out.model, XMMatrixTranspose(mat));
			out.modelIndex = MODEL_INDEX_SPHERE;
			out.modelType = MODEL_TYPE_GENERAL;
			out.param1 = 0.0f;
			out.param2 = 0.0f;
			out.color = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
		}
		return;

		case FS_TYPE_CYLINDER:
		{
			XMVECTOR tmp1 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->cylinder_param.t));
			XMVECTOR tmp2 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->cylinder_param.b));

			XMVECTOR pos = XMVectorAdd(tmp1, tmp2);
			pos = XMVectorMultiply(pos, XMLoadFloat4(&HALF));

			XMVECTOR scaledAxis = XMVectorSubtract(tmp1, tmp2);
			XMVECTOR axis = XMVector3Normalize(scaledAxis);

			tmp1 = XMVector3Normalize( XMVector3Cross(lookAt, axis) ); // Right
			tmp2 = XMVector3Normalize(XMVector3Cross(tmp1, axis)); // Front

			tmp1 = XMVectorScale(tmp1, pResult->cylinder_param.r);
			tmp2 = XMVectorScale(tmp2, pResult->cylinder_param.r);

			XMMATRIX mat = {
				XMVectorSetW(tmp1, 0.0f),
				XMVectorSetW(scaledAxis, 0.0f),
				XMVectorSetW(tmp2, 0.0f),
				XMVectorSetW(pos, 1.0f)
			};
			mat = XMMatrixMultiply(mat, baseModel);

			XMStoreFloat4x4(&out.model, XMMatrixTranspose(mat));
			out.modelIndex = MODEL_INDEX_CYLINDER;
			out.modelType = MODEL_TYPE_GENERAL;
			out.param1 = 0.0f;
			out.param2 = 0.0f;
			out.color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
		}
		return;

		case FS_TYPE_CONE:
		{
			XMVECTOR tmp1 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->cone_param.t));
			XMVECTOR tmp2 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->cone_param.b));

			XMVECTOR pos = XMVectorAdd(tmp1, tmp2);
			pos = XMVectorMultiply(pos, XMLoadFloat4(&HALF));

			XMVECTOR scaledAxis = XMVectorSubtract(tmp1, tmp2);
			XMVECTOR axis = XMVector3Normalize(scaledAxis);

			tmp1 = XMVector3Normalize(XMVector3Cross(lookAt, axis)); // Right
			tmp2 = XMVector3Normalize(XMVector3Cross(tmp1, axis)); // Front

			tmp1 = XMVectorScale(tmp1, pResult->cylinder_param.r);
			tmp2 = XMVectorScale(tmp2, pResult->cylinder_param.r);

			XMMATRIX mat = {
				XMVectorSetW(tmp1, 0.0f),
				XMVectorSetW(scaledAxis, 0.0f),
				XMVectorSetW(tmp2, 0.0f),
				XMVectorSetW(pos, 1.0f)
			};
			mat = XMMatrixMultiply(mat, baseModel);

			XMStoreFloat4x4(&out.model, XMMatrixTranspose(mat));
			out.modelIndex = MODEL_INDEX_CONE;
			out.modelType = MODEL_TYPE_CONE_TRANSFORM;
			out.param1 = pResult->cone_param.tr / pResult->cone_param.br;
			out.param2 = 0.0f;
			out.color = XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f);
		}
		return;

		case FS_TYPE_TORUS:
		{
			XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->torus_param.n)));
			XMVECTOR pos = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(pResult->torus_param.c));

			XMVECTOR right;
			float det = XMVectorGetX(XMVector3Dot(normal, lookAt));
			if (abs(det) < 0.15f)
			{
				XMVECTOR tmp = det > 0.0f ? XMVector3Cross(normal, upDir) : XMVector3Cross(upDir, normal);
				right = XMVector3Normalize(tmp);
			}
			else 
			{
				right = XMVector3Normalize(XMVector3Cross(lookAt, normal));
			}
			XMVECTOR front = XMVector3Normalize(XMVector3Cross(right, normal));

			XMMATRIX mat = {
				XMVectorSetW(right, 0.0f),
				XMVectorSetW(normal, 0.0f),
				XMVectorSetW(front, 0.0f),
				XMVectorSetW(pos, 1.0f)
			};
			mat = XMMatrixMultiply(mat, baseModel);

			XMStoreFloat4x4(&out.model, XMMatrixTranspose(mat));
			out.modelIndex = MODEL_INDEX_TORUS;
			out.modelType = MODEL_TYPE_TORUS_TRANSFORM;
			out.param1 = pResult->torus_param.mr;
			out.param2 = pResult->torus_param.tr;
			out.color = XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f);
		}
		return;
	}

}