// Anaglyph Green-Magenta shader based on Dubois algorithm
// Constants taken from the screenshot:
// "https://www.flickr.com/photos/e_dubois/5132528166/"
// Eric Dubois

void main()
{
	float4 c0 = SampleLayer(0);
	float4 c1 = SampleLayer(1);

	float3 lr = float3(-0.062,-0.158,-0.039);
	float3 lg = float3( 0.284, 0.668, 0.143);
	float3 lb = float3(-0.015,-0.027, 0.021);

	float3 rr = float3( 0.529, 0.705, 0.024);
	float3 rg = float3(-0.016,-0.015, 0.065);
	float3 rb = float3( 0.009, 0.075, 0.937);

	SetOutput(float4(dot(lr, c0.rgb) + dot(rr, c1.rgb), dot(lg, c0.rgb) + dot(rg, c1.rgb), dot(lb, c0.rgb) + dot(rb, c1.rgb), c0.a));
}
