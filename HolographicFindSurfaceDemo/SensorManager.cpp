#include "pch.h"
#include "SensorManager.h"

#include <sstream>

using namespace HolographicFindSurfaceDemo;

using namespace winrt::Windows::Perception::Spatial;
using namespace winrt::Windows::Perception::Spatial::Preview;

extern "C" HMODULE LoadLibraryA( LPCSTR lpLibFileName );

static ResearchModeSensorConsent camAccessCheck;
static HANDLE camConsentGiven = nullptr;

static void _camAccessOnComplete(ResearchModeSensorConsent consent)
{
	camAccessCheck = consent;
	SetEvent(camConsentGiven);
}

// Thread
void SensorManager::SensorLoopThread(SensorManager* pOwner)
{
	if (camConsentGiven)
	{
		HRESULT hr = S_OK;
		DWORD waitResult = WaitForSingleObject(camConsentGiven, INFINITE);

		if (waitResult == WAIT_OBJECT_0)
		{
			switch (camAccessCheck)
			{
			case ResearchModeSensorConsent::Allowed:
				OutputDebugString(L"Access is granted\n");
				break;
			case ResearchModeSensorConsent::DeniedBySystem:
				OutputDebugString(L"Access is denied by the system\n");
				hr = E_ACCESSDENIED;
				break;
			case ResearchModeSensorConsent::DeniedByUser:
				OutputDebugString(L"Access is denied by the user\n");
				hr = E_ACCESSDENIED;
				break;
			case ResearchModeSensorConsent::NotDeclaredByApp:
				OutputDebugString(L"Capability is not declared in the app manifest\n");
				hr = E_ACCESSDENIED;
				break;
			case ResearchModeSensorConsent::UserPromptRequired:
				OutputDebugString(L"Capability user prompt required\n");
				hr = E_ACCESSDENIED;
				break;
			default:
				OutputDebugString(L"Access is denied by the system\n");
				hr = E_ACCESSDENIED;
				break;
			}
		}
		else
		{
			hr = E_UNEXPECTED;
		}

		if (FAILED(hr))
		{
			OutputDebugString(L"Failed to Run Thread\n");
			return;
		}
	}

	// Sensor Check
	if (pOwner->m_pSensor)
	{
		// Open Stream
		winrt::check_hresult(pOwner->m_pSensor->OpenStream());

		while (!pOwner->m_fExit)
		{
			IResearchModeSensorFrame* pSensorFrame = nullptr;

			pOwner->m_pSensor->GetNextBuffer(&pSensorFrame);

			if (pSensorFrame) {
				pOwner->onProcessFrame(pSensorFrame);
				pSensorFrame->Release();
			}
		}

		if (pOwner->m_pSensor) {
			pOwner->m_pSensor->CloseStream();
		}
	}
}

void SensorManager::initializeSensor()
{
	camConsentGiven = CreateEvent(nullptr, true, false, nullptr);

	HMODULE hrResearchMode = LoadLibraryA("ResearchModeAPI");
	if (hrResearchMode)
	{
		typedef HRESULT(__cdecl* PFN_CREATEPROVIDER) (IResearchModeSensorDevice** ppSensorDevice);
		PFN_CREATEPROVIDER pfnCreate = reinterpret_cast<PFN_CREATEPROVIDER>(GetProcAddress(hrResearchMode, "CreateResearchModeSensorDevice"));
		if (pfnCreate)
		{
			winrt::check_hresult(pfnCreate(&m_pSensorDevice));
		}
		else
		{
			winrt::check_hresult(E_INVALIDARG);
		}
	}

	winrt::check_hresult(m_pSensorDevice->QueryInterface(IID_PPV_ARGS(&m_pSensorDeviceConsent)));
	winrt::check_hresult(m_pSensorDeviceConsent->RequestCamAccessAsync(_camAccessOnComplete));

	// This call makes cameras run at full frame rate. Normaly they are optimized 
	// for headtracker use. For some applications that may be sufficient 
	m_pSensorDevice->DisableEyeSelection();

	// Get Depth Sensor
	winrt::check_hresult(m_pSensorDevice->GetSensor(DEPTH_LONG_THROW, &m_pSensor));
	winrt::check_hresult(m_pSensor->QueryInterface(IID_PPV_ARGS(&m_pCameraSensor)));

	// Get Extrinsic
	DirectX::XMMATRIX rigPoseToCameraNode;
	DirectX::XMMATRIX cameraNodeToRigPose;
	DirectX::XMVECTOR det;

	m_pCameraSensor->GetCameraExtrinsicsMatrix(&m_matExtrinsic);

	rigPoseToCameraNode = DirectX::XMLoadFloat4x4(&m_matExtrinsic);
	det = DirectX::XMMatrixDeterminant(rigPoseToCameraNode);
	cameraNodeToRigPose = DirectX::XMMatrixInverse(&det, rigPoseToCameraNode);

	DirectX::XMStoreFloat4x4(&m_matInvExtrinsic, cameraNodeToRigPose);

	// Spatial Locator
	IResearchModeSensorDevicePerception* pSensorDevicePerception = nullptr;
	GUID guid;

	winrt::check_hresult(m_pSensorDevice->QueryInterface(IID_PPV_ARGS(&pSensorDevicePerception)));
	winrt::check_hresult(pSensorDevicePerception->GetRigNodeId(&guid));
	m_refSpatialLocator = SpatialGraphInteropPreview::CreateLocatorForNode(guid);
	pSensorDevicePerception->Release();
}

