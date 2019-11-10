// Anaglyph Red-Cyan shader based on Dubois algorithm
// Constants taken from the paper:
// "Conversion of a Stereo Pair to Anaglyph with
// the Least-Squares Projection Method"
// Eric Dubois, March 2009

void main()
{
	float4 c0 = SampleLayer(0);
	float4 c1 = SampleLayer(1);

	float3 lr = float3( 0.437, 0.449, 0.164);
	float3 lg = float3(-0.062,-0.062,-0.024);
	float3 lb = float3(-0.048,-0.050,-0.017);

	float3 rr = float3(-0.011,-0.032,-0.007);
	float3 rg = float3( 0.377, 0.761, 0.009);
	float3 rb = float3(-0.026,-0.093, 1.234);

	SetOutput(float4(dot(lr, c0.rgb) + dot(rr, c1.rgb), dot(lg, c0.rgb) + dot(rg, c1.rgb), dot(lb, c0.rgb) + dot(rb, c1.rgb), c0.a));
}
