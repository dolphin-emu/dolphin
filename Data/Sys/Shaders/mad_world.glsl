void main()
{
	float4 emboss = (SampleLocation(GetCoordinates()+GetInvResolution()) - SampleLocation(GetCoordinates()-GetInvResolution()))*2.0;
	emboss -= (SampleLocation(GetCoordinates()+float2(1,-1)*GetInvResolution()).rgba - SampleLocation(GetCoordinates()+float2(-1,1)*GetInvResolution()).rgba);
	float4 color = Sample();

	if (color.r > 0.8 && color.b + color.b < 0.2)
	{
		SetOutput(float4(1,0,0,0));
	}
	else
	{
		color += emboss;
		if (dot(color.rgb, float3(0.3, 0.5, 0.2)) > 0.5)
			SetOutput(float4(1,1,1,1));
		else
			SetOutput(float4(0,0,0,0));
	}
}
