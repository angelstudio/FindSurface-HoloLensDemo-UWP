// Per-vertex data from the vertex shader.
struct GeometryShaderInput
{
    float4 pos     : SV_POSITION;
    float3 color   : COLOR0;
    uint   instId  : TEXCOORD0;
};

// Per-vertex data passed to the rasterizer.
struct GeometryShaderOutput
{
    float4 pos     : SV_POSITION;
    float3 color   : COLOR0;
    uint   rtvId   : SV_RenderTargetArrayIndex;
};

// This geometry shader is a pass-through that leaves the geometry unmodified 
// and sets the render target array index.
[maxvertexcount(1)]
void main(point GeometryShaderInput input[1], inout PointStream<GeometryShaderOutput> outStream)
{
    GeometryShaderOutput output;
    
    output.pos = input[0].pos;
    output.color = input[0].color;
    output.rtvId = input[0].instId;
    outStream.Append(output);
}
