void main()
{
	float4 c0 = Sample();
	float4 c1 = SampleOffset(int2(5, 5));

	SetOutput(c0 - c1);
}
