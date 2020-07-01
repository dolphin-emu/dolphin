/*
[configuration]

[Pass]
Input0 = ColorBuffer
Input0Mode = Clamp
Input0Filter = Linear
EntryPoint = main

[/configuration]
*/

void main()
{
	float2 inv_target_res = GetInvTargetResolution();
	float2 coords = GetCoordinates();
	float4 coords0 = coords.xyyx + (float4(-0.375f, -0.125f, -0.375f, 0.125f) * inv_target_res.xyyx);
	float4 coords1 = coords.xyyx + (float4(0.375f, 0.125f, 0.375f, -0.125f) * inv_target_res.xyyx);
	float4 outputcolor = Sample();
	outputcolor += SampleLocation(coords0.xy);
	outputcolor += SampleLocation(coords0.wz);
	outputcolor += SampleLocation(coords1.xy);
	outputcolor += SampleLocation(coords1.wz);
	outputcolor *= 0.2;
	SetOutput(outputcolor);
}
