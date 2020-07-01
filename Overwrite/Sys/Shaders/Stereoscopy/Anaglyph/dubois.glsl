// Anaglyph Red-Cyan shader based on Dubois algorithm
// Constants taken from the paper:
// "Conversion of a Stereo Pair to Anaglyph with
// the Least-Squares Projection Method"
// Eric Dubois, March 2009

void main()
{
	float4 c0 = ApplyGCGamma(SampleLayer(0));
	float4 c1 = ApplyGCGamma(SampleLayer(1));
	SetOutput(float4(pow(0.7 * c0.g + 0.3 * c0.b, 1.5), c1.gba));
}
