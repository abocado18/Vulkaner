


struct VsOutput
{
    float4 pos : SV_Position;
};

[shader("vertex")]
VsOutput vertexMain(uint id : SV_VertexID)
{
     VsOutput o;

   
    float2 p[3] = {
        float2( 0.0,  0.5),
        float2(-0.5, -0.5),
        float2( 0.5, -0.5)
    };

    o.pos = float4(p[id], 0.0, 1.0);
    return o;
}

[shader("fragment")]
float4 pixelMain(VsOutput in)
{
    return float4(1.0, 0.0, 0.0, 1.0);
}