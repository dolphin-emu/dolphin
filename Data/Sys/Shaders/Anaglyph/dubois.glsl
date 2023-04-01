// Anaglyph Red-Cyan shader based on Dubois algorithm
// Constants taken from the paper:
// "Conversion of a Stereo Pair to Anaglyph with
// the Least-Squares Projection Method"
// Eric Dubois, March 2009

void main()
{
	float4 c0 = SampleLayer(0);
	float4 c1 = SampleLayer(1);
	mat3 l = mat3( 0.437, 0.449, 0.164,
	              -0.062,-0.062,-0.024,
	              -0.048,-0.050,-0.017);
	mat3 r = mat3(-0.011,-0.032,-0.007,
	               0.377, 0.761, 0.009,
	              -0.026,-0.093, 1.234);
	SetOutput(float4(c0.rgb * l + c1.rgb * r, c0.a));
}
