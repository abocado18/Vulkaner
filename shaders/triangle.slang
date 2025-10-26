struct VSOut
{
    float4 position : SV_Position;
};

[shader("vertex")]
VSOut vertexMain(uint id : SV_VertexID)
{
    // Hardcoded triangle positions in clip space
    float4 positions[3] = {
        float4(-0.5, -0.5, 0, 1),
        float4( 0.5, -0.5, 0, 1),
        float4( 0.0,  0.5, 0, 1)
    };

    VSOut output;
    output.position = positions[id];
    return output;
}

[shader("fragment")]
float4 pixelMain(VSOut input) : SV_Target
{
    return float4(0a, 1, 0, 1); // red triangle
}