void SensorManager::startSensor()
{
	// Check Thread is already working
	if (m_pSensorThread)
	{
		if (m_pSensorThread->joinable()) { return; }
		// Else Cleanup previous thread context
		delete m_pSensorThread;
		m_pSensorThread = nullptr;
	}
	m_fExit = false;
	m_pSensorThread = new std::thread(SensorLoopThread, this);
}

void SensorManager::stopSensor()
{
	if (m_pSensorThread)
	{
		if (m_pSensorThread->joinable()) {
			m_fExit = true;
			m_pSensorThread->join();
		}
		m_fDataUpdated = false;
		// Cleanup thread context
		delete m_pSensorThread;
		m_pSensorThread = nullptr;
	}
}

bool SensorManager::getUpdatedData(_Out_ std::vector<DirectX::XMFLOAT3>& buffer, _Out_ long long& timestamp)
{
	std::lock_guard lock(m_hDataMutex);
	if (!m_fDataUpdated) { return false; }

	// Copy Point Cloud
	buffer.clear();
	buffer.assign(m_vecPointCloud.begin(), m_vecPointCloud.end());

	// Copy Timestamp
	timestamp = m_nPrevTimestamp;
	m_fDataUpdated = false;

	return true;
}

void SensorManager::onProcessFrame(IResearchModeSensorFrame* pSensorFrame)
{
	ResearchModeSensorResolution resolution;
	ResearchModeSensorTimestamp  timestamp;

	IResearchModeSensorDepthFrame* pDepthFrame = nullptr;
	const UINT16* pAbImage = nullptr;
	const UINT16* pDepth = nullptr;

	// sigma buffer needed only for Long Throw
	const BYTE* pSigma = nullptr;

	// invalidation mask for Long Throw
	constexpr USHORT mask = 0x80;

	// assert( m_pSensor->GetSensorType() == DEPTH_LONG_THROW );

	HRESULT hr = S_OK;
	size_t outBufferCount;

	pSensorFrame->GetResolution(&resolution);
	pSensorFrame->GetTimeStamp(&timestamp);

	hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pDepthFrame));
	if (SUCCEEDED(hr))
	{
		// extract sigma buffer for Long Throw
		hr = pDepthFrame->GetSigmaBuffer(&pSigma, &outBufferCount);

		// extract depth buffer
		hr = pDepthFrame->GetBuffer(&pDepth, &outBufferCount);

		// outBufferCount == (resolution.Width * resolution.Height)
		if (resolution.Width != m_nPrevFrameRes.x || resolution.Height != m_nPrevFrameRes.y)
		{
			m_nPrevFrameRes.x = resolution.Width;
			m_nPrevFrameRes.y = resolution.Height;

#ifdef _DEBUG
			{
				std::wostringstream wss;
				wss << "Res: " << resolution.Width << ", " << resolution.Height << std::endl;
				wss << "BufCnt: " << outBufferCount << std::endl;

				OutputDebugString(wss.str().c_str());
			}
#endif
			m_vecUnitXYPlane.clear();
			m_vecUnitXYPlane.reserve(resolution.Width * resolution.Height);
			
			for (UINT v = 0; v < resolution.Height; v++)
			{
				for (UINT u = 0; u < resolution.Width; u++)
				{
					float uv[2] = { static_cast<float>(u) + 0.5f, static_cast<float>(v) + 0.5f };
					float xy[2] = { 0, 0 };
					m_pCameraSensor->MapImagePointToCameraUnitPlane(uv, xy);

					float depthAdjust = sqrt(1.0f + xy[0] * xy[0] + xy[1] * xy[1]); // sqrt( x*x + y*y + z*z ) where z = 1

					m_vecUnitXYPlane.emplace_back(DirectX::XMFLOAT4(xy[0], xy[1], depthAdjust, 1.0f / depthAdjust));
				}
			}
		}

		std::vector< DirectX::XMFLOAT3 > pointBuffer;
		pointBuffer.reserve(resolution.Width * resolution.Height);

		for (UINT v = 0; v < resolution.Height; v++) 
		{
			for (UINT u = 0; u < resolution.Width; u++)
			{
				UINT index = resolution.Width * v + u;
				UINT16 depthValue = (pSigma && ((pSigma[index] & mask) > 0)) ? 0 : pDepth[index];
				if (depthValue > 0) {
					constexpr float mm2m = 0.001f;

					float z = static_cast<float>(depthValue) * m_vecUnitXYPlane[index].w * mm2m;
					// or static_cast<float>(depthValue) / m_vecUnitXYPlane[index].z;

					// Do not need Flip YZ in here
					pointBuffer.emplace_back(DirectX::XMFLOAT3( m_vecUnitXYPlane[index].x * z, (m_vecUnitXYPlane[index].y * z), z ));
				}
			}
		}


		// Update Here!!
		{
			std::lock_guard lock(m_hDataMutex);
			m_vecPointCloud.swap(pointBuffer);
			m_fDataUpdated = true;
			// assert(timestamp.HostTicks <= LLONG_MAX);
			m_nPrevTimestamp = static_cast<long long>(timestamp.HostTicks);
		}
	}

	if (pDepthFrame) {
		pDepthFrame->Release();
	}

}