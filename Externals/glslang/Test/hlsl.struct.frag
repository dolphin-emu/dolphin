struct {
};

struct {
    bool b;
};

struct myS {
    bool b, c;
    float4 a, d;
};

myS s1;

struct {
    float4 i;
} s2;

struct {
    linear float4 a;
    nointerpolation bool b;
    noperspective centroid float1 c;
    sample centroid float2 d;
    bool ff1 : SV_IsFrontFace;
    bool ff2 : packoffset(c0.y);
    bool ff3 : packoffset(c0.y) : register(ps_5_0, s[0]) ;
    float4 ff4 : VPOS : packoffset(c0.y) : register(ps_5_0, s[0]) <int bambam=30;> ;
} s4;

float4 PixelShaderFunction(float4 input) : COLOR0
{
    struct FS {
        bool3 b3;
    } s3;

    s3 == s3;
    s2.i = s4.ff4;

    return input;
}