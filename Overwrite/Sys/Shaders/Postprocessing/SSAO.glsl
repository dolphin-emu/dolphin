// Simple SSAO

/*
[configuration]
[OptionBool]
GUIName = SSAO
OptionName = A_SSAO_ENABLED
DefaultValue = True
ResolveAtCompilation = True

[OptionBool]
GUIName = SSGI
OptionName = A_SSGI_ENABLED
DefaultValue = false
ResolveAtCompilation = True
DependentOption = A_SSAO_ENABLED

[OptionBool]
GUIName = Ambient Only
OptionName = A_SSAO_ONLY
DefaultValue = False
DependentOption = A_SSAO_ENABLED

[OptionRangeInteger]
GUIName = SSAO Quality
OptionName = B_SSAO_SAMPLES
MinValue = 16
MaxValue = 64
StepAmount = 4
DefaultValue = 16
DependentOption = A_SSAO_ENABLED
ResolveAtCompilation = True

[OptionRangeFloat]
GUIName = Sample Range
OptionName = C_SAMPLE_RANGE
MinValue = 0.001
MaxValue = 0.05
StepAmount = 0.0001
DefaultValue = 0.01
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = Filter Limit
OptionName = D_FILTER_LIMIT
MinValue = 0.001
MaxValue = 0.01
StepAmount = 0.0001
DefaultValue = 0.002
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = Max Depth
OptionName = E_MAX_DEPTH
MinValue = 0.0001
MaxValue = 0.03
StepAmount = 0.0001
DefaultValue = 0.015
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = Min Depth
OptionName = F_MIN_DEPTH
MinValue = 0.0
MaxValue = 0.02
StepAmount = 0.0001
DefaultValue = 0.0002
DependentOption = A_SSAO_ENABLED

[OptionBool]
GUIName = Depth Of Field
OptionName = G_DOF
DefaultValue = false
[OptionRangeFloat]
GUIName = Blur Radius
OptionName = H_BLUR
MinValue = 1.0
MaxValue = 6.0
DefaultValue = 1.0
StepAmount = 0.01
DependentOption = G_DOF
[OptionRangeFloat]
GUIName = Focus Position
OptionName = I_FOCUS_POS
MinValue = 0.0, 0.0
MaxValue = 1.0, 1.0
DefaultValue = 0.5, 0.5
StepAmount = 0.01, 0.01
DependentOption = G_DOF
[Pass]
EntryPoint = SSAO
DependantOption = A_SSAO_ENABLED
Input0=ColorBuffer
Input0Filter=Linear
Input0Mode=Clamp
Input1=DepthBuffer
Input1Filter=Nearest
Input1Mode=Clamp
[Pass]
EntryPoint = BlurH
DependantOption = A_SSAO_ENABLED
Input0=PreviousPass
Input0Filter=Linear
Input0Mode=Clamp
Input1=DepthBuffer
Input1Filter=Nearest
Input1Mode=Clamp
[Pass]
EntryPoint = Merger
Input0=ColorBuffer
Input0Filter=Linear
Input0Mode=Clamp
Input1=DepthBuffer
Input1Filter=Nearest
Input1Mode=Clamp
Input2=PreviousPass
Input2Filter=Linear
Input2Mode=Clamp
[Pass]
EntryPoint = DOF
DependantOption = G_DOF
Input0=PreviousPass
Input0Filter=Linear
Input0Mode=Clamp
Input1=DepthBuffer
Input1Filter=Nearest
Input1Mode=Clamp
[/configuration]
*/
float3 GetNormalFromDepth(float fDepth) 
{ 
  	float depth1 = SampleDepthOffset(int2(0, 1));
  	float depth2 = SampleDepthOffset(int2(1, 0));
	float2 invres = GetInvResolution();
  	float3 p1 = float3(0,invres.y, depth1 - fDepth);
  	float3 p2 = float3(invres.x, 0, depth2 - fDepth);
  
  	float3 normal = cross(p1, p2);
  	normal.z = -normal.z;
  
  	return normalize(normal);
}

float4 BilateralX(float depth)
{
	float limit = GetOption(D_FILTER_LIMIT);
	float count = 1.0;
	float4 value = SamplePrev();

	float Weight = min(sign(limit - abs(SampleDepthOffset(int2(-3, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(-3, 0)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(-2, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(-2, 0)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(-1, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(-1, 0)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(1, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(1, 0)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(2, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(2, 0)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(3, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(3, 0)) * Weight;
	count += Weight;
	return value / count;
}

float4 BilateralY(float depth)
{
	float limit = GetOption(D_FILTER_LIMIT);
	float count = 1.0;
	float4 value = SamplePrev();

	float Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, -3)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, -3)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, -2)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, -2)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, -1)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, -1)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, 1)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, 1)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, 2)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, 2)) * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, 3)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, 3)) * Weight;
	count += Weight;
	return value / count;
}

