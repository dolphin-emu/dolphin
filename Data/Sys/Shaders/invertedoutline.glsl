SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	float4 c0 = texture(samp9, uv0);
	float4 c1 = texture(samp9, uv0 + float2(5,5)*resolution.zw);

	ocol0 = c0 - c1;
}
