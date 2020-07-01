// Matsodof port for Ishiiruka - Dolphin
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//Credits :: Matso, Gilcher Pascal aka Marty McFly (original port to MCFX)
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
[configuration]
[OptionBool]
GUIName = MATSO DOF
OptionName = MATSODOF
DefaultValue = True

[OptionRangeFloat]
GUIName = Focus Point
OptionName = DOF_FOCUSPOINT
MinValue = 0.0, 0.0
MaxValue = 1.0, 1.0
DefaultValue = 0.5, 0.5
StepAmount = 0.01, 0.01
DependentOption = MATSODOF

[OptionRangeFloat]
GUIName = Near Blue Curve
OptionName = DOF_NEARBLURCURVE
MinValue = 0.4
MaxValue = 2.0
DefaultValue = 0.7
StepAmount = 0.01
DependentOption = MATSODOF

[OptionRangeFloat]
GUIName = FarBlue Curve
OptionName = DOF_FARBLURCURVE
MinValue = 0.4
MaxValue = 2.0
DefaultValue = 1.5
StepAmount = 0.01
DependentOption = MATSODOF

[OptionRangeFloat]
GUIName = Blur Radius
OptionName = DOF_BLURRADIUS
MinValue = 5.0
MaxValue = 50.0
DefaultValue = 5.0
StepAmount = 1.0
DependentOption = MATSODOF

[OptionRangeInteger]
GUIName = Vignette
OptionName = DOF_VIGNETTE
MinValue = 0
MaxValue = 1000
DefaultValue = 400
StepAmount = 10
DependentOption = MATSODOF

[OptionBool]
GUIName = CROMA Enable
OptionName = bMatsoDOFChromaEnable
DefaultValue = True
DependentOption = MATSODOF

[OptionRangeFloat]
GUIName = Chroma Pow
OptionName = fMatsoDOFChromaPow
MinValue = 0.2
MaxValue = 3.0
DefaultValue = 1.4
StepAmount = 0.01
DependentOption = MATSODOF

[OptionRangeFloat]
GUIName = Bokeh Curve
OptionName = fMatsoDOFBokehCurve
MinValue = 0.5
MaxValue = 20.0
DefaultValue = 8.0
StepAmount = 0.01
DependentOption = MATSODOF

[OptionRangeFloat]
GUIName = Bokeh Light
OptionName = fMatsoDOFBokehLight
MinValue = 0.0
MaxValue = 2.0
DefaultValue = 0.012
StepAmount = 0.001
DependentOption = MATSODOF

[OptionRangeInteger]
GUIName = Bokeh Quality
OptionName = iMatsoDOFBokehQuality
MinValue = 1
MaxValue = 10
DefaultValue = 4
StepAmount = 1
DependentOption = MATSODOF
ResolveAtCompilation = True

[OptionRangeInteger]
GUIName = Bokeh Angle
OptionName = fMatsoDOFBokehAngle
MinValue = 0
MaxValue = 360
DefaultValue = 0
StepAmount = 1
DependentOption = MATSODOF

[Pass]
EntryPoint = PS_DOF_MatsoDOF1
Input0 = ColorBuffer
Input0Filter = Linear
Input0Mode = Clamp
Input1 = DepthBuffer
Input1Filter = Nearest
Input1Mode = Clamp
[Pass]
EntryPoint = PS_DOF_MatsoDOF2
Input0 = PreviousPass
Input0Filter = Linear
Input0Mode = Clamp
Input1 = DepthBuffer
Input1Filter = Nearest
Input1Mode = Clamp
[Pass]
EntryPoint = PS_DOF_MatsoDOF3
Input0 = PreviousPass
Input0Filter = Linear
Input0Mode = Clamp
Input1 = DepthBuffer
Input1Filter = Nearest
Input1Mode = Clamp
[Pass]
EntryPoint = PS_DOF_MatsoDOF4
Input0 = PreviousPass
Input0Filter = Linear
Input0Mode = Clamp
Input1 = DepthBuffer
Input1Filter = Nearest
Input1Mode = Clamp

[/configuration]

