#pragma once

namespace HolographicFindSurfaceDemo
{
    //
    // Constant buffer for MeshRenderer
    //

    struct InstanceConstantBuffer // shared both vertex & pixel shader
    {
        // for Vertex Shader
        DirectX::XMFLOAT4X4 model; // Model Transform Matrix
        int modelIndex;
        int modelType;
        float param1;
        float param2;
        // for Pixel Shader
        DirectX::XMFLOAT4 color;   // Model Color
    };

    // Assert that the constant buffer remains 16-byte aligned (best practice).
    static_assert((sizeof(InstanceConstantBuffer) % (sizeof(float) * 4)) == 0, "Model constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");

    //
    // Constant buffer for GazePointRenderer
    //

    // Constant buffer used to send hologram position transform to the shader pipeline.
    struct ModelConstantBuffer
    {
        DirectX::XMFLOAT4X4 model;
    };

    // Assert that the constant buffer remains 16-byte aligned (best practice).
    static_assert((sizeof(ModelConstantBuffer) % (sizeof(float) * 4)) == 0, "Model constant buffer size must be 16-byte aligned (16 bytes is the length of four floats).");


    // Used to send per-vertex data to the vertex shader.
    struct VertexPositionColor // for Spinning Cube
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT3 color;
    };
};

