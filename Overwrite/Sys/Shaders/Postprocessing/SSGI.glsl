// Simple SSGI

/*
[configuration]
[OptionBool]
GUIName = SSGI
OptionName = A_SSGI_ENABLED
DefaultValue = True
ResolveAtCompilation = True

[OptionRangeInteger]
GUIName = SSGI Samples
OptionName = iSSGISamples
MinValue = 5
MaxValue = 24
StepAmount = 1
DefaultValue = 10
DependentOption = A_SSGI_ENABLED
ResolveAtCompilation = True

[OptionRangeFloat]
GUIName = Sample Range
OptionName = fSSGISamplingRange
MinValue = 5.0
MaxValue = 80.0
StepAmount = 0.1
DefaultValue = 40.0
DependentOption = A_SSGI_ENABLED

[OptionRangeFloat]
GUIName = Ilumination Multiplier
OptionName = fSSGIIlluminationMult
MinValue = 1.0
MaxValue = 8.0
StepAmount = 0.1
DefaultValue = 4.0
DependentOption = A_SSGI_ENABLED

[OptionRangeFloat]
GUIName = Occlusion Multiplier
OptionName = fSSGIOcclusionMult
MinValue = 1.0
MaxValue = 10.0
StepAmount = 0.1
DefaultValue = 2.0
DependentOption = A_SSGI_ENABLED

[OptionRangeFloat]
GUIName = Model Thickness
OptionName = fSSGIModelThickness
MinValue = 0.5
MaxValue = 100.0
StepAmount = 0.1
DefaultValue = 40.0
DependentOption = A_SSGI_ENABLED

[OptionRangeFloat]
GUIName = Saturation
OptionName = fSSGISaturation
MinValue = 0.2
MaxValue = 2.0
StepAmount = 0.01
DefaultValue = 1.5
DependentOption = A_SSGI_ENABLED

[OptionRangeFloat]
GUIName = AO Sharpness
OptionName = AO_SHARPNESS
MinValue = 0.1
MaxValue = 5.0
StepAmount = 0.01
DefaultValue = 0.75
DependentOption = A_SSGI_ENABLED

[OptionBool]
GUIName = Ambient Only
OptionName = A_AO_ONLY
DefaultValue = False
DependentOption = A_SSGI_ENABLED

[OptionBool]
GUIName = Ilumination Only
OptionName = A_ILUMINATION_ONLY
DefaultValue = False
DependentOption = A_SSGI_ENABLED

[Pass]
EntryPoint = PS_AO_SSGI
DependantOption = A_SSGI_ENABLED
Input0=ColorBuffer
Input0Filter=Linear
Input0Mode=Clamp
Input1=DepthBuffer
Input1Filter=Nearest
Input1Mode=Clamp
[Pass]
EntryPoint = PS_AO_BlurV
DependantOption = A_SSGI_ENABLED
Input0=PreviousPass
Input0Filter=Linear
Input0Mode=Clamp
Input1=DepthBuffer
Input1Filter=Nearest
Input1Mode=Clamp
[Pass]
EntryPoint = PS_AO_GICombine
Input0=ColorBuffer
Input0Filter=Linear
Input0Mode=Clamp
Input1=DepthBuffer
Input1Filter=Nearest
Input1Mode=Clamp
Input2=PreviousPass
Input2Filter=Linear
Input2Mode=Clamp
[/configuration]
*/

float3 GetEyePosition(float2 uv, float eye_z, float2 InvFocalLen) {
	uv = (uv * float2(2.0, -2.0) - float2(1.0, -1.0));
	float3 pos = float3(uv * InvFocalLen * eye_z, eye_z);
	return pos;
}

float2 GetRandom2_10(float2 uv) {
	float noiseX = (frac(sin(dot(uv, float2(12.9898, 78.233) * 2.0)) * 43758.5453));
	float noiseY = sqrt(1 - noiseX * noiseX);
	return float2(noiseX, noiseY);
}

