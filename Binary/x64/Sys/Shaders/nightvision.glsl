void main()
{
	float4 c0 = Sample();
	float green = c0.g;

	if (c0.g < 0.50)
		green = c0.r + c0.b;

	SetOutput(float4(0.0, green, 0.0, 1.0));
}
