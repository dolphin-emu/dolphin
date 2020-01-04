void main()
{
	float4 c0 = Sample();
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

	SetOutput(float4(red, green, blue, 1.0));
}
