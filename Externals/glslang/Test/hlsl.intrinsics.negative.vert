uint gs_ua;
uint gs_ub;
uint gs_uc;
uint2 gs_ua2;
uint2 gs_ub2;
uint2 gs_uc2;
uint3 gs_ua3;
uint3 gs_ub3;
uint3 gs_uc3;
uint4 gs_ua4;
uint4 gs_ub4;
uint4 gs_uc4;

float VertexShaderFunction(float inF0, float inF1, float inF2, int inI0)
{
    uint out_u1;

    AllMemoryBarrier();                       // expected error: only valid in compute stage
    AllMemoryBarrierWithGroupSync();          // expected error: only valid in compute stage
    asdouble(inF0, inF1);                     // expected error: only integer inputs
    CheckAccessFullyMapped(3.0);              // expected error: only valid on integers
    CheckAccessFullyMapped(3);                // expected error: only valid in pixel & compute stages
    clip(inF0);                               // expected error: only valid in pixel stage
    countbits(inF0);                          // expected error: only integer inputs
    cross(inF0, inF1);                        // expected error: only on float3 inputs
    D3DCOLORtoUBYTE4(inF0);                   // expected error: only on float4 inputs
    DeviceMemoryBarrier();                    // expected error: only valid in pixel & compute stages
    DeviceMemoryBarrierWithGroupSync();       // expected error: only valid in compute stage
    ddx(inF0);                                // expected error: only valid in pixel stage
    ddx_coarse(inF0);                         // expected error: only valid in pixel stage
    ddx_fine(inF0);                           // expected error: only valid in pixel stage
    ddy(inF0);                                // expected error: only valid in pixel stage
    ddy_coarse(inF0);                         // expected error: only valid in pixel stage
    ddy_fine(inF0);                           // expected error: only valid in pixel stage
    determinant(inF0);                        // expected error: only valid on mats
    EvaluateAttributeAtCentroid(inF0);        // expected error: only valid in pixel stage
    EvaluateAttributeAtSample(inF0, 2);       // expected error: only valid in pixel stage
    EvaluateAttributeSnapped(inF0, int2(2));  // expected error: only valid in pixel stage
    f16tof32(inF0);                           // expected error: only integer inputs
    firstbithigh(inF0);                       // expected error: only integer inputs
    firstbitlow(inF0);                        // expected error: only integer inputs
    fma(inF0, inF1, inF2);                    // expected error: only double inputs
    fwidth(inF0);                             // expected error: only valid in pixel stage
    InterlockedAdd(gs_ua, gs_ub);             // expected error: only valid in pixel stage
    InterlockedAdd(gs_ua, gs_ub, out_u1);     // expected error: only valid in pixel stage
    InterlockedAnd(gs_ua, gs_ub);             // expected error: only valid in pixel stage
    InterlockedAnd(gs_ua, gs_ub, out_u1);     // expected error: only valid in pixel stage
    InterlockedCompareExchange(gs_ua, gs_ub, gs_uc, out_u1); // expected error: only valid in pixel stage
    InterlockedExchange(gs_ua, gs_ub, out_u1);// expected error: only valid in pixel stage
    InterlockedMax(gs_ua, gs_ub);             // expected error: only valid in pixel stage
    InterlockedMax(gs_ua, gs_ub, out_u1);     // expected error: only valid in pixel stage
    InterlockedMin(gs_ua, gs_ub);             // expected error: only valid in pixel stage
    InterlockedMin(gs_ua, gs_ub, out_u1);     // expected error: only valid in pixel stage
    InterlockedOr(gs_ua, gs_ub);              // expected error: only valid in pixel stage
    InterlockedOr(gs_ua, gs_ub, out_u1);      // expected error: only valid in pixel stage
    InterlockedXor(gs_ua, gs_ub);             // expected error: only valid in pixel stage
    InterlockedXor(gs_ua, gs_ub, out_u1);     // expected error: only valid in pixel stage
    GroupMemoryBarrier();                     // expected error: only valid in compute stage
    GroupMemoryBarrierWithGroupSync();        // expected error: only valid in compute stage
    length(inF0);                             // expect error: invalid on scalars
    msad4(inF0, float2(0), float4(0));        // expected error: only integer inputs
    normalize(inF0);                          // expect error: invalid on scalars
    reflect(inF0, inF1);                      // expect error: invalid on scalars
    refract(inF0, inF1, inF2);                // expect error: invalid on scalars
    refract(float2(0), float2(0), float2(0)); // expected error: last parameter only scalar
    reversebits(inF0);                        // expected error: only integer inputs
    transpose(inF0);                          // expect error: only valid on mats

    // TODO: texture intrinsics, when we can declare samplers.

    return 0.0;
}

