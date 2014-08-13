SAMPLER_BINDING(9) uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);
	float red	= 0.0;
	float green	= 0.0;
	float blue	= 0.0;

	if (c0.r < 0.50 || c0.b > 0.5)
	{
		blue = c0.r;
		red = c0.g;
	}
	else
	{
		blue = c0.r;
		green = c0.r;
	}

	ocol0 = vec4(red, green, blue, 1.0);
}
