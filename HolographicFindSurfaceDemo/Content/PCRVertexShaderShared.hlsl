// A constant buffer that stores the model transform.
cbuffer ModelConstantBuffer : register(b0)
{
    float4x4 model;
};

// A constant buffer that stores each set of view and projection matrices in column-major format.
cbuffer ViewProjectionConstantBuffer : register(b1)
{
    float4x4 viewProjection[2];
    float4x4 viewMatrix[2];
};

// Per-vertex data used as input to the vertex shader.
struct VertexShaderInput
{
    float3 pos     : POSITION;
    uint   instId  : SV_InstanceID;
};

// Simple shader to do vertex processing on the GPU.
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 pos = float4(input.pos, 1.0f);

    // Note which view this vertex has been sent to. Used for matrix lookup.
    // Taking the modulo of the instance ID allows geometry instancing to be used
    // along with stereo instanced drawing; in that case, two copies of each 
    // instance would be drawn, one for left and one for right.
    int idx = input.instId % 2;

    // Transform the vertex position into world space.
    pos = mul(pos, model);

    float4 cameraBasedPosition = mul(pos, viewMatrix[0]);
    float colorDistRatio = clamp(abs(cameraBasedPosition.z), 0.0f, 3.0f) / 3.0f; // up to 3 meter

    float3 outputColor = colorDistRatio > 0.5f
        ? lerp(float3(0.0f, 1.0f, 0.0f), float3(1.0f, 0.0f, 0.0f), (colorDistRatio - 0.5f) * 2.0f)
        : lerp(float3(0.0f, 0.0f, 1.0f), float3(0.0f, 1.0f, 0.0f), colorDistRatio * 2.0f);

    // Correct for perspective and project the vertex position onto the screen.
    pos = mul(pos, viewProjection[idx]);
    
    output.pos = (float4)pos;
    output.color = outputColor;

    // Set the render target array index.
    output.viewId = idx;

    return output;
}
