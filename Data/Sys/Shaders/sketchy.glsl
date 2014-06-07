SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

uniform vec4 resolution;

void main()
{
	float4 c0 = texture(samp9, uv0).rgba;
	float4 tmp = float4(0.0, 0.0, 0.0, 0.0);
	tmp += c0 - texture(samp9, uv0 + float2(2.0, 2.0)*resolution.zw).rgba;
	tmp += c0 - texture(samp9, uv0 - float2(2.0, 2.0)*resolution.zw).rgba;
	tmp += c0 - texture(samp9, uv0 + float2(2.0, -2.0)*resolution.zw).rgba;
	tmp += c0 - texture(samp9, uv0 - float2(2.0, -2.0)*resolution.zw).rgba;
	float grey = ((0.222 * tmp.r) + (0.707 * tmp.g) + (0.071 * tmp.b));

	// get rid of the bottom line, as it is incorrect.
	if (uv0.y*resolution.y < 163.0)
		tmp = float4(1.0, 1.0, 1.0, 1.0);

	c0 = c0 + 1.0 - grey * 7.0;
	ocol0 = float4(c0.r, c0.g, c0.b, 1.0);
}
