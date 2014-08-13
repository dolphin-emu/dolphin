SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	float4 c0 = texture(samp9, uv0).rgba;
	float green = c0.g;

	if (c0.g < 0.50)
		green = c0.r + c0.b;

	ocol0 = float4(0.0, green, 0.0, 1.0);
}
