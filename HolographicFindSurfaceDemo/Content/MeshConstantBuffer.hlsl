// A constant buffer that stores the model transform.
cbuffer InstanceConstantBuffer : register(b0)
{
    // for Vertex Shader
    float4x4 model; // Model Transform Matrix
    int modelIndex; // Do not use in shader
    int modelType;  // 0: General, 1: Cone, 2: Torus
    float1 param1;  // Cone: Top Radius / Bottom Radius, Torus: Mean Radius
    float1 param2;  // Torus: Tube Radius
    // for Pixel Shader
    float4 color;   // Model Color
};
