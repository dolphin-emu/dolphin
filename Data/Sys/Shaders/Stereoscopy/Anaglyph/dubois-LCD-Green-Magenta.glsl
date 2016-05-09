// Anaglyph Green-Magenta shader based on Dubois algorithm
// Constants taken from the screenshot:
// "https://www.flickr.com/photos/e_dubois/5132528166/"
// Eric Dubois

void main()
{
	float4 c0 = SampleLayer(0);
	float4 c1 = SampleLayer(1);
	float3x3 l = float3x3(-0.062,-0.158,-0.039,
	                       0.284, 0.668, 0.143,
	                      -0.015,-0.027, 0.021);
	float3x3 r = float3x3( 0.529, 0.705, 0.024,
	                      -0.016,-0.015, 0.065,
	                       0.009, 0.075, 0.937);
	SetOutput(float4(mul(l, c0.rgb) + mul(r, c1.rgb), c0.a));
}
