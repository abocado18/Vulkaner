






struct Vertex {

  


  float3 position;

  float3 normal;

  float3 tangent;

  float3 bitangent;

  float4 bone_ids;

  float4 bone_weights;

  float2 tex_coords;

  
};












struct VSOut
{
    float4 position : SV_Position;
    float2 uv;
};


struct Col
{
    float3 value;
    int _pad;
}

[[vk_push_constant]]
Col *buffer_address;





[vk_binding(0, 0)]
Sampler2D<float4> sampled_textures[];

[shader("vertex")]
VSOut vertexMain(Vertex in, uint id : SV_VertexID)
{
    // Hardcoded triangle positions in clip space

    float4x4 view = float4x4(
    0, 0, 1, 0,
    0, 1, 0, 0,
   -1, 0, 0, 0,
    0, 0, 15, 1
);

float4x4 proj = float4x4(
    1.3618, 0,      0,       0,
    0,      2.4142, 0,       0,
    0,      0,     -1.001,  -1,
    0,      0,     -0.1001,  0
);
    

    VSOut output;
    output.position = mul(proj, mul(view, float4(in.position, 1.0)));
    output.uv = in.tex_coords;
    return output;
}




[shader("fragment")]
float4 pixelMain(VSOut input) : SV_Target
{

   return float4(1.0, 0.0, 0.0, 1.0);
}