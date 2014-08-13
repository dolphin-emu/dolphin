void main()
{
	float4 c0 = Sample();
	float4 c1 = SampleOffset(int2( 1,  1));
	float4 c2 = SampleOffset(int2(-1, -1));
	float red	= c0.r;
	float green	= c0.g;
	float blue	= c0.b;
	float alpha	= c0.a;

	red = c0.r - c1.b;
	blue = c0.b - c2.r + (c0.g - c0.r);

	SetOutput(float4(red, 0.0, blue, alpha));
}
