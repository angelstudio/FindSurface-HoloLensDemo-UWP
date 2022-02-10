#include "pch.h"
#include "GazePointRenderer.h"
#include "Common/DirectXHelper.h"

using namespace HolographicFindSurfaceDemo;
using namespace DirectX;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI::Input::Spatial;

// Loads vertex and pixel shaders from files and instantiates the cube geometry.
GazePointRenderer::GazePointRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
    m_deviceResources(deviceResources)
{
    CreateDeviceDependentResources();
}

void GazePointRenderer::PositionGazePointUI( SpatialPointerPose const& pointerPose, const float3 const& gazePoint )
{
    if (pointerPose != nullptr)
    {
        const float3 headPosition = pointerPose.Head().Position();
        const float3 headZAxis = -pointerPose.Head().ForwardDirection();
        const float3 headYAxis = pointerPose.Head().UpDirection();

        float distance = abs( dot(headPosition - gazePoint, headZAxis) ); // abs for safety
        float3 headXAxis = normalize(cross(headYAxis, headZAxis));

        // Update
        m_position = gazePoint;
        m_headOrientation[0] = headXAxis;
        m_headOrientation[1] = headYAxis;
        m_headOrientation[2] = headZAxis;
        m_distance = distance;
    }
}

// Called once per frame. Rotates the cube, and calculates and sets the model matrix
// relative to the position transform indicated by hologramPositionTransform.
void GazePointRenderer::Update(DX::StepTimer const& timer)
{
    // Rotate the cube.
    // Convert degrees to radians, then convert seconds to rotation angle.
    const float    cubeScale = m_distance * CUBE_SIZE_AT_METER;
    const float    circleScale = m_distance * m_seedRadiusAtMeter;

    const float    radiansPerSecond = XMConvertToRadians(m_degreesPerSecond);
    const double   totalRotation = timer.GetTotalSeconds() * radiansPerSecond;
    const float    radians = static_cast<float>(fmod(totalRotation, XM_2PI));

    const XMMATRIX translation = XMMatrixTranslationFromVector(XMLoadFloat3(&m_position));

    XMMATRIX scale;
    XMMATRIX rotate;
    
    XMMATRIX modelTransform;

    // Cube
    scale = XMMatrixScaling(cubeScale, cubeScale, cubeScale);
    rotate = XMMatrixRotationY(-radians);
    modelTransform = XMMatrixMultiply(scale, rotate);
    modelTransform = XMMatrixMultiply(modelTransform, translation);

    XMStoreFloat4x4(&m_cubeModelConstantBufferData.model, XMMatrixTranspose(modelTransform));

    // Circle
    scale = XMMatrixScaling(circleScale, circleScale, 1.0f);
    rotate = XMMatrixIdentity();
    rotate.r[0] = XMLoadFloat3(&(m_headOrientation[0]));
    rotate.r[1] = XMLoadFloat3(&(m_headOrientation[1]));
    rotate.r[2] = XMLoadFloat3(&(m_headOrientation[2]));
    modelTransform = XMMatrixMultiply(scale, rotate);
    modelTransform = XMMatrixMultiply(modelTransform, translation);

    XMStoreFloat4x4(&m_circleModelConstantBufferData.model, XMMatrixTranspose(modelTransform));

    // Loading is asynchronous. Resources must be created before they can be updated.
    if (!m_loadingComplete)
    {
        return;
    }

    // Use the D3D device context to update Direct3D device-based resources.
    const auto context = m_deviceResources->GetD3DDeviceContext();

    // Update the model transform buffer for the hologram.
    context->UpdateSubresource(
        m_cubeModelConstantBuffer.Get(),
        0,
        nullptr,
        &m_cubeModelConstantBufferData,
        0,
        0
    );

    context->UpdateSubresource(
        m_circleModelConstantBuffer.Get(),
        0,
        nullptr,
        &m_circleModelConstantBufferData,
        0,
        0
    );
}

