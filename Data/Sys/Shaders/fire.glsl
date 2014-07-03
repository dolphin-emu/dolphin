uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);
	float red	= 0.0;
	float green	= 0.0;
	float blue	= 0.0;

	red = c0.r;

	if (c0.r > 0.0 && c0.g > c0.r)
		green = (c0.g - (c0.g - c0.r)) / 3.0;

	if (c0.b > 0.0 && c0.r < 0.25)
	{
		red = c0.b;
		green = c0.b / 3.0;
	}

	if (c0.g > 0.0 && c0.r < 0.25)
	{
		red = c0.g;
		green = c0.g / 3.0;
	}

	if (((c0.r + c0.g + c0.b) / 3.0) > 0.9)
		green = c0.r / 3.0;

	ocol0 = vec4(red, green, blue, 1.0);
}
