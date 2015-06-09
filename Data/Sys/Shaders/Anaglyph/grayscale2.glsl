// Anaglyph Red-Cyan luma grayscale shader
// Info: http://www.oreillynet.com/cs/user/view/cs_msg/8691

void main()
{
	float3 luma = float3(0.222, 0.707, 0.071);
	float4 c0 = SampleLayer(0);
	float avg0 = dot(c0.rgb, luma);
	float4 c1 = SampleLayer(1);
	float avg1 = dot(c1.rgb, luma);
	SetOutput(float4(avg0, avg1, avg1, c0.a));
}
