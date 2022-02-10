#pragma once

#include "../Common/DeviceResources.h"
#include "ShaderStructures.h"

#define MAX_INSTANCE_COUNT 30

// Note; ModelTypeIndex(int) -> 0: Plane, 1: Sphere, 2: Cylinder, 3: Cone, 4: Torus
#define MODEL_INDEX_PLANE 0
#define MODEL_INDEX_SPHERE 1
#define MODEL_INDEX_CYLINDER 2
#define MODEL_INDEX_CONE 3
#define MODEL_INDEX_TORUS 4

#define MODEL_TYPE_GENERAL 0
#define MODEL_TYPE_CONE_TRANSFORM 1
#define MODEL_TYPE_TORUS_TRANSFORM 2

namespace HolographicFindSurfaceDemo
{
	class MeshRenderer
	{
    public:
        MeshRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources);
        std::future<void> CreateDeviceDependentResources();
        void ReleaseDeviceDependentResources();
        void Render();

    public:
        void SetCurrentModel(const InstanceConstantBuffer& buffer);
        void ClearCurrentModel() { m_modelTypeIndices[0] = -1; }

        void StoreCurrent(); // Add Current Model ConstantBuffer to Buffer.
        void RemoveLast();   // Remove Last Model ConstantBuffer.
        void RemoveAll();    // Remove All Model ConstantBuffer in Buffer.

    private:
        // Cached pointer to device resources.
        std::shared_ptr<DX::DeviceResources>            m_deviceResources;

        // Direct3D resources
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_rasterizeState;
        Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            m_indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11VertexShader>      m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11GeometryShader>    m_geometryShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       m_pixelShader;
        // Note: index 0 is for live Model, stored model index starts from 1
        std::array< Microsoft::WRL::ComPtr<ID3D11Buffer>, MAX_INSTANCE_COUNT + 1 > m_modelConstantBuffers;

        // System resources
        std::vector<uint32_t>                           m_baseVertexList;
        std::vector<uint32_t>                           m_baseIndexList;
        std::vector<uint32_t>                           m_indexCountList;

        // Variables used with the rendering loop.
        bool                                            m_loadingComplete = false;

        uint32_t                                        m_modelCount = 0;
        std::array< int, MAX_INSTANCE_COUNT + 1>        m_modelTypeIndices;
        

        // If the current D3D Device supports VPRT, we can avoid using a geometry
        // shader just to set the render target array index.
        bool                                            m_usingVprtShaders = false;
	};
};
