void main()
{
	float4 c0 = Sample();
	float4 c1 = SampleOffset(int2(-1,  0));
	float4 c2 = SampleOffset(int2( 0, -1));
	float4 c3 = SampleOffset(int2( 1,  0));
	float4 c4 = SampleOffset(int2( 0,  1));

	float red = c0.r;
	float blue = c0.b;
	float green = c0.g;

	float red2 = (c1.r + c2.r + c3.r + c4.r) / 4.0;
	float blue2 = (c1.b + c2.b + c3.b + c4.b) / 4.0;
	float green2 = (c1.g + c2.g + c3.g + c4.g) / 4.0;

	if (red2 > 0.3)
		red = c0.r + c0.r / 2.0;
	else
		red = c0.r - c0.r / 2.0;

	if (green2 > 0.3)
		green = c0.g+ c0.g / 2.0;
	else
		green = c0.g - c0.g / 2.0;

	if (blue2  > 0.3)
		blue = c0.b+ c0.b / 2.0;
	else
		blue = c0.b - c0.b / 2.0;

	SetOutput(float4(red, green, blue, c0.a));
}