void BlurH()
{
	SetOutput(BilateralX(SampleDepth()));
}

void Merger()
{
	float4 value = float4(1.0,1.0,1.0,1.0);
	if (!OptionEnabled(A_SSAO_ONLY))
	{
		value = Sample();
	}
#if A_SSAO_ENABLED == 1
	float4 blur = BilateralY(SampleDepth());
	value.rgb = (1 + blur.rgb) * blur.a * value.rgb;
#endif
	SetOutput(value);
}

void DOF()
{
	float4 value = SamplePrev();
	if(OptionEnabled(G_DOF))
	{
		float2 coords = GetCoordinates();
		float4 color = SamplePrev();
		float4 blur = color;
		float2 unit = GetInvResolution() * GetOption(H_BLUR);
		blur += SamplePrevLocation(coords + unit * float2(-1.50, -0.66));
		blur += SamplePrevLocation(coords + unit * float2(0.66, -1.50));
		blur += SamplePrevLocation(coords + unit * float2(1.50, 0.66));
		blur += SamplePrevLocation(coords + unit * float2(-0.66, 1.50));
		
		float depth = SampleDepth();
		float focusDepth = SampleDepthLocation(GetOption(I_FOCUS_POS).xy);
		depth = clamp(abs((depth - focusDepth) / depth), 0.0, 1.0);
		value = lerp(color, blur * 0.2, depth);
	}
	SetOutput(value);
}

