void main()
{
	float4 c0 = Sample();
	float avg = (c0.r + c0.g + c0.b) / 3.0;
	SetOutput(float4(avg, avg, avg, c0.a));
}
