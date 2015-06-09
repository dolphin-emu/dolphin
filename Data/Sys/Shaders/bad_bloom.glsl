void main()
{
	float4 c_center = Sample();

	float4 bloom_sum = float4(0.0, 0.0, 0.0, 0.0);
	float2 pos = GetCoordinates() + float2(0.3, 0.3) * GetInvResolution();
	float2 radius1 = 1.3 * GetInvResolution();
	bloom_sum += SampleLocation(pos + float2(-1.5, -1.5) * radius1);
	bloom_sum += SampleLocation(pos + float2(-2.5, 0.0)  * radius1);
	bloom_sum += SampleLocation(pos + float2(-1.5, 1.5) * radius1);
	bloom_sum += SampleLocation(pos + float2(0.0, 2.5) * radius1);
	bloom_sum += SampleLocation(pos + float2(1.5, 1.5) * radius1);
	bloom_sum += SampleLocation(pos + float2(2.5, 0.0) * radius1);
	bloom_sum += SampleLocation(pos + float2(1.5, -1.5) * radius1);
	bloom_sum += SampleLocation(pos + float2(0.0, -2.5) * radius1);

	float2 radius2 = 4.6 * GetInvResolution();
	bloom_sum += SampleLocation(pos + float2(-1.5, -1.5) * radius2);
	bloom_sum += SampleLocation(pos + float2(-2.5, 0.0)  * radius2);
	bloom_sum += SampleLocation(pos + float2(-1.5, 1.5)  * radius2);
	bloom_sum += SampleLocation(pos + float2(0.0, 2.5)  * radius2);
	bloom_sum += SampleLocation(pos + float2(1.5, 1.5)  * radius2);
	bloom_sum += SampleLocation(pos + float2(2.5, 0.0)  * radius2);
	bloom_sum += SampleLocation(pos + float2(1.5, -1.5)  * radius2);
	bloom_sum += SampleLocation(pos + float2(0.0, -2.5)  * radius2);

	bloom_sum *= 0.07;
	bloom_sum -= float4(0.3, 0.3, 0.3, 0.3);
	bloom_sum = max(bloom_sum, float4(0.0, 0.0, 0.0, 0.0));

	float2 vpos = (GetCoordinates() - float2(0.5, 0.5)) * 2.0;
	float dist = (dot(vpos, vpos));
	dist = 1.0 - 0.4*dist;

	SetOutput((c_center * 0.7 + bloom_sum) * dist);
}
