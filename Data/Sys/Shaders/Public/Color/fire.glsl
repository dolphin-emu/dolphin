void main()
{
	float4 c0 = Sample();
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

	SetOutput(float4(red, green, blue, 1.0));
}
