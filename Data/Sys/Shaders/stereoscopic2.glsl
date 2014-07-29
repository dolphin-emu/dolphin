// Omega's 3D Stereoscopic filtering (Amber/Blue)
// TODO: Need depth info!

void main()
{
	// Source Color
	float4 c0 = Sample();
	float sep = 5.0;
	float red   = c0.r;
	float green = c0.g;
	float blue  = c0.b;

	// Left Eye (Amber)
	float4 c2 = SampleLocation(GetCoordinates() + float2(sep,0.0)*GetInvResolution()).rgba;
	float amber = (c2.r + c2.g) / 2.0;
	red = max(c0.r, amber);
	green = max(c0.g, amber);

	// Right Eye (Blue)
	float4 c1 = SampleLocation(GetCoordinates() + float2(-sep,0.0)*GetInvResolution()).rgba;
	blue = max(c0.b, c1.b);

	SetOutput(float4(red, green, blue, c0.a));
}
