
#define gs     // TODO: define as groupshared when available in the grammar
gs uint gs_ua;
gs uint gs_ub;
gs uint gs_uc;
gs uint2 gs_ua2;
gs uint2 gs_ub2;
gs uint2 gs_uc2;
gs uint3 gs_ua3;
gs uint3 gs_ub3;
gs uint3 gs_uc3;
gs uint4 gs_ua4;
gs uint4 gs_ub4;
gs uint4 gs_uc4;

float PixelShaderFunction(float inF0, float inF1, float inF2, uint inU0, uint inU1)
{
    uint out_u1;

    all(inF0);
    abs(inF0);
    acos(inF0);
    any(inF0);
    asin(inF0);
    asint(inF0);
    asuint(inF0);
    asfloat(inU0);
    // asdouble(inU0, inU1);  // TODO: enable when HLSL parser used for intrinsics
    atan(inF0);
    atan2(inF0, inF1);
    ceil(inF0);
    clamp(inF0, inF1, inF2);
    clip(inF0);
    cos(inF0);
    cosh(inF0);
    countbits(7);
    ddx(inF0);
    ddx_coarse(inF0);
    ddx_fine(inF0);
    ddy(inF0);
    ddy_coarse(inF0);
    ddy_fine(inF0);
    degrees(inF0);
    // EvaluateAttributeAtCentroid(inF0);
    // EvaluateAttributeAtSample(inF0, 0);
    // TODO: EvaluateAttributeSnapped(inF0, int2(1,2));
    exp(inF0);
    exp2(inF0);
    firstbithigh(7);
    firstbitlow(7);
    floor(inF0);
    // TODO: fma(inD0, inD1, inD2);
    fmod(inF0, inF1);
    frac(inF0);
    frexp(inF0, inF1);
    fwidth(inF0);
    isinf(inF0);
    isnan(inF0);
    ldexp(inF0, inF1);
    log(inF0);
    log10(inF0);
    log2(inF0);
    max(inF0, inF1);
    min(inF0, inF1);
    pow(inF0, inF1);
    radians(inF0);
    rcp(inF0);
    reversebits(2);
    round(inF0);
    rsqrt(inF0);
    saturate(inF0);
    sign(inF0);
    sin(inF0);
    sincos(inF0, inF1, inF2);
    sinh(inF0);
    smoothstep(inF0, inF1, inF2);
    sqrt(inF0);
    step(inF0, inF1);
    tan(inF0);
    tanh(inF0);
    // TODO: sampler intrinsics, when we can declare the types.
    trunc(inF0);

    return 0.0;
}

float1 PixelShaderFunction(float1 inF0, float1 inF1, float1 inF2)
{
    // TODO: ... add when float1 prototypes are generated
    return 0.0;
}

float2 PixelShaderFunction(float2 inF0, float2 inF1, float2 inF2, uint2 inU0, uint2 inU1)
{
    uint2 out_u2;

    all(inF0);
    abs(inF0);
    acos(inF0);
    any(inF0);
    asin(inF0);
    asint(inF0);
    asuint(inF0);
    asfloat(inU0);
    // asdouble(inU0, inU1);  // TODO: enable when HLSL parser used for intrinsics
    atan(inF0);
    atan2(inF0, inF1);
    ceil(inF0);
    clamp(inF0, inF1, inF2);
    clip(inF0);
    cos(inF0);
    cosh(inF0);
    countbits(int2(7,3));
    ddx(inF0);
    ddx_coarse(inF0);
    ddx_fine(inF0);
    ddy(inF0);
    ddy_coarse(inF0);
    ddy_fine(inF0);
    degrees(inF0);
    distance(inF0, inF1);
    dot(inF0, inF1);
    // EvaluateAttributeAtCentroid(inF0);
    // EvaluateAttributeAtSample(inF0, 0);
    // TODO: EvaluateAttributeSnapped(inF0, int2(1,2));
    exp(inF0);
    exp2(inF0);
    faceforward(inF0, inF1, inF2);
    firstbithigh(7);
    firstbitlow(7);
    floor(inF0);
    // TODO: fma(inD0, inD1, inD2);
    fmod(inF0, inF1);
    frac(inF0);
    frexp(inF0, inF1);
    fwidth(inF0);
    isinf(inF0);
    isnan(inF0);
    ldexp(inF0, inF1);
    length(inF0);
    log(inF0);
    log10(inF0);
    log2(inF0);
    max(inF0, inF1);
    min(inF0, inF1);
    normalize(inF0);
    pow(inF0, inF1);
    radians(inF0);
    rcp(inF0);
    reflect(inF0, inF1);
    refract(inF0, inF1, 2.0);
    reversebits(int2(1,2));
    round(inF0);
    rsqrt(inF0);
    saturate(inF0);
    sign(inF0);
    sin(inF0);
    sincos(inF0, inF1, inF2);
    sinh(inF0);
    smoothstep(inF0, inF1, inF2);
    sqrt(inF0);
    step(inF0, inF1);
    tan(inF0);
    tanh(inF0);
    // TODO: sampler intrinsics, when we can declare the types.
    trunc(inF0);

    // TODO: ... add when float1 prototypes are generated
    return float2(1,2);
}

