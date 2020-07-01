// shadertype=hlsl
/*
SSAO GLSL shader v1.3
assembled by Martins Upitis (martinsh) (devlog-martinsh.blogspot.com)
original technique is made by Arkano22 (www.gamedev.net/topic/550699-ssao-no-halo-artifacts/)
     
changelog:
1.3 - heavy optimization - varying sample count and ao density areas based on luminance
1.2 - added fog calculation to mask AO. Minor fixes.
1.1 - added spiral sampling method from here:
(http://www.cgafaq.info/wiki/Evenly_distributed_points_on_sphere)
*/
/*
[configuration]
[OptionBool]
GUIName = SSAO
OptionName = A_SSAO_ENABLED
DefaultValue = True
ResolveAtCompilation = True

[OptionBool]
GUIName = Ambient Only
OptionName = B_SSAO_ONLY
DefaultValue = False
DependentOption = A_SSAO_ENABLED

[OptionRangeInteger]
GUIName = SSAO Quality
OptionName = C_SSAO_SAMPLES
MinValue = 16
MaxValue = 64
StepAmount = 4
DefaultValue = 16
DependentOption = A_SSAO_ENABLED
ResolveAtCompilation = True

[OptionRangeFloat]
GUIName = Filter Limit
OptionName = D_FILTER_LIMIT
MinValue = 0.001
MaxValue = 0.01
StepAmount = 0.0001
DefaultValue = 0.002
DependentOption = A_SSAO_ENABLED

[OptionBool]
GUIName = Optimize Samples
OptionName = D_SSAO_OPTIMIZE
DefaultValue = False
DependentOption = A_SSAO_ENABLED

[OptionRangeInteger]
GUIName = SSAO Min Quality
OptionName = E_SSAO_MIN_SAMPLES
MinValue = 2
MaxValue = 16
StepAmount = 1
DefaultValue = 2
DependentOption = D_SSAO_OPTIMIZE
ResolveAtCompilation = True

[OptionRangeFloat]
GUIName = AO Radius
OptionName = F_RADIUS
MinValue = 1.0
MaxValue = 8.0
StepAmount = 0.1
DefaultValue = 2.0
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = AO Clamp
OptionName = G_AO_CLAMP
MinValue = 0.001
MaxValue = 0.5
StepAmount = 0.001
DefaultValue = 0.1
DependentOption = A_SSAO_ENABLED

[OptionBool]
GUIName = Use Noise
OptionName = H_USE_NOISE
DefaultValue = False
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = Self-shadowing reduction
OptionName = I_DIFF_AREA
MinValue = 0.001
MaxValue = 0.1
StepAmount = 0.001
DefaultValue = 0.01
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = Gauss bell center
OptionName = J_GDISPLACE
MinValue = 0.05
MaxValue = 0.5
StepAmount = 0.001
DefaultValue = 0.06
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = Gauss bell width
OptionName = K_AOWIDTH
MinValue = 0.001
MaxValue = 0.1
StepAmount = 0.001
DefaultValue = 0.06
DependentOption = A_SSAO_ENABLED

[OptionBool]
GUIName = Use Fog
OptionName = L_USE_FOG
DefaultValue = False
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = Fog Start
OptionName = M_FOG_START
MinValue = 0.0
MaxValue = 100.0
StepAmount = 0.001
DefaultValue = 0.0
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = luminance effect
OptionName = N_LUM_INFLUENCE
MinValue = 0.0
MaxValue = 0.9
StepAmount = 0.001
DefaultValue = 0.8
DependentOption = A_SSAO_ENABLED

[OptionRangeFloat]
GUIName = Fog End
OptionName = N_FOG_END
MinValue = 0.1
MaxValue = 100.0
StepAmount = 0.001
DefaultValue = 100.0
DependentOption = A_SSAO_ENABLED
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
[/configuration]
*/
   
#define PI    3.14159265

     

     
     
float lumInfluence = 0.8;
//how much luminance affects occlusion
     
//--------------------------------------------------------
     
     
float doMist()
{
	float depth = SampleDepth();
	return clamp((depth-GetOption(M_FOG_START))/GetOption(N_FOG_END),0.0,1.0);
}
    
float2 rand(float2 coord) //generating noise/pattern texture for dithering
{
	float2 noiseresult = float2(0.0, 0.0);
	if (OptionEnabled(H_USE_NOISE))
	{
		noiseresult = dot(coord, float2(12.9898, 78.233)) * float2(1.0, 2.0);
		noiseresult = sin(noiseresult);
		noiseresult = frac(noiseresult * 43758.5453);
	}
	else
	{
		float2 res = GetResolution();
		res *= 0.5;
		float2 temp1 = frac(1.0 - coord*res.yx) * float2(0.25, 0.75);
		float2 temp2 = frac(coord.yx*res.xy) * float2(0.75, 0.25);
		noiseresult = temp1 + temp2;
	}
	noiseresult = (noiseresult * 4.0) - 2.0;
    return noiseresult;
}
       
