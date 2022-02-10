#pragma once

#include "../Common/DeviceResources.h"
#include "../Common/StepTimer.h"
#include "ShaderStructures.h"

#define MIN_SEED_RADIUS_AT_METER 0.005f // 0.5cm
#define MAX_SEED_RADIUS_AT_METER 0.6f   // 0.6m
#define CUBE_SIZE_AT_METER       0.01f  // 1cm
#define CIRCLE_SEGMENT           24

#define ROTATE_FAST_SPEED        720.f
#define ROTATE_NORMAL_SPEED      45.f

#define CIRCLE_INDEX_NORMAL 0
#define CIRCLE_INDEX_HIGH   1
#define CIRCLE_INDEX_LOW    2

namespace HolographicFindSurfaceDemo
{
    class GazePointRenderer
    {
    public:
        GazePointRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources);
        std::future<void> CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Update(DX::StepTimer const& timer);
        void Render();

        // Repositions the sample hologram.
        void PositionGazePointUI(
            winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose, 
            const winrt::Windows::Foundation::Numerics::float3 const& gazePoint
        );

        // Property accessors.
        winrt::Windows::Foundation::Numerics::float3 const& GetGazePoint() { return m_position; }
        float GetHeadForwardDistanceToGazePoint() { return m_distance; }
        float GetSeedRadiusAtGazePoint() { return m_seedRadiusAtMeter * m_distance; }

        void SetSeedRadiusAtMeter(float baseRadius) { 
            if (baseRadius < MIN_SEED_RADIUS_AT_METER || baseRadius > MAX_SEED_RADIUS_AT_METER) { return; }
            m_seedRadiusAtMeter = baseRadius; 
        }
        float GetSeedRadiusAtMeter() { return m_seedRadiusAtMeter; }
        void AdjustSeedRadiusAtMeter(float delta) {
            // m_seedRadiusAtMeter = clamp( m_seedRadiusAtMeter + delta, MIN_SEED_RADIUS_AT_METER, MAX_SSED_RADIUS_AT_METER );
            float val = m_seedRadiusAtMeter + delta;
            if (val < MIN_SEED_RADIUS_AT_METER) { val = MIN_SEED_RADIUS_AT_METER; }
            else if (val > MAX_SEED_RADIUS_AT_METER) { val = MAX_SEED_RADIUS_AT_METER; }
            m_seedRadiusAtMeter = val;
        }

        void SetRotateSpeed(float degreesPerSec) { m_degreesPerSecond = degreesPerSec;  }
        void SetCircleIndex(uint32_t index) { m_circleVertexIndex = index; }

    private:
        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>            m_deviceResources;

        // Direct3D resources for cube geometry.
        Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_cubeVertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_cubeIndexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_circleVertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11GeometryShader>    m_geometryShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_cubeModelConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_circleModelConstantBuffer;

        // System resources for cube geometry.
        ModelConstantBuffer                             m_cubeModelConstantBufferData;
        uint32_t                                        m_cubeIndexCount = 0;
        // System resources for circle geometry.
        ModelConstantBuffer                             m_circleModelConstantBufferData;
        uint32_t                                        m_circleVertexCount = 0;
        uint32_t                                        m_circleVertexIndex = 0;
        float                                           m_seedRadiusAtMeter = 0.1f;
        // 
        float                                           m_distance = 1.0f; // Distance to Gaze Point From Head Position on Head Forward Direction.

        // Variables used with the rendering loop.
        bool                                            m_loadingComplete = false;
        float                                           m_degreesPerSecond = 45.f;

        winrt::Windows::Foundation::Numerics::float3    m_position = { 0.f, 0.f, -2.f };
        winrt::Windows::Foundation::Numerics::float3    m_headOrientation[3] = { { 1, 0, 0 }, {0, 1, 0}, {0, 0, 1} };

        // If the current D3D Device supports VPRT, we can avoid using a geometry
        // shader just to set the render target array index.
        bool                                            m_usingVprtShaders = false;
    };
}