#define PI 		3.1415972
#define PIOVER180 	0.017453292


float4 GetMatsoDOFCA(float2 tex, float CoC)
{
	float3 ChromaPOW = float3(1,1,1) * GetOption(fMatsoDOFChromaPow) * CoC;
	float3 chroma = pow(float3(0.5, 1.0, 1.5), ChromaPOW) * 0.5;
	tex = (2.0 * tex - 1.0);
	float2 tr = (tex * chroma.r) + 0.5;
	float2 tg = (tex * chroma.g) + 0.5;
	float2 tb = (tex * chroma.b) + 0.5;
	
	float3 color = float3(SamplePrevLocation(tr).r, SamplePrevLocation(tg).g, SamplePrevLocation(tb).b) * (1.0 - CoC);
	
	return float4(color, 1.0);
}

float4 GetMatsoDOFBlur(int axis, float2 coord)
{
	float4 tcol = SamplePrevLocation(coord);
	float scenedepth = SampleDepth();
	float scenefocus =  SampleDepthLocation(GetOption(DOF_FOCUSPOINT));
	
	float depthdiff = abs(scenedepth-scenefocus);
	depthdiff = (scenedepth < scenefocus) ? pow(depthdiff, GetOption(DOF_NEARBLURCURVE))*(1.0f+pow(abs(0.5f-coord.x)*depthdiff+0.1f,2.0)*GetOption(DOF_VIGNETTE)) : depthdiff;
	depthdiff = (scenedepth > scenefocus) ? pow(depthdiff, GetOption(DOF_FARBLURCURVE)) : depthdiff;	

	float2 discRadius = depthdiff * GetOption(DOF_BLURRADIUS)*GetInvResolution()*0.5/float(iMatsoDOFBokehQuality);

	int passnumber=1;

	float sf = 0;

	float2 tdirs[4] = { float2(-0.306, 0.739), float2(0.306, 0.739), float2(-0.739, 0.306), float2(-0.739, -0.306) };
	float wValue = (1.0 + pow(length(tcol.rgb) + 0.1, GetOption(fMatsoDOFBokehCurve))) * (1.0 - GetOption(fMatsoDOFBokehLight));	// special recipe from papa Matso ;)

	for(int i = -iMatsoDOFBokehQuality; i < iMatsoDOFBokehQuality; i++)
	{
		float2 taxis =  tdirs[axis];

		taxis.x = cos(GetOption(fMatsoDOFBokehAngle)*PIOVER180)*taxis.x-sin(GetOption(fMatsoDOFBokehAngle)*PIOVER180)*taxis.y;
		taxis.y = sin(GetOption(fMatsoDOFBokehAngle)*PIOVER180)*taxis.x+cos(GetOption(fMatsoDOFBokehAngle)*PIOVER180)*taxis.y;
		
		float2 tdir = float(i) * taxis * discRadius;
		float2 tcoord = coord.xy + tdir.xy;
		float4 ct;
		if(OptionEnabled(bMatsoDOFChromaEnable))
		{
			ct = GetMatsoDOFCA(tcoord.xy, discRadius.x);
		}
		else
		{
			ct = SamplePrevLocation(tcoord.xy);
		}
		float w = 0.0;
		float b = dot(ct.rgb,float3(0.333,0.333,0.333)) + length(ct.rgb) + 0.1;
		w = pow(b, GetOption(fMatsoDOFBokehCurve)) + abs(float(i));
		tcol += ct * w;
		wValue += w;
	}

	tcol /= wValue;

	return float4(tcol.xyz, 1.0);
}

void PS_DOF_MatsoDOF1()
{
	SetOutput(GetMatsoDOFBlur(2, GetCoordinates()));
}

void PS_DOF_MatsoDOF2()
{
	SetOutput(GetMatsoDOFBlur(3, GetCoordinates()));
}

void PS_DOF_MatsoDOF3()
{
	SetOutput(GetMatsoDOFBlur(0, GetCoordinates()));	
}

void PS_DOF_MatsoDOF4()
{
	SetOutput(GetMatsoDOFBlur(1, GetCoordinates()));	
}