float1 VertexShaderFunction(float1 inF0, float1 inF1, float1 inF2, int1 inI0)
{
    // TODO: ... add when float1 prototypes are generated

    GetRenderTargetSamplePosition(inF0); // expected error: only integer inputs

    return 0.0;
}

float2 VertexShaderFunction(float2 inF0, float2 inF1, float2 inF2, int2 inI0)
{
    uint2 out_u2;

    asdouble(inF0, inF1);                                       // expected error: only integer inputs
    CheckAccessFullyMapped(inF0);                               // expect error: only valid on scalars
    countbits(inF0);                                            // expected error: only integer inputs
    cross(inF0, inF1);                                          // expected error: only on float3 inputs
    D3DCOLORtoUBYTE4(inF0);                                     // expected error: only on float4 inputs
    ddx(inF0);                                                  // only valid in pixel stage
    ddx_coarse(inF0);                                           // only valid in pixel stage
    ddx_fine(inF0);                                             // only valid in pixel stage
    ddy(inF0);                                                  // only valid in pixel stage
    ddy_coarse(inF0);                                           // only valid in pixel stage
    ddy_fine(inF0);                                             // only valid in pixel stage
    determinant(inF0);                                          // expect error: only valid on mats
    EvaluateAttributeAtCentroid(inF0);                          // expected error: only valid in pixel stage
    EvaluateAttributeAtSample(inF0, 2);                         // expected error: only valid in pixel stage
    EvaluateAttributeSnapped(inF0, int2(2));                    // expected error: only valid in pixel stage
    f16tof32(inF0);                                             // expected error: only integer inputs
    firstbithigh(inF0);                                         // expected error: only integer inputs
    firstbitlow(inF0);                                          // expected error: only integer inputs
    fma(inF0, inF1, inF2);                                      // expected error: only double inputs
    fwidth(inF0);                                               // expected error: only valid in pixel stage
    InterlockedAdd(gs_ua2, gs_ub2);                             // expected error: only valid in pixel stage
    InterlockedAdd(gs_ua2, gs_ub2, out_u2);                     // expected error: only valid in pixel stage
    InterlockedAnd(gs_ua2, gs_ub2);                             // expected error: only valid in pixel stage
    InterlockedAnd(gs_ua2, gs_ub2, out_u2);                     // expected error: only valid in pixel stage
    InterlockedCompareExchange(gs_ua2, gs_ub2, gs_uc2, out_u2); // expected error: only valid in pixel stage
    InterlockedExchange(gs_ua2, gs_ub2, out_u2);                // expected error: only valid in pixel stage
    InterlockedMax(gs_ua2, gs_ub2);                             // expected error: only valid in pixel stage
    InterlockedMax(gs_ua2, gs_ub2, out_u2);                     // expected error: only valid in pixel stage
    InterlockedMin(gs_ua2, gs_ub2);                             // expected error: only valid in pixel stage
    InterlockedMin(gs_ua2, gs_ub2, out_u2);                     // expected error: only valid in pixel stage
    InterlockedOr(gs_ua2, gs_ub2);                              // expected error: only valid in pixel stage
    InterlockedOr(gs_ua2, gs_ub2, out_u2);                      // expected error: only valid in pixel stage
    InterlockedXor(gs_ua2, gs_ub2);                             // expected error: only valid in pixel stage
    InterlockedXor(gs_ua2, gs_ub2, out_u2);                     // expected error: only valid in pixel stage
    noise(inF0);                                                // expected error: only valid in pixel stage
    reversebits(inF0);                                          // expected error: only integer inputs
    transpose(inF0);                                            // expect error: only valid on mats

    // TODO: texture intrinsics, when we can declare samplers.

    return float2(1,2);
}

