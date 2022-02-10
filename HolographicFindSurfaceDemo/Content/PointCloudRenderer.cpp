#include "pch.h"
#include "PointCloudRenderer.h"
#include "Common/DirectXHelper.h"

using namespace HolographicFindSurfaceDemo;
using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

// Loads vertex and pixel shaders from files.
PointCloudRenderer::PointCloudRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
    m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

void PointCloudRenderer::UpdatePointCloudBuffer(const DirectX::XMFLOAT3* pBuffer, size_t count, DirectX::XMMATRIX model, bool isTransposed)
{
    if (!m_loadingComplete || pBuffer == nullptr || count < 1 || count > static_cast<size_t>(MAX_POINT_CLOUD_COUNT)) { return; }
    if (!isTransposed) { model = XMMatrixTranspose(model); }

    const auto context = m_deviceResources->GetD3DDeviceContext();

    // Update the model transform buffer for the hologram.
    XMFLOAT4X4 modelConstant;
    XMStoreFloat4x4(&modelConstant, model);

    context->UpdateSubresource(
        m_modelConstantBuffer.Get(),
        0,
        nullptr,
        &modelConstant,
        0,
        0
    );

    D3D11_BOX dstBox;
    dstBox.left = 0;
    dstBox.right = static_cast<UINT>(sizeof(XMFLOAT3) * count);
    dstBox.top = 0;
    dstBox.bottom = 1;
    dstBox.front = 0;
    dstBox.back = 1;

    context->UpdateSubresource(
        m_pointCloudBuffer.Get(),
        0,
        &dstBox,
        pBuffer,
        0,
        0
    );

    m_vertexCount = static_cast<uint32_t>(count);
}

// Renders one frame using the vertex and pixel shaders.
// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
// a pass-through geometry shader is also used to set the render 
// target array index.
void PointCloudRenderer::Render()
{
    // Loading is asynchronous. Resources must be created before drawing can occur.
    if (!m_loadingComplete || m_vertexCount < 1)
    {
        return;
    }

    const auto context = m_deviceResources->GetD3DDeviceContext();

    // Each vertex is one instance of the XMFLOAT3 struct.
    const UINT stride = sizeof(XMFLOAT3);
    const UINT offset = 0;
    context->IASetVertexBuffers(
        0,
        1,
        m_pointCloudBuffer.GetAddressOf(),
        &stride,
        &offset
    );
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    context->IASetInputLayout(m_inputLayout.Get());

    // Attach the vertex shader.
    context->VSSetShader(
        m_vertexShader.Get(),
        nullptr,
        0
    );
    // Apply the model constant buffer to the vertex shader.
    context->VSSetConstantBuffers(
        0,
        1,
        m_modelConstantBuffer.GetAddressOf()
    );

    if (!m_usingVprtShaders)
    {
        // On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
        // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
        // a pass-through geometry shader is used to set the render target 
        // array index.
        context->GSSetShader(
            m_geometryShader.Get(),
            nullptr,
            0
        );
    }

    // Attach the pixel shader.
    context->PSSetShader(
        m_pixelShader.Get(),
        nullptr,
        0
    );

    // Draw the objects.
    context->DrawInstanced(
        m_vertexCount, // Vertex count per instance.
        2,             // Instance count.
        0,             // Start vertex location.
        0              // Start instance location.
    );
}

std::future<void> PointCloudRenderer::CreateDeviceDependentResources()
{
    m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

    // On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
    // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
    // we can avoid using a pass-through geometry shader to set the render
    // target array index, thus avoiding any overhead that would be 
    // incurred by setting the geometry shader stage.
    std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///PCRVprtVertexShader.cso" : L"ms-appx:///PCRVertexShader.cso";

    // Shaders will be loaded asynchronously.

    // After the vertex shader file is loaded, create the shader and input layout.
    std::vector<byte> vertexShaderFileData = co_await DX::ReadDataAsync(vertexShaderFileName);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateVertexShader(
            vertexShaderFileData.data(),
            vertexShaderFileData.size(),
            nullptr,
            &m_vertexShader
        ));

    constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 1> vertexDesc =
    { {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    } };

    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateInputLayout(
            vertexDesc.data(),
            static_cast<UINT>(vertexDesc.size()),
            vertexShaderFileData.data(),
            static_cast<UINT>(vertexShaderFileData.size()),
            &m_inputLayout
        ));

    // After the pixel shader file is loaded, create the shader and constant buffer.
    std::vector<byte> pixelShaderFileData = co_await DX::ReadDataAsync(L"ms-appx:///PCRPixelShader.cso");
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreatePixelShader(
            pixelShaderFileData.data(),
            pixelShaderFileData.size(),
            nullptr,
            &m_pixelShader
        ));

    const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(XMFLOAT4X4), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &constantBufferDesc,
            nullptr,
            &m_modelConstantBuffer
        ));

    if (!m_usingVprtShaders)
    {
        // Load the pass-through geometry shader.
        std::vector<byte> geometryShaderFileData = co_await DX::ReadDataAsync(L"ms-appx:///PCRGeometryShader.cso");

        // After the pass-through geometry shader file is loaded, create the shader.
        winrt::check_hresult(
            m_deviceResources->GetD3DDevice()->CreateGeometryShader(
                geometryShaderFileData.data(),
                geometryShaderFileData.size(),
                nullptr,
                &m_geometryShader
            ));
    }

    // Create Empty Point Cloud Buffer (max 1 mio points)
    const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(XMFLOAT3) * static_cast<UINT>(MAX_POINT_CLOUD_COUNT), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &vertexBufferDesc,
            nullptr,
            &m_pointCloudBuffer
        ));

    // the object is ready to be rendered.
    m_loadingComplete = true;
};

void PointCloudRenderer::ReleaseDeviceDependentResources()
{
    m_loadingComplete = false;
    m_usingVprtShaders = false;
    m_vertexShader.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_geometryShader.Reset();
    m_modelConstantBuffer.Reset();
    m_pointCloudBuffer.Reset();
}
