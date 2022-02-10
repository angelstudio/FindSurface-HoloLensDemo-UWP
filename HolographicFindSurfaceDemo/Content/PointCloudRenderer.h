#pragma once

#include "../Common/DeviceResources.h"
#include "ShaderStructures.h"

#define MAX_POINT_CLOUD_COUNT 1000000 // 1mio

namespace HolographicFindSurfaceDemo
{
    // This sample renderer instantiates a basic rendering pipeline.
    class PointCloudRenderer
    {
    public:
        PointCloudRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources);
        std::future<void> CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Render();

    public:
        void UpdatePointCloudBuffer(const DirectX::XMFLOAT3* pBuffer, size_t count, DirectX::XMMATRIX model, bool isTransposed = false);
        void ClearPointCloudBuffer() { m_vertexCount = 0; }

    private:
        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>            m_deviceResources;

        // Direct3D resources for cube geometry.
        Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11GeometryShader>    m_geometryShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_modelConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_pointCloudBuffer;

        // System resources for point cloud geometry.
        uint32_t                                        m_vertexCount = 0;

        // Variables used with the rendering loop.
        bool                                            m_loadingComplete = false;
        

        // If the current D3D Device supports VPRT, we can avoid using a geometry
        // shader just to set the render target array index.
        bool                                            m_usingVprtShaders = false;
    };
}
