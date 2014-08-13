void main()
{
	float4 c0 = Sample();
	float red	= 0.0;
	float blue	= 0.0;

	if (c0.r > 0.15 && c0.b > 0.15)
	{
		blue = 0.5;
		red = 0.5;
	}

	float green = max(c0.r + c0.b, c0.g);

	SetOutput(float4(red, green, blue, 1.0));
}
