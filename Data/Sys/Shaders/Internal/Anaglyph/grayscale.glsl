// Anaglyph Red-Cyan grayscale shader

void main()
{
	float4 c0 = SampleLayer(0);
	float avg0 = (c0.r + c0.g + c0.b) / 3.0;
	float4 c1 = SampleLayer(1);
	float avg1 = (c1.r + c1.g + c1.b) / 3.0;
	SetOutput(float4(avg0, avg1, avg1, c0.a));
}
