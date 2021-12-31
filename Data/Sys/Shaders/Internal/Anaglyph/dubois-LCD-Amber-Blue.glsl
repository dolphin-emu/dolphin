// Anaglyph Amber-Blue shader based on Dubois algorithm
// Constants taken from the screenshot:
// "https://www.flickr.com/photos/e_dubois/5230654930/"
// Eric Dubois

void main()
{
	float4 c0 = SampleLayer(0);
	float4 c1 = SampleLayer(1);

	float3 lr = float3( 1.062,-0.205, 0.299);
	float3 lg = float3(-0.026, 0.908, 0.068);
	float3 lb = float3(-0.038,-0.173, 0.022);

	float3 rr = float3(-0.016,-0.123,-0.017);
	float3 rg = float3( 0.006, 0.062, 0.017);
	float3 rb = float3(-0.094,-0.185, 0.991);

	SetOutput(float4(dot(lr, c0.rgb) + dot(rr, c1.rgb), dot(lg, c0.rgb) + dot(rg, c1.rgb), dot(lb, c0.rgb) + dot(rb, c1.rgb), c0.a));
}
