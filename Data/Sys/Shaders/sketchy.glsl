void main()
{
	float4 c0 = Sample();
	float4 tmp = float4(0.0, 0.0, 0.0, 0.0);
	tmp += c0 - SampleOffset(int2( 2,  2));
	tmp += c0 - SampleOffset(int2(-2, -2));
	tmp += c0 - SampleOffset(int2( 2, -2));
	tmp += c0 - SampleOffset(int2(-2,  2));
	float grey = ((0.222 * tmp.r) + (0.707 * tmp.g) + (0.071 * tmp.b));

	// get rid of the bottom line, as it is incorrect.
	if (GetCoordinates().y*GetResolution().y < 163.0)
		tmp = float4(1.0, 1.0, 1.0, 1.0);

	c0 = c0 + 1.0 - grey * 7.0;
	SetOutput(float4(c0.r, c0.g, c0.b, 1.0));
}
