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
VSOut vertexMain(uint id : SV_VertexID)
{
    // Hardcoded triangle positions in clip space
    float4 positions[3] = {
        float4(-0.5, 0.5, 0, 1),
        float4( 0.5, 0.5, 0, 1),
        float4( 0.0,  -0.5, 0, 1)
    };

      float2 uvs[3] = {
        float2(0,1),
        float2(1,1),
        float2(0.5, 0)
    };

    VSOut output;
    output.position = positions[id];
    output.uv = uvs[id];
    return output;
}




[shader("fragment")]
float4 pixelMain(VSOut input) : SV_Target
{

    
    Col bb = buffer_address[2];
    

    Sampler2D tex = sampled_textures[0];
    float3 rgb = tex.Sample(input.uv).rgb;
    return float4(bb.value, 1); // red triangle
}