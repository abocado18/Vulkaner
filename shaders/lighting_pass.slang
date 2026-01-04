



struct CamData {
    float4x4 proj_mat;
    float4x4 view_mat;
    float4x4 inv_view_mat;
};












[vk::binding(0, 0)]
Sampler2D albedo;

[vk::binding(1, 0)]
Sampler2D depth;

[vk::binding(2, 0)]
Sampler2D normal;



[vk::binding(3, 0)]
Sampler2D mrao;

[vk::binding(4, 0)]
Sampler2D emissive;

[vk::binding(5, 0)]
Sampler2D<uint16_t2> object_ids;




struct VsOutput
{
    float4 pos : SV_Position;


    float2 tex_coords;

 
};

struct PsOutput {
    float4 lighting : SV_Target0;
    

};
    

[shader("vertex")]
VsOutput vertexMain(uint id : SV_VertexID)
{
     VsOutput o;


     float2 pos;
     if (id == 0)      pos = float2(-1.0, -1.0);
     else if (id == 1) pos = float2( 3.0, -1.0);
     else                    pos = float2(-1.0,  3.0);

  
     o.pos = float4(pos, 0.0, 1.0);

     o.tex_coords = pos * 0.5 + 0.5;

   
    return o;
}

[shader("fragment")]
PsOutput pixelMain(VsOutput in)
{
    
    PsOutput o;

    float3 albedo_sample = albedo.Sample(in.tex_coords).rgb;
    float3 d = depth.Sample(in.tex_coords).rgb;
    float3 e = emissive.Sample(in.tex_coords).rgb;
    float id = object_ids.Sample(in.tex_coords).r;
    float2 mrao_v = mrao.Sample(in.tex_coords).rg;
    float2 n = normal.Sample(in.tex_coords).rg;


    float ultimate_number = albedo_sample.r + d.r + e.r + id + mrao_v.r + n.r;

    o.lighting = float4(ultimate_number, 0.0, 0.0, 1.0);

    return o;

    
}


