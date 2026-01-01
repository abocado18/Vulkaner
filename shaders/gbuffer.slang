

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

     float3 emissive;
    float metallic;
    float roughness;
   
    uint use_textures[8];
 
    
  
}


struct Transform {


    float4x4 transform_matrix;
    float4x4 normal_matrix;

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
    float3 view_normal;


    float2 tex_coords;

    float metallic;
    float roughness;

    float4 view_tangent;
 
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

     
     
    o.view_normal = mul(float3x3(cam_data.view_mat), mul(float3x3(transform.normal_matrix), normalize(in.normals)));
     

    o.view_tangent.rgb = mul(float3x3(cam_data.view_mat), mul(float3x3(transform.normal_matrix), normalize(in.tangent.rgb)));
    o.view_tangent.w = in.tangent.w;

    o.tex_coords = in.tex_coords0;

    o.metallic = material.metallic;
    o.roughness = material.roughness;
   
    return o;
}

[shader("fragment")]
PsOutput pixelMain(VsOutput in)
{
    
    PsOutput o;

    if (material.use_textures[0] == 1)
        {
            float3 albedo_tex = albedo.Sample(in.tex_coords).rgb;
            o.albedo = material.albedo * float4(albedo_tex, 1.0);
        }
        else
        {
            o.albedo = material.albedo;
        }

    if(material.use_textures[1] == 1)
        {

            float3 N = normalize(in.view_normal);
            float3 T = normalize(in.view_tangent.rgb);
            float3 B = cross(N, T) * in.view_tangent.w;
        
            float3x3 TBN = float3x3(T, B, N);

            float3 tex_n = normal.Sample(in.tex_coords).rgb;
            tex_n = tex_n * 2.0 - 1.0;

            float3 n_view_space = normalize(mul(TBN, tex_n));

        

            float2 encoded_n = encode(n_view_space);



            o.normal = float4(encoded_n, 0.0, 1.0);
        }
        else 
        {   
          

            float2 encoded_n = encode(in.view_normal);
            o.normal = float4(encoded_n, 0.0, 1.0);
        }

        if (material.use_textures[2] == 1)
            {
                float3 mr_tex = metallic_rougness.Sample(in.tex_coords).rgb;
        
                float metallic  = clamp(in.metallic  * mr_tex.b, 0.0, 1.0);
                float roughness = clamp(in.roughness * mr_tex.g, 0.0, 1.0);
        
                o.mrao.r = metallic;
                o.mrao.g = roughness;
                o.mrao.b = mr_tex.r; 
            }
            else
            {
                o.mrao.r = clamp(in.metallic,  0.0, 1.0);
                o.mrao.g = clamp(in.roughness, 0.0, 1.0);
                o.mrao.b = 1.0;
            }
        
            o.mrao.a = 1.0;
   

    return o;

    
}

float2 octWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * (v.xy >= 0.0 ? 1.0 : -1.0);
}
 
float2 encode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : octWrap(n.xy);
    return n.xy;
}

