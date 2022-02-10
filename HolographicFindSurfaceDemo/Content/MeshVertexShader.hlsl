// Per-vertex data passed to the geometry shader.
struct VertexShaderOutput
{
    float4 pos     : SV_POSITION;

    // The render target array index will be set by the geometry shader.
    uint   viewId  : TEXCOORD0;
};

#include "MeshVertexShaderShared.hlsl"
