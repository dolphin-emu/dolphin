float4 AmbientColor = float4(1, 0.5, 0, 1);

bool ff1 : SV_IsFrontFace;
float4 ff2 : packoffset(c0.y);
float4 ff3 : packoffset(c0.y) : register(ps_5_0, s[0]) ;
float4 ff4 : VPOS : packoffset(c0.y) : register(ps_5_0, s[0]) <int bambam=30;> ;

float4 ShaderFunction(float4 input) : COLOR0
{
    return input * AmbientColor;
}
