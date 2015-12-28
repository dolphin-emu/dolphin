void main()
{
	float4 c0 = Sample();
	float red	= 0.0;
	float green	= 0.0;
	float blue	= 0.0;

	if (c0.r > 0.25)
		red = c0.r;

	if (c0.g > 0.25)
		green = c0.g;

	if (c0.b > 0.25)
		blue = c0.b;

	SetOutput(float4(red, green, blue, 1.0));
}