void PS_AO_SSGI()
{
	float depth = SampleDepth();
	float2 texcoord = GetCoordinates();
	float4 Occlusion1R = float4(0.0, 0.0, 0.0, 1.0);
	if (depth < 0.9999) 
	{
		float2 sample_offset[24] =
		{
			float2(-0.1376476f,  0.2842022f),float2(-0.626618f ,  0.4594115f),
			float2(-0.8903138f, -0.05865424f),float2(0.2871419f,  0.8511679f),
			float2(-0.1525251f, -0.3870117f),float2(0.6978705f, -0.2176773f),
			float2(0.7343006f,  0.3774331f),float2(0.1408805f, -0.88915f),
			float2(-0.6642616f, -0.543601f),float2(-0.324815f, -0.093939f),
			float2(-0.1208579f , 0.9152063f),float2(-0.4528152f, -0.9659424f),
			float2(-0.6059740f,  0.7719080f),float2(-0.6886246f, -0.5380305f),
			float2(0.5380307f, -0.2176773f),float2(0.7343006f,  0.9999345f),
			float2(-0.9976073f, -0.7969264f),float2(-0.5775355f,  0.2842022f),
			float2(-0.626618f ,  0.9115176f),float2(-0.29818942f, -0.0865424f),
			float2(0.9161239f,  0.8511679f),float2(-0.1525251f, -0.07103951f),
			float2(0.7022788f, -0.823825f),float2(0.60250657f,  0.64525909f)
		};

		float sample_radius[24] =
		{
			0.5162497,0.2443335,
			0.1014819,0.1574599,
			0.6538922,0.5637644,
			0.6347278,0.2467654,
			0.5642318,0.0035689,
			0.6384532,0.3956547,
			0.7049623,0.3482861,
			0.7484038,0.2304858,
			0.0043161,0.5423726,
			0.5025704,0.4066662,
			0.2654198,0.8865175,
			0.9505567,0.9936577
		};

		float2 InvFocalLen = float2(1.0, tan(0.5235987756));
		float aspect = GetInvResolution().y * GetResolution().x;
		InvFocalLen.x = InvFocalLen.y * aspect;
		float3 pos = GetEyePosition(texcoord.xy, depth, InvFocalLen);
		float3 dx = ddx(pos);
		float3 dy = ddy(pos);
		float3 norm = normalize(cross(dx, dy));
		norm.y *= -1;		

		float4 gi = float4(0.0, 0.0, 0.0, 0.0);
		float is = 0.0, as = 0.0;

		float rangeZ = 5000.0;

		
		
		
		float2 rand_vec = GetRandom2_10(texcoord.xy);
		float2 rand_vec2 = GetRandom2_10(-texcoord.xy);
		float2 sample_vec_divisor = InvFocalLen * depth / (GetOption(fSSGISamplingRange) * GetInvResolution().xy);
		float2 sample_center = texcoord.xy + norm.xy / sample_vec_divisor * float2(1, aspect);
		float ii_sample_center_depth = depth * rangeZ + norm.z * GetOption(fSSGISamplingRange) * 20;
		float ao_sample_center_depth = depth * rangeZ + norm.z * GetOption(fSSGISamplingRange) * 5;

		

		for (int i = 0; i < iSSGISamples; i++) 
		{
			float2 sample_vec = reflect(sample_offset[i], rand_vec) / sample_vec_divisor;
			float2 sample_coords = sample_center + sample_vec *  float2(1, aspect);
			float  sample_depth = rangeZ * SampleDepthLocation(sample_coords);

			float ii_curr_sample_radius = sample_radius[i] * GetOption(fSSGISamplingRange) * 20;
			float ao_curr_sample_radius = sample_radius[i] * GetOption(fSSGISamplingRange) * 5;

			gi.a += clamp(0, ao_sample_center_depth + ao_curr_sample_radius - sample_depth, 2 * ao_curr_sample_radius);
			gi.a -= clamp(0, ao_sample_center_depth + ao_curr_sample_radius - sample_depth - GetOption(fSSGIModelThickness), 2 * ao_curr_sample_radius);

			if ((sample_depth < ii_sample_center_depth + ii_curr_sample_radius) &&
				(sample_depth > ii_sample_center_depth - ii_curr_sample_radius)) {
				gi.rgb += SampleLocation(sample_coords).rgb;
			}

			is += 1.0f;
			as += 2.0f * ao_curr_sample_radius;
		}

		gi.rgb /= is * 5.0f;
		gi.a /= as;

		gi.rgb = 0.0 + gi.rgb * GetOption(fSSGIIlluminationMult);
		gi.a = 1.0 - gi.a   * GetOption(fSSGIOcclusionMult);

		gi.rgb = lerp(dot(gi.rgb, float3(0.333, 0.333, 0.333)) * float3(1.0,1.0,1.0), gi.rgb, GetOption(fSSGISaturation));

		Occlusion1R = gi;
	}
	SetOutput(Occlusion1R);
}

float4 PS_AO_Blur(float2 axis)
{
	float gaussweight[7] = { 0.111220, 0.107798, 0.098151, 0.083953, 0.067458, 0.050920, 0.036108 };
	float4 sum = float4(0.0, 0.0, 0.0, 0.0);
	float totalweight = 0.0;
	float4 base = SamplePrev();
	float4 temp = sum;

	float centerdepth = SampleDepth();
	float2 texcoord = GetCoordinates();
	axis *= GetInvResolution();
	for (int r = -6; r <= 6; ++r)
	{
		float2 coord = texcoord.xy + axis * r;
		temp = SamplePrevLocation(coord);
		float tempdepth = SampleDepthLocation(coord);
		float weight = 0.3 + gaussweight[abs(r)];
		weight *= max(0.0, 1.0 - (1000.0 * GetOption(AO_SHARPNESS)) * abs(tempdepth - centerdepth));
		sum += temp * weight;
		totalweight += weight;
	}

	return sum / (totalweight + 0.0001);
}

void PS_AO_BlurV()
{
	float2 axis = float2(0, 1);
	SetOutput(PS_AO_Blur(axis));
}

void PS_AO_GICombine()
{
	float4 color = Sample();
#if A_SSGI_ENABLED == 1
	float2 axis = float2(1, 0);
	float4 gi = PS_AO_Blur(axis);

	color.xyz = (gi.w + gi.xyz)*color.xyz;
	if (OptionEnabled(A_AO_ONLY))
	{
		color = gi.wwww;
	}
	else if (OptionEnabled(A_ILUMINATION_ONLY))
	{
		color = gi.xyzz;
	}
#endif
	SetOutput(color);
}