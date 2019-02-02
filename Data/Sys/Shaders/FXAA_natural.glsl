#define FXAA_REDUCE_MIN     (1.0 / 128.0)
#define FXAA_REDUCE_MUL     (1.0 / 8.0)
#define FXAA_SPAN_MAX       8.0

float3 applyFXAA(float2 fragCoord)
{
    float2 inverseVP = GetInvResolution();
    float3 rgbNW = SampleLocation((fragCoord + float2(-1.0, -1.0)) * inverseVP).xyz;
    float3 rgbNE = SampleLocation((fragCoord + float2(1.0, -1.0)) * inverseVP).xyz;
    float3 rgbSW = SampleLocation((fragCoord + float2(-1.0, 1.0)) * inverseVP).xyz;
    float3 rgbSE = SampleLocation((fragCoord + float2(1.0, 1.0)) * inverseVP).xyz;
    float3 rgbM  = SampleLocation(fragCoord  * inverseVP).xyz;
    float3 luma = float3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
                        (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
            max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
            dir * rcpDirMin)) * inverseVP;

    float3 rgbA = 0.5 * (
        SampleLocation(fragCoord * inverseVP + dir * (1.0 / 3.0 - 0.5)).xyz +
        SampleLocation(fragCoord * inverseVP + dir * (2.0 / 3.0 - 0.5)).xyz);
    float3 rgbB = rgbA * 0.5 + 0.25 * (
        SampleLocation(fragCoord * inverseVP + dir * -0.5).xyz +
        SampleLocation(fragCoord * inverseVP + dir * 0.5).xyz);

    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
        return rgbA;
    else
        return rgbB;
}

#define MUL(a, b) (b * a)
#define GIN   2.2
#define GOUT  2.2
#define Y   1.1
#define I   1.1
#define Q   1.1 

const mat3x3 RGBtoYIQ = mat3x3(0.299,     0.587,     0.114,
                      0.595716, -0.274453, -0.321263,
                      0.211456, -0.522591,  0.311135);

const mat3x3 YIQtoRGB = mat3x3(1,  0.95629572,  0.62102442,
                      1, -0.27212210, -0.64738060,
                      1, -1.10698902,  1.70461500);

const float3 YIQ_lo = float3(0, -0.595716, -0.522591);
const float3 YIQ_hi = float3(1,  0.595716,  0.522591);

float4 applyNatural(float3 c)
{
    c = pow(c, float3(GIN, GIN, GIN));
    c = MUL(RGBtoYIQ, c);
    c = float3(pow(c.x, Y), c.y * I, c.z * Q);
    c = clamp(c, YIQ_lo, YIQ_hi);
    c = MUL(YIQtoRGB, c);
    c = pow(c, float3(1.0/GOUT, 1.0/GOUT, 1.0/GOUT));
    return float4(c, 1.0);
}

void main()
{
    SetOutput(applyNatural(applyFXAA(GetCoordinates() * GetResolution())));
}
