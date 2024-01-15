// scanlines shader by Nerboruto 2020
//
// fxaa Copyright (C) 2013 mudlord
//
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/*
[configuration]

[OptionBool]
GUIName = Sharp filter Active
OptionName = SHARP_ON
DefaultValue = true

[OptionRangeFloat]
GUIName = Sharp Amount
OptionName = SHARP_C
MinValue = 0.50
MaxValue = 1.50
StepAmount = 0.10
DefaultValue = 0.50
DependentOption = SHARP_ON

[OptionBool]
GUIName = Scanlines filter Active
OptionName = SCAN_ON
DefaultValue = true

[OptionBool]
GUIName = DoubleScanline
OptionName = SCAN_D
DefaultValue = false
DependentOption = SCAN_ON

[OptionRangeFloat]
GUIName = Scanlines Amount
OptionName = SCAN_LINES
MinValue = 0.1
MaxValue = 0.3
StepAmount = 0.01
DefaultValue = 0.2
DependentOption = SCAN_ON

[OptionRangeFloat]
GUIName = White Amount
OptionName = SCAN_W
MinValue = 0.5
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 0.85
DependentOption = SCAN_ON

[/configuration]
*/

#define FXAA_REDUCE_MIN		(1.0/ 128.0)
#define FXAA_REDUCE_MUL		(1.0 / 8.0)
#define C_LUMA float3(0.2126, 0.7152, 0.0722) //luma coefficient

float4 applyFXAA(float2 fragCoord)
{
	float4 color;
	float FXAA_SPAN_MAX=1.25*(GetResolution().x/640);
	float2 inverseVP = GetInvResolution();
	float3 rgbNW = SampleLocation((fragCoord + float2(-1.0, -1.0)) * inverseVP).xyz;
	float3 rgbNE = SampleLocation((fragCoord + float2(1.0, -1.0)) * inverseVP).xyz;
	float3 rgbSW = SampleLocation((fragCoord + float2(-1.0, 1.0)) * inverseVP).xyz;
	float3 rgbSE = SampleLocation((fragCoord + float2(1.0, 1.0)) * inverseVP).xyz;
	float3 rgbM  = SampleLocation(fragCoord  * inverseVP).xyz;
	float3 luma = float3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM  = dot(rgbM,  luma);
	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	float2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
						(0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);

	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	dir = min(float2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
			max(float2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
			dir * rcpDirMin)) * inverseVP;

	float3 rgbA = 0.5 * (
		SampleLocation(fragCoord * inverseVP + dir * (1.0 / 3.0 - 0.5)).xyz +
		SampleLocation(fragCoord * inverseVP + dir * (2.0 / 3.0 - 0.5)).xyz);
	float3 rgbB = rgbA * 0.5 + 0.25 * (
		SampleLocation(fragCoord * inverseVP + dir * -0.5).xyz +
		SampleLocation(fragCoord * inverseVP + dir * 0.5).xyz);

	float lumaB = dot(rgbB, luma);
	if ((lumaB < lumaMin) || (lumaB > lumaMax))
		color = float4(rgbA, 1.0);
	else
		color = float4(rgbB, 1.0);
	if (SHARP_ON == 1)
	{
		float3 blur = rgbNW;
		blur += rgbNE;
		blur += rgbSW;
		blur += rgbSE;
		blur *= 0.25;
		float3 sharp = color.rgb - blur;
		float sharp_luma = dot(sharp, C_LUMA * SHARP_C);
		sharp_luma = clamp(sharp_luma, -0.035, 0.035);
		color = color + sharp_luma;	
	}	
	return color;
}

void main()
{
	// fxaa pass
	float3 c1 = applyFXAA(GetCoordinates() * GetResolution()).rgb;
	if (SCAN_ON == 1)
	{
		float3 c2;
		float horzline;
		float Vpos = floor(GetCoordinates().y * GetWindowResolution().y);
		if (SCAN_D == 1.0) horzline = mod(Vpos, 1.5);
		else horzline = mod(Vpos, 2.0);
		if (horzline == 0.0) c2 = float3(SCAN_W, SCAN_W, SCAN_W);
		else c2 = float3(0.0, 0.0, 0.0);
		c1 = lerp(c1, c1 * c2 * 2.0, SCAN_LINES);
	}	
	SetOutput(float4(c1, 0.0));
}
