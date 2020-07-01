/*
[configuration]

[OptionRangeFloat]
GUIName = Natural-Vision - Color Boost
OptionName = COLOR_BOOST
MinValue = 0.8
MaxValue = 1.6
StepAmount = 0.05
DefaultValue = 1.2

[Pass]
Input0 = ColorBuffer
Input0Mode = Clamp
Input0Filter = Linear
EntryPoint = main

[/configuration]
*/

/*
   ShadX's Natural Vision Shader

   Ported and tweaked by Hyllian - 2016

*/

void main()
{
	const float3x3 RGBtoYIQ = float3x3(0.299, 0.596, 0.212, 
                                           0.587,-0.275,-0.523, 
                                           0.114,-0.321, 0.311);

	const float3x3 YIQtoRGB = float3x3(1.0, 1.0, 1.0,
                                           0.95568806036115671171,-0.27158179694405859326,-1.1081773266826619523,
                                           0.61985809445637075388,-0.64687381613840131330, 1.7050645599191817149);

	float cb = GetOption(COLOR_BOOST);

	float3 val00 = float3( cb, cb, cb);

	float3 c0,c1;

	c0 = Sample().xyz;
	c1 = mul(c0,RGBtoYIQ);
	c1 = float3(pow(c1.x,val00.x),c1.yz*val00.yz);

	SetOutput(float4(mul(c1,YIQtoRGB), 1.0));
}
