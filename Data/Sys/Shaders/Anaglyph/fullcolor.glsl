// Anaglyph Red-Cyan shader without compensation

void main()
{
	float4 c0 = SampleLayer(0);
	float4 c1 = SampleLayer(1);
	SetOutput(float4(c0.r, c1.gb, c0.a));
}
