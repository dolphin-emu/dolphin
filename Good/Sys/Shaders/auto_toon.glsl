void main()
{
	float4 to_gray = float4(0.3,0.59,0.11,0);

	float x1 = dot(to_gray, SampleOffset(int2( 1, 1)));
	float x0 = dot(to_gray, SampleOffset(int2(-1,-1)));
	float x3 = dot(to_gray, SampleOffset(int2( 1,-1)));
	float x2 = dot(to_gray, SampleOffset(int2(-1, 1)));

	float edge = (x1 - x0) * (x1 - x0) + (x3 - x2) * (x3 - x2);

	float4 color = Sample();

	SetOutput(color - float4(edge, edge, edge, edge) * 12.0);
}
