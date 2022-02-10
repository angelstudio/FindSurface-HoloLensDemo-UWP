// Per-vertex data passed to the geometry shader.
struct VertexShaderOutput
{
    float4 pos     : SV_POSITION;

    // The render target array index is set here in the vertex shader.
    uint   viewId  : SV_RenderTargetArrayIndex;
};

#include "MeshVertexShaderShared.hlsl"
