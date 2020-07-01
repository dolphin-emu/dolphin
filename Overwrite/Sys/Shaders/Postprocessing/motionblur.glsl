/**************************Simple Motion Blur implementation**************************/
/*
[configuration]
[OptionRangeFloat]
GUIName = Motion Intesity
OptionName = A_MOTIONINTENSTY
MinValue = 0.0
MaxValue = 2.0
StepAmount = 0.01
DefaultValue = 0.5

[Pass]
EntryPoint = Motionblur
Input0=ColorBuffer
Input0Filter=Linear
Input0Mode=Clamp
Input1=Frame0
Input1Filter=Linear
Input1Mode=Clamp
Input2=Frame1
Input2Filter=Linear
Input2Mode=Clamp
Input3=Frame2
Input3Filter=Linear
Input3Mode=Clamp
[Frame]
OutputScale=0.125
Count=3
[/configuration]
*/
void Motionblur()
{
	float4 orginal = SampleInput(0);
	SetOutput(lerp(orginal, orginal * 0.8 + SampleInputBicubic(1) * 0.125 + SampleInputBicubic(2) * 0.0625 + SampleInputBicubic(3) * 0.0125, GetOption(A_MOTIONINTENSTY)));
}