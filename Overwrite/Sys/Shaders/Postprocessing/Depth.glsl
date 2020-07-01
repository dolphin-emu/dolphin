// Simple Depth
/*
[configuration]

[Pass]
Input0 = ColorBuffer
Input0Filter = Linear
Input0Mode = Clamp
Input1 = DepthBuffer
Input1Filter = Linear
Input1Mode = Clamp
OutputScale = 1
EntryPoint = main

[/configuration]
*/
void main()
{
	float depth = SampleDepth();
	SetOutput(float4(depth,depth,depth,1.0));
}