// Renders one frame using the vertex and pixel shaders.
// On devices that do not support the D3D11_FEATURE_D3D11_OPTIONS3::
// VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature,
// a pass-through geometry shader is also used to set the render 
// target array index.
void GazePointRenderer::Render()
{
    // Loading is asynchronous. Resources must be created before drawing can occur.
    if (!m_loadingComplete)
    {
        return;
    }

    const auto context = m_deviceResources->GetD3DDeviceContext();

    context->IASetInputLayout(m_inputLayout.Get());

    // Attach the vertex shader.
    context->VSSetShader(
        m_vertexShader.Get(),
        nullptr,
        0
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

    // Each vertex is one instance of the VertexPositionColor struct.
    const UINT stride = sizeof(VertexPositionColor);
    const UINT offset = 0;

    // Draw a cube at gaze point first
    {
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetVertexBuffers(
            0,
            1,
            m_cubeVertexBuffer.GetAddressOf(),
            &stride,
            &offset
        );
        context->IASetIndexBuffer(
            m_cubeIndexBuffer.Get(),
            DXGI_FORMAT_R16_UINT, // Each index is one 16-bit unsigned integer (short).
            0
        );
        // Apply the model constant buffer to the vertex shader.
        context->VSSetConstantBuffers(
            0,
            1,
            m_cubeModelConstantBuffer.GetAddressOf()
        );
        // Draw the objects.
        context->DrawIndexedInstanced(
            m_cubeIndexCount, // Index count per instance.
            2,                // Instance count.
            0,                // Start index location.
            0,                // Base vertex location.
            0                 // Start instance location.
        );
    }

    // Draw a seed radius circle
    {
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
        context->IASetVertexBuffers(
            0,
            1,
            m_circleVertexBuffer.GetAddressOf(),
            &stride,
            &offset
        );
        // Apply the model constant buffer to the vertex shader.
        context->VSSetConstantBuffers(
            0,
            1,
            m_circleModelConstantBuffer.GetAddressOf()
        );
        // Draw the objects.
        UINT baseVertex = static_cast<UINT>( m_circleVertexIndex * m_circleVertexCount );
        context->DrawInstanced(
            m_circleVertexCount, // Vertex count per instance.
            2,                   // Instance count.
            baseVertex,          // Base vertex location.
            0                    // Start instance location.
        );
    }
}

std::future<void> GazePointRenderer::CreateDeviceDependentResources()
{
    m_usingVprtShaders = m_deviceResources->GetDeviceSupportsVprt();

    // On devices that do support the D3D11_FEATURE_D3D11_OPTIONS3::
    // VPAndRTArrayIndexFromAnyShaderFeedingRasterizer optional feature
    // we can avoid using a pass-through geometry shader to set the render
    // target array index, thus avoiding any overhead that would be 
    // incurred by setting the geometry shader stage.
    std::wstring vertexShaderFileName = m_usingVprtShaders ? L"ms-appx:///VprtVertexShader.cso" : L"ms-appx:///VertexShader.cso";

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

    constexpr std::array<D3D11_INPUT_ELEMENT_DESC, 2> vertexDesc =
    { {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    } };

    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateInputLayout(
            vertexDesc.data(),
            static_cast<UINT>(vertexDesc.size()),
            vertexShaderFileData.data(),
            static_cast<UINT>(vertexShaderFileData.size()),
            &m_inputLayout
        )
    );

    // After the pixel shader file is loaded, create the shader and constant buffer.
    std::vector<byte> pixelShaderFileData = co_await DX::ReadDataAsync(L"ms-appx:///PixelShader.cso");
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreatePixelShader(
            pixelShaderFileData.data(),
            pixelShaderFileData.size(),
            nullptr,
            &m_pixelShader
        )
    );

    const CD3D11_BUFFER_DESC constantBufferDesc(sizeof(ModelConstantBuffer), D3D11_BIND_CONSTANT_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &constantBufferDesc,
            nullptr,
            &m_cubeModelConstantBuffer
        )
    );
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &constantBufferDesc,
            nullptr,
            &m_circleModelConstantBuffer
        )
    );

    if (!m_usingVprtShaders)
    {
        // Load the pass-through geometry shader.
        std::vector<byte> geometryShaderFileData = co_await DX::ReadDataAsync(L"ms-appx:///GeometryShader.cso");

        // After the pass-through geometry shader file is loaded, create the shader.
        winrt::check_hresult(
            m_deviceResources->GetD3DDevice()->CreateGeometryShader(
                geometryShaderFileData.data(),
                geometryShaderFileData.size(),
                nullptr,
                &m_geometryShader
            )
        );
    }

    // Load mesh vertices. Each vertex has a position and a color.
    // Note that the cube size has changed from the default DirectX app
    // template. Windows Holographic is scaled in meters, so to draw the
    // cube at a comfortable size we made the cube width ???cm.
    constexpr float cubeHalfLength = 0.5f;
    static const std::array<VertexPositionColor, 8> cubeVertices =
    { {
        { XMFLOAT3(-cubeHalfLength, -cubeHalfLength, -cubeHalfLength), XMFLOAT3(0.0f, 0.0f, 0.0f) },
        { XMFLOAT3(-cubeHalfLength, -cubeHalfLength,  cubeHalfLength), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3(-cubeHalfLength,  cubeHalfLength, -cubeHalfLength), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3(-cubeHalfLength,  cubeHalfLength,  cubeHalfLength), XMFLOAT3(0.0f, 1.0f, 1.0f) },
        { XMFLOAT3(cubeHalfLength, -cubeHalfLength, -cubeHalfLength), XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3(cubeHalfLength, -cubeHalfLength,  cubeHalfLength), XMFLOAT3(1.0f, 0.0f, 1.0f) },
        { XMFLOAT3(cubeHalfLength,  cubeHalfLength, -cubeHalfLength), XMFLOAT3(1.0f, 1.0f, 0.0f) },
        { XMFLOAT3(cubeHalfLength,  cubeHalfLength,  cubeHalfLength), XMFLOAT3(1.0f, 1.0f, 1.0f) },
    } };

    D3D11_SUBRESOURCE_DATA vertexBufferData = { 0 };
    vertexBufferData.pSysMem = cubeVertices.data();
    vertexBufferData.SysMemPitch = 0;
    vertexBufferData.SysMemSlicePitch = 0;
    const CD3D11_BUFFER_DESC vertexBufferDesc(sizeof(VertexPositionColor) * static_cast<UINT>(cubeVertices.size()), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &vertexBufferDesc,
            &vertexBufferData,
            &m_cubeVertexBuffer
        )
    );

    // Load mesh indices. Each trio of indices represents
    // a triangle to be rendered on the screen.
    // For example: 2,1,0 means that the vertices with indexes
    // 2, 1, and 0 from the vertex buffer compose the
    // first triangle of this mesh.
    // Note that the winding order is clockwise by default.
    constexpr std::array<unsigned short, 36> cubeIndices =
    { {
        2,1,0, // -x
        2,3,1,

        6,4,5, // +x
        6,5,7,

        0,1,5, // -y
        0,5,4,

        2,6,7, // +y
        2,7,3,

        0,4,6, // -z
        0,6,2,

        1,3,7, // +z
        1,7,5,
    } };

    m_cubeIndexCount = static_cast<unsigned int>(cubeIndices.size());

    D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
    indexBufferData.pSysMem = cubeIndices.data();
    indexBufferData.SysMemPitch = 0;
    indexBufferData.SysMemSlicePitch = 0;
    CD3D11_BUFFER_DESC indexBufferDesc(sizeof(unsigned short) * static_cast<UINT>(cubeIndices.size()), D3D11_BIND_INDEX_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &indexBufferDesc,
            &indexBufferData,
            &m_cubeIndexBuffer
        )
    );

    // Load circle vertices.
    //
    constexpr size_t       NUM_OF_CIRCLE = 3;
    constexpr unsigned int CIRCLE_VERTICES_COUNT = CIRCLE_SEGMENT + 1;
    constexpr float        THETA_STEP = 6.28318530718f / static_cast<float>(CIRCLE_SEGMENT);
    
    // 0: Normal - white, 1: High - green, 2: Low - red
    constexpr XMFLOAT3 _CIRCLE_COLOR[NUM_OF_CIRCLE] = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
    constexpr XMFLOAT3 _START_POINT = { 0.0f, 1.0f, 0.0f };
    
    std::array<VertexPositionColor, NUM_OF_CIRCLE * CIRCLE_VERTICES_COUNT> circleVertices;
    for (size_t c = 0; c < NUM_OF_CIRCLE; ++c)
    {
        VertexPositionColor* pCircleVertices = circleVertices.data() + (c * CIRCLE_VERTICES_COUNT);

        pCircleVertices[0].pos = _START_POINT;
        pCircleVertices[0].color = _CIRCLE_COLOR[c];

        pCircleVertices[CIRCLE_VERTICES_COUNT - 1].pos = _START_POINT;
        pCircleVertices[CIRCLE_VERTICES_COUNT - 1].color = _CIRCLE_COLOR[c];

        for (size_t i = 1; i < CIRCLE_VERTICES_COUNT - 1; i++) {
            float theta = THETA_STEP * i;
            pCircleVertices[i].pos = XMFLOAT3(sinf(theta), cosf(theta), 0.0f);
            pCircleVertices[i].color = _CIRCLE_COLOR[c];
        }
    }
    m_circleVertexCount = CIRCLE_VERTICES_COUNT;

    D3D11_SUBRESOURCE_DATA cVertexBufferData = { 0 };
    cVertexBufferData.pSysMem = circleVertices.data();
    cVertexBufferData.SysMemPitch = 0;
    cVertexBufferData.SysMemSlicePitch = 0;
    const CD3D11_BUFFER_DESC cVertexBufferDesc(sizeof(VertexPositionColor)* static_cast<UINT>(circleVertices.size()), D3D11_BIND_VERTEX_BUFFER);
    winrt::check_hresult(
        m_deviceResources->GetD3DDevice()->CreateBuffer(
            &cVertexBufferDesc,
            &cVertexBufferData,
            &m_circleVertexBuffer
        )
    );

    // the object is ready to be rendered.
    m_loadingComplete = true;
};

void GazePointRenderer::ReleaseDeviceDependentResources()
{
    m_loadingComplete = false;
    m_usingVprtShaders = false;
    m_vertexShader.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_geometryShader.Reset();
    m_cubeModelConstantBuffer.Reset();
    m_circleModelConstantBuffer.Reset();
    m_cubeVertexBuffer.Reset();
    m_cubeIndexBuffer.Reset();
    m_circleVertexBuffer.Reset();
}

