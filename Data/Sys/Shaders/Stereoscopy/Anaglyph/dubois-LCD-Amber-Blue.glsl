// Anaglyph Amber-Blue shader based on Dubois algorithm
// Constants taken from the screenshot:
// "https://www.flickr.com/photos/e_dubois/5230654930/"
// Eric Dubois

void main()
{
	float4 c0 = SampleLayer(0);
	float4 c1 = SampleLayer(1);
	float3x3 l = float3x3( 1.062,-0.205, 0.299,
	                      -0.026, 0.908, 0.068,
	                      -0.038,-0.173, 0.022);
	float3x3 r = float3x3(-0.016,-0.123,-0.017,
	                       0.006, 0.062, 0.017,
	                      -0.094,-0.185, 0.991);
	SetOutput(float4(mul(l, c0.rgb) + mul(r, c1.rgb), c0.a));
}
