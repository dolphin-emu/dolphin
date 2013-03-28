uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);
	float red   = 0.0;
	float green = 0.0;
	float blue  = 0.0;

	if (c0.r < 0.35 || c0.b > 0.5)
	{
		red = c0.g + c0.b;
	}
	else
	{
		red = c0.g + c0.b;
		blue = c0.r + c0.b;
		green = c0.r + c0.b;
	}

	ocol0 = vec4(red, green, blue, 1.0);
}