float readDepth(float2 coord)
{
	return SampleDepthLocation(coord);
}
    
float compareDepths(float depth1, float depth2, inout int far)
{
	float garea = GetOption(K_AOWIDTH);
	//gauss bell width    
    float diff = (depth1 - depth2)*100.0;
	//depth difference (0-100)
	//reduce left bell width to avoid self-shadowing
	float gdisplace = GetOption(J_GDISPLACE);
	if (diff<gdisplace)
	{
		garea = GetOption(I_DIFF_AREA);
	}
	else
	{
		far = 1;
	}
	float gauss = exp(-2.0*(diff-gdisplace)*(diff-gdisplace)/(garea*garea));
	return gauss;
} 
     
float calAO(float depth,float2 d, float ao)
{
	float2 texCoord = GetCoordinates();
	float dd = GetOption(F_RADIUS)-depth;
	float temp = 0.0;
	float temp2 = 0.0;	
	float2 coord = texCoord + d * dd;
	float2 coord2 = texCoord - d * dd;
	int far = 0;
	temp = compareDepths(depth, readDepth(coord),far);
	//DEPTH EXTRAPOLATION:
	if (far > 0)
	{
		temp2 = compareDepths(readDepth(coord2),depth,far);
		temp += (1.0-temp)*temp2;
	}
	ao += temp;
	return ao;
}

float BilateralX(float depth)
{
	float limit = GetOption(D_FILTER_LIMIT);
	float count = 1.0;
	float value = SamplePrev().r;

	float Weight = min(sign(limit - abs(SampleDepthOffset(int2(-3, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(-3, 0)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(-2, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(-2, 0)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(-1, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(-1, 0)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(1, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(1, 0)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(2, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(2, 0)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(3, 0)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(3, 0)).r * Weight;
	count += Weight;
	return value / count;
}

float BilateralY(float depth)
{
	float limit = GetOption(D_FILTER_LIMIT);
	float count = 1.0;
	float value = SamplePrev().r;

	float Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, -3)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, -3)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, -2)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, -2)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, -1)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, -1)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, 1)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, 1)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, 2)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, 2)).r * Weight;
	count += Weight;
	Weight = min(sign(limit - abs(SampleDepthOffset(int2(0, 3)) - depth)) + 1.0, 1.0);
	value += SamplePrevOffset(int2(0, 3)).r * Weight;
	count += Weight;
	return value / count;
}

void BlurH()
{
	SetOutput(float4(1, 1, 1, 1) * BilateralX(SampleDepth()));
}

void Merger()
{
	float4 value = float4(1.0,1.0,1.0,1.0);
	if (!OptionEnabled(B_SSAO_ONLY))
	{
		value = Sample();
	}
#if A_SSAO_ENABLED == 1
	float blur = BilateralY(SampleDepth());
	value *= blur;
#endif
	SetOutput(value);
}
     
void SSAO(void)
{
	float2 res = GetResolution();
	float2 invres = GetInvResolution();
	float2 texCoord = GetCoordinates();
	float2 noise = rand(texCoord);
	float depth = readDepth(texCoord);
	float fog = doMist();
	float lum = dot(Sample().rgb, float3(0.299, 0.587, 0.114));
	float2 wh = (invres / clamp(depth,GetOption(G_AO_CLAMP),1.0))*noise;
	float2 pwh = float2(0.0, 0.0);
	float ao = 0.0;
	//user variables
	int samples = C_SSAO_SAMPLES;
	if (OptionEnabled(D_SSAO_OPTIMIZE))
	{
		samples = int(float(E_SSAO_MIN_SAMPLES)+clamp(1.0-(fog+pow(lum,4.0)),0.0,1.0)*(float(samples)-float(E_SSAO_MIN_SAMPLES)));
	}
	fog = 1.0 - fog;
	float dl = PI*(3.0-sqrt(5.0));
	float dz = 1.0/float(samples);
	float l = 0.0;
	float z = 1.0 - dz/2.0;
	for (int i = 0; i <= samples; i ++)
	{
		float r = sqrt(1.0-z);
		pwh = float2(cos(l), sin(l))*r*fog;		
		ao = calAO(depth,pwh*wh, ao);
		z = z - dz;
		l = l + dl;
	} 
	ao /= samples+0.1;
	ao = 1.0-ao;
	if (OptionEnabled(L_USE_FOG))
	{
		ao = lerp(1.0, ao, fog);
	}
      
	lum *= GetOption(N_LUM_INFLUENCE);
	ao = lerp(lerp(0.2,1.0,ao),1.0,lum);
	SetOutput(float4(ao,ao,ao,ao));
}

