void main()
{
	float4 c0 = Sample();
	float red	= 0.0;
	float green	= 0.0;

	if (c0.r < 0.35 || c0.b > 0.35)
		green = c0.g + (c0.b / 2.0);
	else
		red = c0.r + 0.4;

	SetOutput(float4(red, green, 0.0, 1.0));
}
