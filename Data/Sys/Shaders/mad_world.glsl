SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	float4 emboss = (texture(samp9, uv0+resolution.zw) - texture(samp9, uv0-resolution.zw))*2.0;
	emboss -= (texture(samp9, uv0+float2(1,-1)*resolution.zw).rgba - texture(samp9, uv0+float2(-1,1)*resolution.zw).rgba);
	float4 color = texture(samp9, uv0).rgba;

	if (color.r > 0.8 && color.b + color.b < 0.2)
	{
		ocol0 = float4(1,0,0,0);
	}
	else
	{
		color += emboss;
		if (dot(color.rgb, float3(0.3, 0.5, 0.2)) > 0.5)
			ocol0 = float4(1,1,1,1);
		else
			ocol0 = float4(0,0,0,0);
	}
}