float3 PixelShaderFunction(float3 inF0, float3 inF1, float3 inF2, uint3 inU0, uint3 inU1)
{
    uint3 out_u3;
    
    all(inF0);
    abs(inF0);
    acos(inF0);
    any(inF0);
    asin(inF0);
    asint(inF0);
    asuint(inF0);
    asfloat(inU0);
    // asdouble(inU0, inU1);  // TODO: enable when HLSL parser used for intrinsics
    atan(inF0);
    atan2(inF0, inF1);
    ceil(inF0);
    clamp(inF0, inF1, inF2);
    clip(inF0);
    cos(inF0);
    cosh(inF0);
    countbits(int3(7,3,5));
    cross(inF0, inF1);
    ddx(inF0);
    ddx_coarse(inF0);
    ddx_fine(inF0);
    ddy(inF0);
    ddy_coarse(inF0);
    ddy_fine(inF0);
    degrees(inF0);
    distance(inF0, inF1);
    dot(inF0, inF1);
    // EvaluateAttributeAtCentroid(inF0);
    // EvaluateAttributeAtSample(inF0, 0);
    // TODO: EvaluateAttributeSnapped(inF0, int2(1,2));
    exp(inF0);
    exp2(inF0);
    faceforward(inF0, inF1, inF2);
    firstbithigh(7);
    firstbitlow(7);
    floor(inF0);
    // TODO: fma(inD0, inD1, inD2);
    fmod(inF0, inF1);
    frac(inF0);
    frexp(inF0, inF1);
    fwidth(inF0);
    isinf(inF0);
    isnan(inF0);
    ldexp(inF0, inF1);
    length(inF0);
    log(inF0);
    log10(inF0);
    log2(inF0);
    max(inF0, inF1);
    min(inF0, inF1);
    normalize(inF0);
    pow(inF0, inF1);
    radians(inF0);
    rcp(inF0);
    reflect(inF0, inF1);
    refract(inF0, inF1, 2.0);
    reversebits(int3(1,2,3));
    round(inF0);
    rsqrt(inF0);
    saturate(inF0);
    sign(inF0);
    sin(inF0);
    sincos(inF0, inF1, inF2);
    sinh(inF0);
    smoothstep(inF0, inF1, inF2);
    sqrt(inF0);
    step(inF0, inF1);
    tan(inF0);
    tanh(inF0);
    // TODO: sampler intrinsics, when we can declare the types.
    trunc(inF0);

    // TODO: ... add when float1 prototypes are generated
    return float3(1,2,3);
}

float4 PixelShaderFunction(float4 inF0, float4 inF1, float4 inF2, uint4 inU0, uint4 inU1)
{
    uint4 out_u4;

    all(inF0);
    abs(inF0);
    acos(inF0);
    any(inF0);
    asin(inF0);
    asint(inF0);
    asuint(inF0);
    asfloat(inU0);
    // asdouble(inU0, inU1);  // TODO: enable when HLSL parser used for intrinsics
    atan(inF0);
    atan2(inF0, inF1);
    ceil(inF0);
    clamp(inF0, inF1, inF2);
    clip(inF0);
    cos(inF0);
    cosh(inF0);
    countbits(int4(7,3,5,2));
    ddx(inF0);
    ddx_coarse(inF0);
    ddx_fine(inF0);
    ddy(inF0);
    ddy_coarse(inF0);
    ddy_fine(inF0);
    degrees(inF0);
    distance(inF0, inF1);
    dot(inF0, inF1);
    dst(inF0, inF1);
    // EvaluateAttributeAtCentroid(inF0);
    // EvaluateAttributeAtSample(inF0, 0);
    // TODO: EvaluateAttributeSnapped(inF0, int2(1,2));
    exp(inF0);
    exp2(inF0);
    faceforward(inF0, inF1, inF2);
    firstbithigh(7);
    firstbitlow(7);
    floor(inF0);
    // TODO: fma(inD0, inD1, inD2);
    fmod(inF0, inF1);
    frac(inF0);
    frexp(inF0, inF1);
    fwidth(inF0);
    isinf(inF0);
    isnan(inF0);
    ldexp(inF0, inF1);
    length(inF0);
    log(inF0);
    log10(inF0);
    log2(inF0);
    max(inF0, inF1);
    min(inF0, inF1);
    normalize(inF0);
    pow(inF0, inF1);
    radians(inF0);
    rcp(inF0);
    reflect(inF0, inF1);
    refract(inF0, inF1, 2.0);
    reversebits(int4(1,2,3,4));
    round(inF0);
    rsqrt(inF0);
    saturate(inF0);
    sign(inF0);
    sin(inF0);
    sincos(inF0, inF1, inF2);
    sinh(inF0);
    smoothstep(inF0, inF1, inF2);
    sqrt(inF0);
    step(inF0, inF1);
    tan(inF0);
    tanh(inF0);
    // TODO: sampler intrinsics, when we can declare the types.
    trunc(inF0);

    // TODO: ... add when float1 prototypes are generated
    return float4(1,2,3,4);
}

