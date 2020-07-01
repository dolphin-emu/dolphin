void main()
{
	float4 c0   = Sample();
	float4 c1   = SampleOffset(int2(5, 5));
	float y     = (0.222 * c1.r) + (0.707 * c1.g) + (0.071 * c1.b);
	float y2    = ((0.222 * c0.r) + (0.707 * c0.g) + (0.071 * c0.b)) / 3.0;
	float red   = c0.r;
	float green = c0.g;
	float blue  = c0.b;
	float alpha = c0.a;

	red	= y2 + (1.0 - y);
	green	= y2 + (1.0 - y);
	blue	= y2 + (1.0 - y);

	SetOutput(float4(red, green, blue, alpha));
}
