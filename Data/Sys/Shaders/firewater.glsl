SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	float4 c0 = texture(samp9, uv0);
	float4 c1 = texture(samp9, uv0 + float2(1,1)*resolution.zw);
	float4 c2 = texture(samp9, uv0 + float2(-1,-1)*resolution.zw);
	float red	= c0.r;
	float green	= c0.g;
	float blue	= c0.b;
	float alpha	= c0.a;

	red = c0.r - c1.b;
	blue = c0.b - c2.r + (c0.g - c0.r);

	ocol0 = float4(red, 0.0, blue, alpha);
}
