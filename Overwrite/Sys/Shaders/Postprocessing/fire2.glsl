void main()
{
	float4 c0 = Sample();
	float red	= 0.0;
	float green	= 0.0;
	float blue	= 0.0;
	float avg = (c0.r + c0.g + c0.b) / 3.0;

	red = c0.r + (c0.g / 2.0) + (c0.b / 3.0);
	green = c0.r / 3.0;

	SetOutput(float4(red, green, blue, 1.0));
}
