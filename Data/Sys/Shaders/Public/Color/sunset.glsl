void main()
{
	float4 c0 = Sample();
	SetOutput(float4(c0.r * 1.5, c0.g, c0.b * 0.5, c0.a));
}