// TODO: for mats:
//    asfloat(inU0); \
//    asint(inF0); \
//    asuint(inF0); \

// TODO: FXC doesn't accept this with (), but glslang doesn't accept it without.
#define MATFNS() \
    all(inF0); \
    abs(inF0); \
    acos(inF0); \
    any(inF0); \
    asin(inF0); \
    atan(inF0); \
    atan2(inF0, inF1); \
    ceil(inF0); \
    clip(inF0); \
    clamp(inF0, inF1, inF2); \
    cos(inF0); \
    cosh(inF0); \
    ddx(inF0); \
    ddx_coarse(inF0); \
    ddx_fine(inF0); \
    ddy(inF0); \
    ddy_coarse(inF0); \
    ddy_fine(inF0); \
    degrees(inF0); \
    determinant(inF0); \
    exp(inF0); \
    exp2(inF0); \
    firstbithigh(7); \
    firstbitlow(7); \
    floor(inF0); \
    fmod(inF0, inF1); \
    frac(inF0); \
    frexp(inF0, inF1); \
    fwidth(inF0); \
    ldexp(inF0, inF1); \
    log(inF0); \
    log10(inF0); \
    log2(inF0);      \
    max(inF0, inF1); \
    min(inF0, inF1); \
    pow(inF0, inF1); \
    radians(inF0); \
    round(inF0); \
    rsqrt(inF0); \
    saturate(inF0); \
    sign(inF0); \
    sin(inF0); \
    sincos(inF0, inF1, inF2); \
    sinh(inF0); \
    smoothstep(inF0, inF1, inF2); \
    sqrt(inF0); \
    step(inF0, inF1); \
    tan(inF0); \
    tanh(inF0); \
    transpose(inF0); \
    trunc(inF0);

// TODO: turn on non-square matrix tests when protos are available.

float2x2 PixelShaderFunction(float2x2 inF0, float2x2 inF1, float2x2 inF2)
{
    // TODO: FXC doesn't accept this with (), but glslang doesn't accept it without.
    MATFNS()

    // TODO: ... add when float1 prototypes are generated
    return float2x2(2,2,2,2);
}

float3x3 PixelShaderFunction(float3x3 inF0, float3x3 inF1, float3x3 inF2)
{
    // TODO: FXC doesn't accept this with (), but glslang doesn't accept it without.
    MATFNS()

    // TODO: ... add when float1 prototypes are generated
    return float3x3(3,3,3,3,3,3,3,3,3);
}

float4x4 PixelShaderFunction(float4x4 inF0, float4x4 inF1, float4x4 inF2)
{
    // TODO: FXC doesn't accept this with (), but glslang doesn't accept it without.
    MATFNS()

    // TODO: ... add when float1 prototypes are generated
    return float4x4(4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4);
}

#define TESTGENMUL(ST, VT, MT) \
    ST r0 = mul(inF0,  inF1);  \
    VT r1 = mul(inFV0, inF0);  \
    VT r2 = mul(inF0,  inFV0); \
    ST r3 = mul(inFV0, inFV1); \
    VT r4 = mul(inFM0, inFV0); \
    VT r5 = mul(inFV0, inFM0); \
    MT r6 = mul(inFM0, inF0);  \
    MT r7 = mul(inF0, inFM0);  \
    MT r8 = mul(inFM0, inFM1);


void TestGenMul(float inF0, float inF1,
                float2 inFV0, float2 inFV1,
                float2x2 inFM0, float2x2 inFM1)
{
    TESTGENMUL(float, float2, float2x2);
}

void TestGenMul(float inF0, float inF1,
                float3 inFV0, float3 inFV1,
                float3x3 inFM0, float3x3 inFM1)
{
    TESTGENMUL(float, float3, float3x3);
}

void TestGenMul(float inF0, float inF1,
                float4 inFV0, float4 inFV1,
                float4x4 inFM0, float4x4 inFM1)
{
    TESTGENMUL(float, float4, float4x4);
}
