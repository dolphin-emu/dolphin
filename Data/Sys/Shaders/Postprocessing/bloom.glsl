[configuration]

[OptionRangeFloat]
GUIName = Bloom Threshold
OptionName = BLOOM_THRESHOLD
MinValue = 0.1
MaxValue = 1.0
DefaultValue = 0.65
StepAmount = 0.05

[OptionRangeInteger]
GUIName = Blur Radius
OptionName = BLUR_RADIUS
MinValue = 3
MaxValue = 9
StepAmount = 2
DefaultValue = 5
ResolveAtCompilation = true

[OptionRangeFloat]
GUIName = Blur Spread
OptionName = BLUR_SPREAD
MinValue = 0.25
MaxValue = 5
StepAmount = 0.25
DefaultValue = 2.00
ResolveAtCompilation = true

[OptionBool]
GUIName = Multipass Downscale
OptionName = MULTIPASS_DOWNSCALE
DefaultValue = true
ResolveAtCompilation = true

[OptionBool]
GUIName = Show Bloom
OptionName = SHOW_BLOOM
DefaultValue = false
ResolveAtCompilation = true

[Pass]
Input0 = ColorBuffer
Input0Filter = Linear
Input0Mode = Clamp
EntryPoint = Bloom

[Pass]
Input0 = PreviousPass
Input0Filter = Linear
Input0Mode = Clamp
OutputScaleNative = 0.5
DependantOption = MULTIPASS_DOWNSCALE

[Pass]
Input0 = PreviousPass
Input0Filter = Linear
Input0Mode = Clamp
OutputScaleNative = 0.25
DependantOption = MULTIPASS_DOWNSCALE

[Pass]
Input0 = PreviousPass
Input0Filter = Linear
Input0Mode = Clamp
OutputScaleNative = 0.125

[Pass]
Input0 = PreviousPass
Input0Filter = Linear
Input0Mode = Clamp
EntryPoint = BlurH
OutputScaleNative = 0.125

[Pass]
Input0 = PreviousPass
Input0Filter = Linear
Input0Mode = Clamp
EntryPoint = BlurV
OutputScaleNative = 0.125

[Pass]
Input0 = ColorBuffer
Input0Filter = Linear
Input0Mode = Clamp
Input1 = PreviousPass
Input1Filter = Linear
Input1Mode = Clamp
EntryPoint = Apply

[/configuration]

void Bloom()
{
	float bloomThreshold = GetOption(BLOOM_THRESHOLD);
	float3 inColor = Sample().rgb;
	float3 outColor = saturate((inColor - bloomThreshold.rrr) / (1.0f - bloomThreshold).rrr);
	SetOutput(float4(outColor, 1.0f));
}

void Blur(float2 blurDirection)
{
	int blurRadius = GetOption(BLUR_RADIUS);
	float blurSpread = GetOption(BLUR_SPREAD);

	float2 texelSize = GetInvInputResolution(0);
	float2 baseTexCoord = GetCoordinates();
	float3 outColor = float3(0.0f, 0.0f, 0.0f);

	float g = 1.0f / sqrt(2.0f * 3.14159265359f * blurSpread * blurSpread);
	for (int i = -blurRadius; i < blurRadius; i++)
	{
		float weight = g * exp(-float(i * i) / (2.0f * blurSpread * blurSpread));
		float2 texCoord = baseTexCoord + (texelSize * blurDirection) * float(i);
		outColor += SampleInputLocation(0, texCoord).rgb * weight;
	}

	SetOutput(float4(outColor, 1.0f));
}

void BlurH()
{
	Blur(float2(1.0f, 0.0f));
}

void BlurV()
{
	Blur(float2(0.0f, 1.0f));
}

void Apply()
{
	float4 inColor = SampleInput(0);
	float4 inBloom = SampleInput(1);
	if (OptionEnabled(SHOW_BLOOM))
		SetOutput(float4(inBloom.rgb, inColor.a));
	else
		SetOutput(float4(inColor.rgb + inBloom.rgb, inColor.a));
}
