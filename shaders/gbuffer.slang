

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

struct MaterialData {
    float4 albedo;
    float metallic;
    float roughness;
    float3 emissive;
  
}


struct Transform {


    float4x4 transform_matrix;


};


[vk_binding(0, 0)]
ConstantBuffer<CamData> cam_data;


[vk_binding(0, 1)]
ConstantBuffer<Transform> transform;


[vk_binding(0, 2)]
ConstantBuffer<MaterialData> material;

[vk_binding(0, 5)]
Sampler2D albedo;

[vk_binding(0, 6)]
Sampler2D normal;

[vk_binding(0, 7)]
Sampler2D metallic_rougness;

[vk_binding(0, 8)]
Sampler2D emissive;




struct VsOutput
{
    float4 pos : SV_Position;
    float3 normal;


    float2 tex_coords;

    float metallic;
    float roughness;
 
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

    o.tex_coords = in.tex_coords0;

    o.metallic = material.metallic;
    o.roughness = material.roughness;
   
    return o;
}

[shader("fragment")]
PsOutput pixelMain(VsOutput in)
{
    
    PsOutput o;


   


    float3 albedo_tex_value = albedo.Sample(in.tex_coords).rgb;
    float3 normal_tex_value = normal.Sample(in.tex_coords).rgb;
    float3 mr_tex_value = metallic_rougness.Sample(in.tex_coords).rgb;

    

    o.normal = float4(in.normal, 1.0) * float4(normal_tex_value, 1.0);
    o.albedo = material.albedo * float4(albedo_tex_value, 1.0);
    o.mrao = float4(mr_tex_value, 1.0);
    o.mrao.r = in.metallic;
    o.mrao.g = in.roughness;

    return o;

    
}

