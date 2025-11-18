

RWTexture2D<float4> image : register(u0, space0);

[numthreads(16, 16, 1)]

void main(uint3 global_inv_id: SV_DispatchThreadID, uint3 local_inv_id: SV_GroupThreadID)
{
    int2 texel_coord = global_inv_id.xy;
    int2 size;
    image.GetDimensions(size.x, size.y);

    if (texel_coord.x < size.x && texel_coord.y < size.y)
    {
        float4 color = float4(0.0, 0.0, 0.0, 1.0);

        if (local_inv_id.x != 0 && local_inv_id.y != 0)
        {
            color.x = float(texel_coord.x / size.x);
            color.y = float(texel_coord.y / size.y);
        }

        image.Store(texel_coord, color);
    }
}
