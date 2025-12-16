

struct VsInput {
    float3 position;
    uint _pad0;
    float3 color;
    uint _pad1;
    float3 normals;
    uint _pad2;
    float2 tex_coords0;
    float2 tex_coords1;

};

struct CamData {
    float4x4 proj_mat;
    float4x4 view_mat;
};


[vk_binding(0, 0)]
StructuredBuffer<CamData> cam_data;




struct VsOutput
{
    float4 pos : SV_Position;
    float3 col;
};

[shader("vertex")]
VsOutput vertexMain(uint id : SV_VertexID, VsInput in)
{
     VsOutput o;

   
    float2 p[3] = {
        float2( 0.0,  0.5),
        float2(-0.5, -0.5),
        float2( 0.5, -0.5)
    };

    o.col = in.color;

    o.pos = float4(p[id], 0.0, 1.0);
    return o;
}

[shader("fragment")]
float4 pixelMain(VsOutput in)
{
    return float4(in.col, 1.0);
}