






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

      float2 positions[3] = {
        float2(0.0f,  0.5f),   // top
        float2(0.5f, -0.5f),   // right
        float2(-0.5f, -0.5f)   // left
    };

    VSOut output;
    output.position = float4(positions[id], 0.0f, 1.0f);

    // Assign UVs same as positions (for testing)
    float2 uvs[3] = {
        float2(0.5f, 1.0f),
        float2(1.0f, 0.0f),
        float2(0.0f, 0.0f)
    };
    output.uv = uvs[id];

    return output;
}




[shader("fragment")]
float4 pixelMain(VSOut input) : SV_Target
{


    float3 a = sampled_textures[0].Sample(input.uv).rgb;

   return float4(a, 1.0);
}