float3 VertexShaderFunction(float3 inF0, float3 inF1, float3 inF2, int3 inI0)
{
    uint3 out_u3;

    CheckAccessFullyMapped(inF0);                               // expect error: only valid on scalars
    countbits(inF0);                                            // expected error: only integer inputs
    ddx(inF0);                                                  // only valid in pixel stage
    ddx_coarse(inF0);                                           // only valid in pixel stage
    ddx_fine(inF0);                                             // only valid in pixel stage
    ddy(inF0);                                                  // only valid in pixel stage
    ddy_coarse(inF0);                                           // only valid in pixel stage
    ddy_fine(inF0);                                             // only valid in pixel stage
    D3DCOLORtoUBYTE4(inF0);                                     // expected error: only on float4 inputs
    determinant(inF0);                                          // expect error: only valid on mats
    EvaluateAttributeAtCentroid(inF0);                          // expected error: only valid in pixel stage
    EvaluateAttributeAtSample(inF0, 2);                         // expected error: only valid in pixel stage
    EvaluateAttributeSnapped(inF0, int2(2));                    // expected error: only valid in pixel stage
    f16tof32(inF0);                                             // expected error: only integer inputs
    firstbithigh(inF0);                                         // expected error: only integer inputs
    firstbitlow(inF0);                                          // expected error: only integer inputs
    fma(inF0, inF1, inF2);                                      // expected error: only double inputs
    fwidth(inF0);                                               // expected error: only valid in pixel stage
    InterlockedAdd(gs_ua3, gs_ub3);                             // expected error: only valid in pixel stage
    InterlockedAdd(gs_ua3, gs_ub3, out_u3);                     // expected error: only valid in pixel stage
    InterlockedAnd(gs_ua3, gs_ub3);                             // expected error: only valid in pixel stage
    InterlockedAnd(gs_ua3, gs_ub3, out_u3);                     // expected error: only valid in pixel stage
    InterlockedCompareExchange(gs_ua3, gs_ub3, gs_uc3, out_u3); // expected error: only valid in pixel stage
    InterlockedExchange(gs_ua3, gs_ub3, out_u3);                // expected error: only valid in pixel stage
    InterlockedMax(gs_ua3, gs_ub3);                             // expected error: only valid in pixel stage
    InterlockedMax(gs_ua3, gs_ub3, out_u3);                     // expected error: only valid in pixel stage
    InterlockedMin(gs_ua3, gs_ub3);                             // expected error: only valid in pixel stage
    InterlockedMin(gs_ua3, gs_ub3, out_u3);                     // expected error: only valid in pixel stage
    InterlockedOr(gs_ua3, gs_ub3);                              // expected error: only valid in pixel stage
    InterlockedOr(gs_ua3, gs_ub3, out_u3);                      // expected error: only valid in pixel stage
    InterlockedXor(gs_ua3, gs_ub3);                             // expected error: only valid in pixel stage
    InterlockedXor(gs_ua3, gs_ub3, out_u3);                     // expected error: only valid in pixel stage
    noise(inF0);                                                // expected error: only valid in pixel stage
    reversebits(inF0);                                          // expected error: only integer inputs
    transpose(inF0);                                            // expect error: only valid on mats

    // TODO: texture intrinsics, when we can declare samplers.

    return float3(1,2,3);
}

