// Anaglyph Amber-Blue shader based on Dubois algorithm
// Constants taken from the screenshot:
// "https://www.flickr.com/photos/e_dubois/5230654930/"
// Eric Dubois

void main()
{
	float4 c0 = SampleLayer(0);
	float4 c1 = SampleLayer(1);
	mat3 l = mat3( 1.062,-0.205, 0.299,
	              -0.026, 0.908, 0.068,
	              -0.038,-0.173, 0.022);
	mat3 r = mat3(-0.016,-0.123,-0.017,
	               0.006, 0.062, 0.017,
	              -0.094,-0.185, 0.991);
	SetOutput(float4(c0.rgb * l + c1.rgb * r, c0.a));
}
