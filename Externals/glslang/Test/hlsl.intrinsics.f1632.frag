float PixelShaderFunction(float inF0)
{
    f32tof16(inF0);

    return 0.0;
}

float1 PixelShaderFunction(float1 inF0)
{
    // TODO: ... add when float1 prototypes are generated
    return 0.0;
}

float2 PixelShaderFunction(float2 inF0)
{
    f32tof16(inF0);

    return float2(1,2);
}

float3 PixelShaderFunction(float3 inF0)
{
    f32tof16(inF0);

    return float3(1,2,3);
}

float4 PixelShaderFunction(float4 inF0)
{
    f32tof16(inF0);

    return float4(1,2,3,4);
}