float4 VertexShaderFunction(float4 inF0, float4 inF1, float4 inF2, int4 inI0)
{
    uint4 out_u4;

    CheckAccessFullyMapped(inF0);                               // expect error: only valid on scalars
    countbits(inF0);                                            // expected error: only integer inputs
    cross(inF0, inF1);                                          // expected error: only on float3 inputs
    determinant(inF0);                                          // expect error: only valid on mats
    ddx(inF0);                                                  // only valid in pixel stage
    ddx_coarse(inF0);                                           // only valid in pixel stage
    ddx_fine(inF0);                                             // only valid in pixel stage
    ddy(inF0);                                                  // only valid in pixel stage
    ddy_coarse(inF0);                                           // only valid in pixel stage
    ddy_fine(inF0);                                             // only valid in pixel stage
    EvaluateAttributeAtCentroid(inF0);                          // expected error: only valid in pixel stage
    EvaluateAttributeAtSample(inF0, 2);                         // expected error: only valid in pixel stage
    EvaluateAttributeSnapped(inF0, int2(2));                    // expected error: only valid in pixel stage
    f16tof32(inF0);                                             // expected error: only integer inputs
    firstbithigh(inF0);                                         // expected error: only integer inputs
    firstbitlow(inF0);                                          // expected error: only integer inputs
    fma(inF0, inF1, inF2);                                      // expected error: only double inputs
    fwidth(inF0);                                               // expected error: only valid in pixel stage
    InterlockedAdd(gs_ua4, gs_ub4);                             // expected error: only valid in pixel stage
    InterlockedAdd(gs_ua4, gs_ub4, out_u4);                     // expected error: only valid in pixel stage
    InterlockedAnd(gs_ua4, gs_ub4);                             // expected error: only valid in pixel stage
    InterlockedAnd(gs_ua4, gs_ub4, out_u4);                     // expected error: only valid in pixel stage
    InterlockedCompareExchange(gs_ua4, gs_ub4, gs_uc4, out_u4); // expected error: only valid in pixel stage
    InterlockedExchange(gs_ua4, gs_ub4, out_u4);                // expected error: only valid in pixel stage
    InterlockedMax(gs_ua4, gs_ub4);                             // expected error: only valid in pixel stage
    InterlockedMax(gs_ua4, gs_ub4, out_u4);                     // expected error: only valid in pixel stage
    InterlockedMin(gs_ua4, gs_ub4);                             // expected error: only valid in pixel stage
    InterlockedMin(gs_ua4, gs_ub4, out_u4);                     // expected error: only valid in pixel stage
    InterlockedOr(gs_ua4, gs_ub4);                              // expected error: only valid in pixel stage
    InterlockedOr(gs_ua4, gs_ub4, out_u4);                      // expected error: only valid in pixel stage
    InterlockedXor(gs_ua4, gs_ub4);                             // expected error: only valid in pixel stage
    InterlockedXor(gs_ua4, gs_ub4, out_u4);                     // expected error: only valid in pixel stage
    noise(inF0);                                                // expected error: only valid in pixel stage
    reversebits(inF0);                                          // expected error: only integer inputs
    transpose(inF0);                                            // expect error: only valid on mats

    // TODO: texture intrinsics, when we can declare samplers.

    return float4(1,2,3,4);
}

// TODO: FXC doesn't accept this with (), but glslang doesn't accept it without.
#define MATFNS() \
    countbits(inF0);                         \
    cross(inF0, inF1);                       \
    D3DCOLORtoUBYTE4(inF0);                  \
    ddx(inF0);                               \
    ddx_coarse(inF0);                        \
    ddx_fine(inF0);                          \
    ddy(inF0);                               \
    ddy_coarse(inF0);                        \
    ddy_fine(inF0);                          \
    EvaluateAttributeAtCentroid(inF0);       \
    EvaluateAttributeAtSample(inF0, 2);      \
    EvaluateAttributeSnapped(inF0, int2(2)); \
    f16tof32(inF0);                          \
    firstbithigh(inF0);                      \
    firstbitlow(inF0);                       \
    fma(inF0, inF1, inF2);                   \
    fwidth(inF0);                            \
    noise(inF0);                             \
    reversebits(inF0);                       \
    length(inF0);                            \
    noise(inF0);                             \
    normalize(inF0);                         \
    reflect(inF0, inF1);                     \
    refract(inF0, inF1, 1.0);                \
    reversebits(inF0);                       \
    

// TODO: turn on non-square matrix tests when protos are available.

float2x2 VertexShaderFunction(float2x2 inF0, float2x2 inF1, float2x2 inF2)
{
    // TODO: FXC doesn't accept this with (), but glslang doesn't accept it without.
    MATFNS()

    return float2x2(2,2,2,2);
}

float3x3 VertexShaderFunction(float3x3 inF0, float3x3 inF1, float3x3 inF2)
{
    // TODO: FXC doesn't accept this with (), but glslang doesn't accept it without.
    MATFNS()

    return float3x3(3,3,3,3,3,3,3,3,3);
}

float4x4 VertexShaderFunction(float4x4 inF0, float4x4 inF1, float4x4 inF2)
{
    // TODO: FXC doesn't accept this with (), but glslang doesn't accept it without.
    MATFNS()

    return float4x4(4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4);
}
