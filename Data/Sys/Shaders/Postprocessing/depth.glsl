[configuration]

[Pass]
Input0=DepthBuffer
Input0Filter=Linear
OutputScale=1
EntryPoint=main

[/configuration]

void main()
{
	float intensity = SampleDepth();
	SetOutput(float4(intensity.rrr, 1.0f));
}
