// A constant buffer that stores the model transform.
#include "MeshConstantBuffer.hlsl"

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

static const float _2_PI_ = 6.28318530718f;

// Simple shader to do vertex processing on the GPU.
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 pos = float4(input.pos, 1.0f);

    if (modelType == 1 )
    {
        float ratio = lerp(1.0f, param1, input.pos.y + 0.5f);
        pos.x *= ratio;
        pos.z *= ratio;
    }
    else if (modelType == 2)
    {
        float mr = param1;
        float tr = param2;

        float ratio = (input.pos.y + 0.5f);
        if (ratio > 0.99f) { ratio = 0.0f; }

        float theta = lerp(0.0f, _2_PI_, ratio);

        float c = cos(-theta);
        float s = sin(-theta);
        float3x3 rotY = { c, 0.0f, -s, 0.0f, 1.0f, 0.0f, s, 0.0f, c }; //{ c, 0.0f, s, 0.0f, 1.0f, 0.0f, -s, 0.0f, c }; // Check!!

        float3 base = { tr * input.pos.x + mr, -tr * input.pos.z, 0.0f };
        pos = float4(mul(base, rotY), 1);
    }


    // Note which view this vertex has been sent to. Used for matrix lookup.
    // Taking the modulo of the instance ID allows geometry instancing to be used
    // along with stereo instanced drawing; in that case, two copies of each 
    // instance would be drawn, one for left and one for right.
    int idx = input.instId % 2;

    // Transform the vertex position into world space.
    pos = mul(pos, model);

    // Correct for perspective and project the vertex position onto the screen.
    pos = mul(pos, viewProjection[idx]);
    output.pos = (float4)pos;

    // Set the render target array index.
    output.viewId = idx;

    return output;
}
