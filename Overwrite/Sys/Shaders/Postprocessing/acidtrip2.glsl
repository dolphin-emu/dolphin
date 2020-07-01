void main()
{
	float4 a = SampleOffset(int2( 1,  1));
	float4 b = SampleOffset(int2(-1, -1));
	SetOutput(( a*a*1.3 - b ) * 8.0);
}