void SSAO()
{
	float3 PoissonDisc[] = {
		float3(-0.367046f, 0.692618f, 0.0136723f),
		float3(0.262978f, -0.363506f, 0.231819f),
		float3(-0.734306f, -0.451643f, 0.264779f),
		float3(-0.532456f, 0.683096f, 0.552049f),
		float3(0.672536f, 0.283731f, 0.0694296f),
		float3(-0.194678f, 0.548204f, 0.56859f),
		float3(-0.87347f, -0.572741f, 0.923795f),
		float3(0.548936f, -0.717277f, 0.0201727f),
		float3(0.48381f, 0.691397f, 0.699088f),
		float3(-0.592273f, 0.41966f, 0.413953f),
		float3(-0.448042f, -0.957396f, 0.123234f),
		float3(-0.618458f, 0.112949f, 0.412946f),
		float3(-0.412763f, 0.122227f, 0.732078f),
		float3(0.816462f, -0.900815f, 0.741417f),
		float3(-0.0381787f, 0.511521f, 0.799768f),
		float3(-0.688284f, 0.310099f, 0.472732f),
		float3(-0.368023f, 0.720572f, 0.544206f),
		float3(-0.379192f, -0.55504f, 0.035371f),
		float3(0.15482f, 0.0353709f, 0.543779f),
		float3(0.153417f, -0.521409f, 0.943724f),
		float3(-0.168371f, -0.702933f, 0.145665f),
		float3(-0.673391f, -0.925657f, 0.61391f),
		float3(-0.479171f, -0.131993f, 0.659932f),
		float3(0.0549638f, -0.470809f, 0.420759f),
		float3(0.899594f, 0.955077f, 0.54857f),
		float3(-0.230689f, 0.660573f, 0.548112f),
		float3(0.0421461f, -0.19895f, 0.121799f),
		float3(-0.229774f, -0.30137f, 0.507492f),
		float3(-0.983642f, 0.468551f, 0.0393994f),
		float3(-0.00857568f, 0.440657f, 0.337046f),
		float3(0.730461f, -0.283914f, 0.789941f),
		float3(0.271828f, -0.226356f, 0.317026f),
		float3(-0.178869f, -0.946837f, 0.073336f),
		float3(0.389813f, -0.110508f, 0.0549944f),
		float3(0.0242622f, 0.893613f, 0.26957f),
		float3(-0.857601f, 0.0219429f, 0.45146f),
		float3(-0.15659f, 0.550401f, 3.05185e-005f),
		float3(0.0555742f, -0.354656f, 0.573412f),
		float3(-0.267373f, 0.117466f, 0.488571f),
		float3(-0.533799f, -0.431928f, 0.226661f),
		float3(0.49852f, -0.750908f, 0.412427f),
		float3(-0.300882f, 0.366314f, 0.558245f),
		float3(-0.176f, 0.511521f, 0.722465f),
		float3(-0.0514847f, -0.703543f, 0.180273f),
		float3(-0.429914f, 0.0774255f, 0.161534f),
		float3(-0.416791f, -0.788385f, 0.328135f),
		float3(0.127293f, -0.115146f, 0.958586f),
		float3(-0.34959f, -0.278481f, 0.168706f),
		float3(-0.645192f, 0.168798f, 0.577105f),
		float3(-0.190771f, -0.622669f, 0.257851f),
		float3(0.718986f, -0.275369f, 0.602039f),
		float3(-0.444258f, -0.872982f, 0.0275582f),
		float3(0.793512f, 0.0511185f, 0.33964f),
		float3(-0.143651f, 0.155614f, 0.368877f),
		float3(-0.777093f, 0.246864f, 0.290628f),
		float3(0.202979f, -0.61742f, 0.233802f),
		float3(0.198523f, 0.425153f, 0.409162f),
		float3(-0.629688f, 0.597461f, 0.120212f),
		float3(0.0448316f, -0.689566f, 0.0241707f),
		float3(-0.190039f, 0.426496f, 0.254463f),
		float3(-0.255776f, 0.722831f, 0.527451f),
		float3(-0.821528f, 0.303751f, 0.140172f),
		float3(0.696646f, 0.168981f, 0.404492f),
		float3(-0.240211f, -0.109653f, 0.463301f),
	};

	const float2 rndNorm[] =
	{
		 float2(0.505277f, 0.862957f),
		 float2(-0.554562f, 0.832142f),
		 float2(0.663051f, 0.748574f),
		 float2(-0.584629f, -0.811301f),
		 float2(-0.702343f, 0.711838f),
		 float2(0.843108f, -0.537744f),
		 float2(0.85856f, 0.512713f),
		 float2(0.506966f, -0.861966f),
		 float2(0.614758f, -0.788716f),
		 float2(0.993426f, -0.114472f),
		 float2(-0.676375f, 0.736558f),
		 float2(-0.891668f, 0.45269f),
		 float2(0.226367f, 0.974042f),
		 float2(-0.853615f, -0.520904f),
		 float2(0.467359f, 0.884067f),
		 float2(-0.997111f, 0.0759529f),
	};

	float2 coords = GetCoordinates();
	float fCurrDepth = SampleDepth();
	float4 Occlusion = float4(0.0, 0.0, 0.0, 1.0);
	if(fCurrDepth<0.9999) 
	{
		float sample_range = GetOption(C_SAMPLE_RANGE) * fCurrDepth;
		float3 vViewNormal = GetNormalFromDepth(fCurrDepth);
		Randomize();
		uint2 fragcoord = uint2(GetFragmentCoord()) & 3;
		uint rndidx = fragcoord.y * 4 + fragcoord.x;
		float3 vRandom = float3(rndNorm[rndidx], 0);
		float fAO = 0;
		const int NUMSAMPLES = B_SSAO_SAMPLES;
		for (int s = 0; s < NUMSAMPLES; s++)
		{
			float3 offset = PoissonDisc[s];
			float3 vReflRay = reflect(offset, vRandom);
		
			float fFlip = sign(dot(vViewNormal,vReflRay));
        	vReflRay   *= fFlip;
		
			float sD = fCurrDepth - (vReflRay.z * sample_range);
			float2 location = saturate(coords + (sample_range * vReflRay.xy / fCurrDepth));
			float fSampleDepth = SampleDepthLocation(location);
			float fDepthDelta = saturate(sD - fSampleDepth);

			fDepthDelta *= 1-smoothstep(0,GetOption(E_MAX_DEPTH),fDepthDelta);

			if (fDepthDelta > GetOption(F_MIN_DEPTH) && fDepthDelta < GetOption(E_MAX_DEPTH))
			{
				fAO += pow(1 - fDepthDelta, 2.5);
#if A_SSGI_ENABLED == 1
				Occlusion.rgb += SampleLocation(location).rgb;
#endif
			}

		}
		Occlusion.a = 1.0 - (fAO / float(NUMSAMPLES));
#if A_SSGI_ENABLED == 1
		Occlusion.rgb = Occlusion.rgb / float(NUMSAMPLES);
#endif
		Occlusion = saturate(Occlusion);
	}
	SetOutput(Occlusion);
}