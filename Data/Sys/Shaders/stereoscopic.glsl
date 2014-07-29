// Omega's 3D Stereoscopic filtering
// TODO: Need depth info!

void main()
{
	// Source Color
	float4 c0 = Sample();
	const int sep = 5;
	float red     = c0.r;
	float green   = c0.g;
	float blue    = c0.b;

	// Left Eye (Red)
	float4 c1 = SampleOffset(int2(sep, 0));
	red = max(c0.r, c1.r);

	// Right Eye (Cyan)
	float4 c2 = SampleOffset(int2(-sep, 0));
	float cyan = (c2.g + c2.b) / 2.0;
	green = max(c0.g, cyan);
	blue = max(c0.b, cyan);

	SetOutput(float4(red, green, blue, c0.a));
}
