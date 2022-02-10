#pragma once

#ifndef _SENSOR_MANAGER_H_
#define _SENSOR_MANAGER_H_

#include "ResearchMode/ResearchModeApi.h"

#include <chrono>
typedef std::chrono::duration<int64_t, std::ratio<1, 10'000'000>> HundredsOfNanoseconds;

namespace HolographicFindSurfaceDemo
{
	class SensorManager
	{
	private: // Member Variable

		// Research Mode API related...
		IResearchModeSensorDevice* m_pSensorDevice = nullptr;
		IResearchModeSensorDeviceConsent* m_pSensorDeviceConsent = nullptr; // Privilige
		IResearchModeSensor* m_pSensor = nullptr;
		IResearchModeCameraSensor* m_pCameraSensor = nullptr;

		winrt::Windows::Perception::Spatial::SpatialLocator m_refSpatialLocator = nullptr;

		// Worker Thread
		std::thread* m_pSensorThread = nullptr;
		bool m_fExit = false; // Thread Exit Flag

		// PointCloud Data
		std::mutex m_hDataMutex;
		std::vector< DirectX::XMFLOAT3 > m_vecPointCloud; // Point Cloud Buffer
		DirectX::XMUINT2 m_nPrevFrameRes;
		long long m_nPrevTimestamp = 0;
		

		bool m_fDataUpdated = false; // Data Updated Flag

		// lazy constant
		DirectX::XMFLOAT4X4 m_matExtrinsic;    // Extrinsic Matrix (CameraNode to RigPose)
		DirectX::XMFLOAT4X4 m_matInvExtrinsic; // Inverse Extrnisic Matrix	(CameraNode to RigPose Inverted)
		
		std::vector< DirectX::XMFLOAT4 > m_vecUnitXYPlane; // Pre-calculated Unit XY Plane (with Intrinsic Parameter)

	public:
		void initializeSensor();

	public:
		void startSensor();
		void stopSensor();

#ifdef _DEBUG
	public:
		inline void printThreadDebug() const {
			if (m_pSensorThread) {
				OutputDebugString(m_pSensorThread->joinable() ? L"Thread is joinable\n" : L"Thread is dead\n");
			}
			else {
				OutputDebugString(L"No Sensor Thread");
			}
		}
#endif

	public: // Getter
		inline winrt::Windows::Perception::Spatial::SpatialLocator spatialLocator() const { return m_refSpatialLocator; }
		bool getUpdatedData(_Out_ std::vector<DirectX::XMFLOAT3>& buffer, _Out_ long long& timestamp);

		const inline const DirectX::XMFLOAT4X4* getExtrinsicPtr() const { return &m_matExtrinsic; }
		const inline const DirectX::XMFLOAT4X4* getInvExtrinsicPtr() const { return &m_matInvExtrinsic; }

		const inline const DirectX::XMFLOAT4X4* getCameraNodeToRigNode() const { return getInvExtrinsicPtr(); }

	private:
		void onProcessFrame(IResearchModeSensorFrame* pSensorFrame);

	private: // Thread Function
		static void SensorLoopThread(SensorManager* pOwner);
	};
};

#endif
