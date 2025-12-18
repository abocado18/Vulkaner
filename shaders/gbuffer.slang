

struct VsInput {
    float3 position : POSITION;
  

    float3 color : COLOR;


    float3 normals : NORMAL;


    float4 tangent : TANGENT;

    float2 tex_coords0 : TEXCOORD0;
    float2 tex_coords1 : TEXCOORD1;
};

struct CamData {
    float4x4 proj_mat;
    float4x4 view_mat;
    float4x4 inv_view_mat;
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
    float3 normal;
 
};

struct PsOutput {
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1;
    float4 mrao : SV_Target2;

};
    

[shader("vertex")]
VsOutput vertexMain(uint id : SV_VertexID, VsInput in)
{
     VsOutput o;


     o.pos = mul(cam_data.proj_mat, mul(cam_data.view_mat, mul(transform.transform_matrix, float4(in.position, 1.0))));

     float3 world_normal = mul(cam_data.inv_view_mat, float4(in.normals, 1.0)).xyz;

    o.normal = world_normal;
   
    return o;
}

[shader("fragment")]
PsOutput pixelMain(VsOutput in)
{
    
    PsOutput o;


    float3 world_normal = mul(cam_data.inv_view_mat, float4(in.normal, 1.0)).xyz;

    o.normal = float4(in.normal, 1.0);
    o.albedo = float4(1.0, 0.0, 0.0, 1.0);
    o.mrao = float4(0.0, 0.0, 1.0, 1.0);

    return o;
}


float2 packNormal ( float3 normal )
{
    float2 result;
    result.xy = normalize ( normal.xy ) * sqrt ( normal.z * 0.5f + 0.5f );

    return result;
}