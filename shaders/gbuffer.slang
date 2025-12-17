

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


struct Transform {


    float4x4 transform_matrix;


};


[vk_binding(0, 0)]
ConstantBuffer<CamData> cam_data;


[vk_binding(0, 1)]
ConstantBuffer<Transform> transform;




struct VsOutput
{
    float4 pos : SV_Position;
    float3 col;
};

[shader("vertex")]
VsOutput vertexMain(uint id : SV_VertexID, VsInput in)
{
     VsOutput o;

     o.pos = mul(cam_data.proj_mat, mul(cam_data.view_mat, mul(transform.transform_matrix, float4(in.position, 1.0))));

    

   
    return o;
}

[shader("fragment")]
float4 pixelMain(VsOutput in) : SV_Target
{
    return float4(1.0, 0.0, 0.0, 1.0);
}