uniform sampler2D samp9;

out vec4 ocol0;
in vec2 uv0;

void main()
{
	vec4 c0 = texture(samp9, uv0);
	float red = c0.r;
	float blue = c0.b;
	float green = c0.g;
	float factor = 2.0;
	float max = 0.8;
	float min = 0.3;

	if (c0.r > c0.g && c0.b > c0.g)
	{
		if (c0.r < c0.b + 0.05 && c0.b < c0.r + 0.05)
		{
			red = 0.7;
			blue = 0.7;
			green = 0.05;
		}
		else if (c0.r > c0.b + 0.05)
		{
			red = 0.7;
			blue = 0.05;
			green = 0.05;
		}
		else if (c0.b > c0.r + 0.05)
		{
			red = 0.05;
			blue = 0.7;
			green = 0.05;
		}
	}

	if (c0.r > c0.b && c0.g > c0.b)
	{
		if (c0.r < c0.g + 0.05 && c0.g < c0.r + 0.05)
		{
			red = 0.7;
			blue = 0.05;
			green = 0.7;
		}
		else if (c0.r > c0.g + 0.05)
		{
			red = 0.7;
			blue = 0.05;
			green = 0.05;
		}
		else if (c0.g > c0.r + 0.05)
		{
			red = 0.05;
			blue = 0.05;
			green = 0.7;
		}
	}

	if (c0.g > c0.r && c0.b > c0.r)
	{
		if (c0.g < c0.b + 0.05 && c0.b < c0.g + 0.05)
		{
			red = 0.05;
			blue = 0.7;
			green = 0.7;
		}
		else if (c0.g > c0.b + 0.05)
		{
			red = 0.05;
			blue = 0.05;
			green = 0.7;
		}
		else if (c0.b > c0.g + 0.05)
		{
			red = 0.05;
			blue = 0.7;
			green = 0.05;
		}
	}
	ocol0 = vec4(red, green, blue, c0.a);
}
