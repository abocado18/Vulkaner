

struct VsInput {
    float3 position : POSITION;
    float3 color : COLOR;
    float3 normals : NORMAL;
    float2 tex_coords0 : TEXCOORD0;
    float2 tex_coords1 : TEXCOORD1;

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
    float3 normal;
 
};

struct PsOutput {
    float4 albedo : SV_Target0;
    float2 normal : SV_Target1;
    float4 mrao : SV_Target2;

};
    

[shader("vertex")]
VsOutput vertexMain(uint id : SV_VertexID, VsInput in)
{
     VsOutput o;


     o.pos = mul(cam_data.proj_mat, mul(cam_data.view_mat, mul(transform.transform_matrix, float4(in.position, 1.0))));

  
    o.normal = in.normals;
   
    return o;
}

[shader("fragment")]
PsOutput pixelMain(VsOutput in)
{
    
    PsOutput o;

    o.normal = encodeNormalOctahedron(in.normal);
    o.albedo = float4(1.0, 0.0, 0.0, 1.0);
    o.mrao = float4(0.0, 0.0, 1.0, 1.0);

    return o;
}


float2 encodeNormalOctahedron(float3 n)
{
    
    n /= (abs(n.x) + abs(n.y) + abs(n.z));

    float2 enc;
    if (n.z >= 0)
        enc = n.xy;
    else
        enc = (1 - abs(n.yx)) * float2(sign(n.x), sign(n.y));

   
    enc = enc * 0.5 + 0.5;

    return enc;
}