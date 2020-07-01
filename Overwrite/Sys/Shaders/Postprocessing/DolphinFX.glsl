/*===============================================================================*\
|########################     [Dolphin FX Suite 2.20]      #######################|
|##########################        By Asmodean          ##########################|
||                                                                               ||
||          This program is free software; you can redistribute it and/or        ||
||          modify it under the terms of the GNU General Public License          ||
||          as published by the Free Software Foundation; either version 2       ||
||          of the License, or (at your option) any later version.               ||
||                                                                               ||
||          This program is distributed in the hope that it will be useful,      ||
||          but WITHOUT ANY WARRANTY; without even the implied warranty of       ||
||          MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        ||
||          GNU General Public License for more details. (C)2015                 ||
||                                                                               ||
|#################################################################################|
\*===============================================================================*/

/*
[configuration]

[OptionBool]
GUIName = Disable All Effects (To reset all settings to default, delete them from dolphin.ini)
OptionName = DISABLE_EFFECTS
DefaultValue = false

[OptionBool]
GUIName = Scaling Filters
OptionName = A_SCALERS
DefaultValue = false

[OptionRangeInteger]
GUIName = Bilinear Filtering
OptionName = A_BILINEAR_FILTER
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = A_SCALERS

[OptionRangeInteger]
GUIName = Bicubic Scaling
OptionName = B_BICUBLIC_SCALER
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = A_SCALERS

[OptionRangeInteger]
GUIName = Lanczos Scaling
OptionName = B_LANCZOS_SCALER
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = A_SCALERS

[OptionBool]
GUIName = Note: Use FXAA or a Scaling Filter, not both together.
OptionName = PASS_WARN
DefaultValue = true
DependentOption = A_SCALERS

[OptionBool]
GUIName = FXAA
OptionName = A_FXAA_PASS
DefaultValue = false

[OptionRangeFloat]
GUIName = SubpixelMax
OptionName = A_FXAA_SUBPIX_MAX
MinValue = 0.00
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.25
DependentOption = A_FXAA_PASS

[OptionRangeFloat]
GUIName = EdgeThreshold
OptionName = B_FXAA_EDGE_THRESHOLD
MinValue = 0.010
MaxValue = 0.500
StepAmount = 0.001
DefaultValue = 0.050
DependentOption = A_FXAA_PASS

[OptionRangeInteger]
GUIName = ShowEdgeDetection
OptionName = C_FXAA_SHOW_EDGES
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = A_FXAA_PASS

[OptionBool]
GUIName = Blended Bloom
OptionName = B_BLOOM_PASS
DefaultValue = true

[OptionRangeInteger]
GUIName = BloomType
OptionName = A_BLOOM_TYPE
MinValue = 0
MaxValue = 5
StepAmount = 1
DefaultValue = 0
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomStrength
OptionName = B_BLOOM_STRENGTH
MinValue = 0.000
MaxValue = 1.000
StepAmount = 0.001
DefaultValue = 0.220
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BlendStrength
OptionName = C_BLEND_STRENGTH
MinValue = 0.000
MaxValue = 1.200
StepAmount = 0.010
DefaultValue = 1.000
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomDefocus
OptionName = D_B_DEFOCUS
MinValue = 1.000
MaxValue = 4.000
StepAmount = 0.100
DefaultValue = 2.000
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomWidth
OptionName = D_BLOOM_WIDTH
MinValue = 1.000
MaxValue = 8.000
StepAmount = 0.100
DefaultValue = 3.200
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomReds
OptionName = E_BLOOM_REDS
MinValue = 0.000
MaxValue = 0.500
StepAmount = 0.001
DefaultValue = 0.020
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomGreens
OptionName = F_BLOOM_GREENS
MinValue = 0.000
MaxValue = 0.500
StepAmount = 0.001
DefaultValue = 0.010
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomBlues
OptionName = G_BLOOM_BLUES
MinValue = 0.000
MaxValue = 0.500
StepAmount = 0.001
DefaultValue = 0.010
DependentOption = B_BLOOM_PASS

[OptionBool]
GUIName = Scene Tonemapping
OptionName = C_TONEMAP_PASS
DefaultValue = true

[OptionRangeInteger]
GUIName = TonemapType
OptionName = A_TONEMAP_TYPE
MinValue = 0
MaxValue = 3
StepAmount = 1
DefaultValue = 1
DependentOption = C_TONEMAP_PASS

[OptionRangeInteger]
GUIName = FilmOperator
OptionName = A_TONEMAP_FILM
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 1
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = ToneAmount
OptionName = B_TONE_AMOUNT
MinValue = 0.05
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 0.30
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = FilmStrength
OptionName = B_TONE_FAMOUNT
MinValue = 0.00
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.25
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = BlackLevels
OptionName = C_BLACK_LEVELS
MinValue = 0.00
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.06
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = Exposure
OptionName = D_EXPOSURE
MinValue = 0.50
MaxValue = 1.50
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = Luminance
OptionName = E_LUMINANCE
MinValue = 0.50
MaxValue = 1.50
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = WhitePoint
OptionName = F_WHITEPOINT
MinValue = 0.50
MaxValue = 1.50
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = C_TONEMAP_PASS

[OptionBool]
GUIName = Colour Correction
OptionName = D_COLOR_CORRECTION
DefaultValue = true

[OptionRangeInteger]
GUIName = CorrectionPalette
OptionName = A_PALETTE
MinValue = 0
MaxValue = 4
StepAmount = 1
DefaultValue = 2
DependentOption = D_COLOR_CORRECTION

[OptionRangeFloat]
GUIName = Channels R|Y|X|H|Y
OptionName = B_RED_CORRECTION
MinValue = 0.10
MaxValue = 8.00
StepAmount = 0.10
DefaultValue = 1.50
DependentOption = D_COLOR_CORRECTION

[OptionRangeFloat]
GUIName = Channels G|X|Y|S|U
OptionName = C_GREEN_CORRECTION
MinValue = 0.10
MaxValue = 8.00
StepAmount = 0.10
DefaultValue = 1.50
DependentOption = D_COLOR_CORRECTION

[OptionRangeFloat]
GUIName = Channels B|Y|Z|V|V
OptionName = D_BLUE_CORRECTION
MinValue = 0.10
MaxValue = 8.00
StepAmount = 0.10
DefaultValue = 3.00
DependentOption = D_COLOR_CORRECTION

[OptionRangeFloat]
GUIName = PaletteStrength
OptionName = E_CORRECT_STR
MinValue = 0.00
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = D_COLOR_CORRECTION

[OptionBool]
GUIName = Filmic Process
OptionName = E_FILMIC_PROCESS
DefaultValue = false

[OptionRangeInteger]
GUIName = FilmicProcess
OptionName = A_FILMIC
MinValue = 0
MaxValue = 2
StepAmount = 1
DefaultValue = 0
DependentOption = E_FILMIC_PROCESS

[OptionRangeFloat]
GUIName = RedShift
OptionName = B_RED_SHIFT
MinValue = 0.10
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.55
DependentOption = E_FILMIC_PROCESS

[OptionRangeFloat]
GUIName = GreenShift
OptionName = C_GREEN_SHIFT
MinValue = 0.10
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.50
DependentOption = E_FILMIC_PROCESS

[OptionRangeFloat]
GUIName = BlueShift
OptionName = D_BLUE_SHIFT
MinValue = 0.10
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.50
DependentOption = E_FILMIC_PROCESS

[OptionRangeFloat]
GUIName = ShiftRatio
OptionName = E_SHIFT_RATIO
MinValue = 0.00
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 0.50
DependentOption = E_FILMIC_PROCESS

[OptionBool]
GUIName = Gamma Correction
OptionName = F_GAMMA_CORRECTION
DefaultValue = false

[OptionRangeFloat]
GUIName = Gamma
OptionName = A_GAMMA
MinValue = 1.00
MaxValue = 4.00
StepAmount = 0.01
DefaultValue = 2.20
DependentOption = F_GAMMA_CORRECTION

[OptionBool]
GUIName = Texture Sharpen
OptionName = G_TEXTURE_SHARPEN
DefaultValue = false

[OptionRangeFloat]
GUIName = SharpenStrength
OptionName = A_SHARPEN_STRENGTH
MinValue = 0.00
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 0.75
DependentOption = G_TEXTURE_SHARPEN

[OptionRangeFloat]
GUIName = SharpenClamp
OptionName = B_SHARPEN_CLAMP
MinValue = 0.005
MaxValue = 0.250
StepAmount = 0.001
DefaultValue = 0.012
DependentOption = G_TEXTURE_SHARPEN

[OptionRangeFloat]
GUIName = SharpenBias
OptionName = C_SHARPEN_BIAS
MinValue = 1.00
MaxValue = 4.00
StepAmount = 0.05
DefaultValue = 1.00
DependentOption = G_TEXTURE_SHARPEN

[OptionRangeInteger]
GUIName = ShowEdgeMask
OptionName = D_SEDGE_DETECTION
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = G_TEXTURE_SHARPEN

[OptionBool]
GUIName = Pixel Vibrance
OptionName = H_PIXEL_VIBRANCE
DefaultValue = false

[OptionRangeFloat]
GUIName = Vibrance
OptionName = A_VIBRANCE
MinValue = -0.50
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.15
DependentOption = H_PIXEL_VIBRANCE

[OptionRangeFloat]
GUIName = RedVibrance
OptionName = B_R_VIBRANCE
MinValue = -1.00
MaxValue = 4.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = H_PIXEL_VIBRANCE

[OptionRangeFloat]
GUIName = GreenVibrance
OptionName = C_G_VIBRANCE
MinValue = -1.00
MaxValue = 4.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = H_PIXEL_VIBRANCE

[OptionRangeFloat]
GUIName = BlueVibrance
OptionName = D_B_VIBRANCE
MinValue = -1.00
MaxValue = 4.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = H_PIXEL_VIBRANCE

[OptionBool]
GUIName = Contrast Enhancement
OptionName = I_CONTRAST_ENHANCEMENT
DefaultValue = false

[OptionRangeFloat]
GUIName = Contrast
OptionName = A_CONTRAST
MinValue = 0.00
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.35
DependentOption = I_CONTRAST_ENHANCEMENT

[OptionBool]
GUIName = Cel Shading
OptionName = J_CEL_SHADING
DefaultValue = false

[OptionRangeFloat]
GUIName = EdgeStrength
OptionName = A_EDGE_STRENGTH
MinValue = 0.00
MaxValue = 4.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = J_CEL_SHADING

[OptionRangeFloat]
GUIName = EdgeFilter
OptionName = B_EDGE_FILTER
MinValue = 0.25
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.60
DependentOption = J_CEL_SHADING

[OptionRangeFloat]
GUIName = EdgeThickness
OptionName = C_EDGE_THICKNESS
MinValue = 0.25
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = J_CEL_SHADING

[OptionRangeInteger]
GUIName = PaletteType
OptionName = D_PALETTE_TYPE
MinValue = 0
MaxValue = 2
StepAmount = 1
DefaultValue = 1
DependentOption = J_CEL_SHADING

[OptionRangeInteger]
GUIName = UseYuvLuma
OptionName = E_YUV_LUMA
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = J_CEL_SHADING

[OptionRangeInteger]
GUIName = ColourRounding
OptionName = G_COLOR_ROUNDING
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 1
DependentOption = J_CEL_SHADING

[OptionBool]
GUIName = Paint Shading
OptionName = J_PAINT_SHADING
DefaultValue = false

[OptionRangeInteger]
GUIName = Paint method
GUIDescription = The algorithm used for paint effect. 1: water painting, 0: oil painting. You may want to readjust the radius between the two.
OptionName = PaintMethod
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = J_PAINT_SHADING

[OptionRangeInteger]
GUIName = Paint radius
GUIDescription = Radius of the painted effect, increasing the radius also requires longer loops, so higher values require more performance.
OptionName = PaintRadius
MinValue = 2
MaxValue = 8
StepAmount = 1
DefaultValue = 4
ResolveAtCompilation = True
DependentOption = J_PAINT_SHADING

[OptionRangeFloat]
GUIName = Paint radius
GUIDescription = The overall interpolated strength of the paint effect. Where 1.0 equates to 100% strength.
OptionName = PaintStrength
MinValue = 0.0
MaxValue = 1.0
StepAmount = 0.01
DefaultValue = 1.0
DependentOption = J_PAINT_SHADING

[OptionBool]
GUIName = Scanlines
OptionName = K_SCAN_LINES
DefaultValue = false

[OptionRangeInteger]
GUIName = ScanlineType
OptionName = A_SCANLINE_TYPE
MinValue = 0
MaxValue = 2
StepAmount = 1
DefaultValue = 0
DependentOption = K_SCAN_LINES

[OptionRangeFloat]
GUIName = ScanlineIntensity
OptionName = B_SCANLINE_INTENSITY
MinValue = 0.15
MaxValue = 0.30
StepAmount = 0.01
DefaultValue = 0.18
DependentOption = K_SCAN_LINES

[OptionRangeFloat]
GUIName = ScanlineThickness
OptionName = B_SCANLINE_THICKNESS
MinValue = 0.20
MaxValue = 0.80
StepAmount = 0.01
DefaultValue = 0.50
DependentOption = K_SCAN_LINES

[OptionRangeFloat]
GUIName = ScanlineBrightness
OptionName = B_SCANLINE_BRIGHTNESS
MinValue = 0.50
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 1.10
DependentOption = K_SCAN_LINES

[OptionRangeFloat]
GUIName = ScanlineSpacing
OptionName = B_SCANLINE_SPACING
MinValue = 0.10
MaxValue = 0.99
StepAmount = 0.01
DefaultValue = 0.25
DependentOption = K_SCAN_LINES

[OptionBool]
GUIName = Film Grain
OptionName = L_FILM_GRAIN_PASS
DefaultValue = false

[OptionRangeFloat]
GUIName = GrainSize
OptionName = A_GRAIN_SIZE
MinValue = 1.50
MaxValue = 2.50
StepAmount = 0.10
DefaultValue = 1.60
DependentOption = L_FILM_GRAIN_PASS

[OptionRangeFloat]
GUIName = GrainAmount
OptionName = B_GRAIN_AMOUNT
MinValue = 0.01
MaxValue = 0.50
StepAmount = 0.01
DefaultValue = 0.05
DependentOption = L_FILM_GRAIN_PASS

[OptionRangeInteger]
GUIName = ColouredGrain
OptionName = C_COLORED
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 1
DependentOption = L_FILM_GRAIN_PASS

[OptionRangeFloat]
GUIName = ColourAmount
OptionName = D_COLOR_AMOUNT
MinValue = 0.10
MaxValue = 1.00
StepAmount = 0.10
DefaultValue = 0.60
DependentOption = L_FILM_GRAIN_PASS

[OptionRangeFloat]
GUIName = LumaFilter
OptionName = E_LUMA_AMOUNT
MinValue = 0.10
MaxValue = 1.00
StepAmount = 0.10
DefaultValue = 1.00
DependentOption = L_FILM_GRAIN_PASS

[OptionBool]
GUIName = Vignette
OptionName = L_VIGNETTE_PASS
DefaultValue = false

[OptionRangeFloat]
GUIName = VignetteRatio
OptionName = A_VIG_RATIO
MinValue = 1.00
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 1.50
DependentOption = L_VIGNETTE_PASS

[OptionRangeFloat]
GUIName = VignetteRadius
OptionName = B_VIG_RADIUS
MinValue = 0.50
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = L_VIGNETTE_PASS

[OptionRangeFloat]
GUIName = VignetteAmount
OptionName = C_VIG_AMOUNT
MinValue = 0.10
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 0.25
DependentOption = L_VIGNETTE_PASS

[OptionRangeInteger]
GUIName = VignetteSlope
OptionName = D_VIG_SLOPE
MinValue = 2
MaxValue = 16
StepAmount = 2
DefaultValue = 12
DependentOption = L_VIGNETTE_PASS

[OptionBool]
GUIName = Dithering
OptionName = M_DITHER_PASS
DefaultValue = false

[OptionRangeInteger]
GUIName = DitherType
OptionName = A_DITHER_TYPE
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = M_DITHER_PASS

[OptionRangeInteger]
GUIName = ShowMethod
OptionName = B_DITHER_SHOW
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = M_DITHER_PASS

[OptionBool]
GUIName = Pixel Border
OptionName = N_PIXEL_BORDER
DefaultValue = false

[OptionRangeFloat]
GUIName = Border Width
OptionName = BorderWidth
MinValue = 0.0, 0.0
MaxValue = 1024.0, 1024.0
StepAmount = 1.0, 1.0
DefaultValue = 2.0, 2.0
DependentOption = N_PIXEL_BORDER

[OptionRangeFloat]
GUIName = Border Color
OptionName = BorderColor
MinValue = 0.0, 0.0, 0.0
MaxValue = 1.0, 1.0, 1.0
StepAmount = 0.01, 0.01, 0.01
DefaultValue = 0.0, 0.0, 0.0
DependentOption = N_PIXEL_BORDER

[/configuration]
*/

/*------------------------------------------------------------------------------
[GLOBALS|FUNCTIONS]
------------------------------------------------------------------------------*/

#define FIX(c) max(abs(c), 1e-5)

#define Epsilon (1e-10)
#define lumCoeff float3(0.2126729, 0.7151522, 0.0721750)

float ColorLuminance(float3 color)
{
	return dot(color, lumCoeff);
}

//Average relative luminance
float AvgLuminance(float3 color)
{
	return sqrt(dot(color * color, lumCoeff));
}

float smootherstep(float a, float b, float x)
{
	x = saturate((x - a) / (b - a));
	return x*x*x*(x*(x * 6.0 - 15.0) + 10.0);
}

/*
float4 DebugClipping(float4 color)
{
if (color.x >= 0.99999 && color.y >= 0.99999 &&
color.z >= 0.99999) color.xyz = float3(1.0f, 0.0f, 0.0f);

if (color.x <= 0.00001 && color.y <= 0.00001 &&
color.z <= 0.00001) color.xyz = float3(0.0f, 0.0f, 1.0f);

return color;
}
*/

//Conversion matrices
float3 RGBtoXYZ(float3 rgb)
{
	const float3x3 m = float3x3(
		0.4124564, 0.3575761, 0.1804375,
		0.2126729, 0.7151522, 0.0721750,
		0.0193339, 0.1191920, 0.9503041);

	return mul(rgb, m);
}

float3 XYZtoRGB(float3 xyz)
{
	const float3x3 m = float3x3(
		3.2404542, -1.5371385, -0.4985314,
		-0.9692660, 1.8760108, 0.0415560,
		0.0556434, -0.2040259, 1.0572252);

	return mul(xyz, m);
}

float3 RGBtoYUV(float3 RGB)
{
	const float3x3 m = float3x3(
		0.2126, 0.7152, 0.0722,
		-0.09991, -0.33609, 0.436,
		0.615, -0.55861, -0.05639);

	return mul(RGB, m);
}

float3 YUVtoRGB(float3 YUV)
{
	const float3x3 m = float3x3(
		1.000, 0.000, 1.28033,
		1.000, -0.21482, -0.38059,
		1.000, 2.12798, 0.000);

	return mul(YUV, m);
}

//Converting XYZ to Yxy
float3 XYZtoYxy(float3 xyz)
{
	float3 Yxy;
	float w = 1.0 / (xyz.r + xyz.g + xyz.b);

	Yxy.r = xyz.g;
	Yxy.g = xyz.r * w;
	Yxy.b = xyz.g * w;

	return Yxy;
}

//Converting Yxy to XYZ
float3 YxytoXYZ(float3 Yxy)
{
	float3 xyz;
	float w = 1.0 / Yxy.b;
	xyz.g = Yxy.r;
	xyz.r = Yxy.r * Yxy.g * w;
	xyz.b = Yxy.r * (1.0 - Yxy.g - Yxy.b) * w;
	return xyz;
}

float MidLuminance(float3 color)
{
	return sqrt(
		(color.x * color.x * 0.3333) +
		(color.y * color.y * 0.3333) +
		(color.z * color.z * 0.3333));
}

/*------------------------------------------------------------------------------
[BICUBIC SCALER CODE SECTION]
------------------------------------------------------------------------------*/

float4 BicubicScaler(float2 uv, float2 txSize)
{
	float2 inputSize = float2(1.0 / txSize.x, 1.0 / txSize.y);

	float2 coord_hg = uv * txSize - 0.5;
	float2 index = floor(coord_hg);
	float2 f = coord_hg - index;

	float4x4 M = float4x4(-1.0, 3.0, -3.0, 1.0, 3.0, -6.0, 3.0, 0.0,
		-3.0, 0.0, 3.0, 0.0, 1.0, 4.0, 1.0, 0.0);
	M /= 6.0;

	float4 wx = mul(float4(f.x*f.x*f.x, f.x*f.x, f.x, 1.0), M);
	float4 wy = mul(float4(f.y*f.y*f.y, f.y*f.y, f.y, 1.0), M);
	float2 w0 = float2(wx.x, wy.x);
	float2 w1 = float2(wx.y, wy.y);
	float2 w2 = float2(wx.z, wy.z);
	float2 w3 = float2(wx.w, wy.w);

	float2 g0 = w0 + w1;
	float2 g1 = w2 + w3;
	float2 h0 = w1 / g0 - 1.0;
	float2 h1 = w3 / g1 + 1.0;

	float2 coord00 = index + h0;
	float2 coord10 = index + float2(h1.x, h0.y);
	float2 coord01 = index + float2(h0.x, h1.y);
	float2 coord11 = index + h1;

	coord00 = (coord00 + 0.5) * inputSize;
	coord10 = (coord10 + 0.5) * inputSize;
	coord01 = (coord01 + 0.5) * inputSize;
	coord11 = (coord11 + 0.5) * inputSize;

	float4 tex00 = SampleLocation(coord00);
	float4 tex10 = SampleLocation(coord10);
	float4 tex01 = SampleLocation(coord01);
	float4 tex11 = SampleLocation(coord11);

	tex00 = lerp(tex01, tex00, float4(g0.y, g0.y, g0.y, g0.y));
	tex10 = lerp(tex11, tex10, float4(g0.y, g0.y, g0.y, g0.y));

	float4 res = lerp(tex10, tex00, float4(g0.x, g0.x, g0.x, g0.x));

	return res;
}

float4 BicubicScalerPass(float4 color)
{
	color = BicubicScaler(GetCoordinates(), GetResolution());
	return color;
}

/*------------------------------------------------------------------------------
[LANCZOS SCALER CODE SECTION]
------------------------------------------------------------------------------*/

float3 PixelPos(float xpos, float ypos)
{
	return SampleLocation(float2(xpos, ypos)).rgb;
}

float4 WeightQuad(float x)
{
	const float PI = 3.141592653;
	float4 weight = FIX(PI * float4(1.0 + x, x, 1.0 - x, 2.0 - x));
	float4 ret = sin(weight) * sin(weight / 2.0) / (weight * weight);

	return ret / dot(ret, float4(1.0, 1.0, 1.0, 1.0));
}

float3 LineRun(float ypos, float4 xpos, float4 linetaps)
{
	float4x3 m = float4x3(
		PixelPos(xpos.x, ypos),
		PixelPos(xpos.y, ypos),
		PixelPos(xpos.z, ypos),
		PixelPos(xpos.w, ypos));
	return mul(linetaps, m);
}

float4 LanczosScaler(float2 inputSize)
{
	float2 stepxy = float2(1.0 / inputSize.x, 1.0 / inputSize.y);
	float2 pos = GetCoordinates() + stepxy * 0.5;
	float2 f = frac(pos / stepxy);

	float2 xystart = (-1.5 - f) * stepxy + pos;
	float4 xpos = float4(
		xystart.x,
		xystart.x + stepxy.x,
		xystart.x + stepxy.x * 2.0,
		xystart.x + stepxy.x * 3.0);

	float4 linetaps = WeightQuad(f.x);
	float4 columntaps = WeightQuad(f.y);
	float4x3 m = float4x3(
		LineRun(xystart.y, xpos, linetaps),
		LineRun(xystart.y + stepxy.y, xpos, linetaps),
		LineRun(xystart.y + stepxy.y * 2.0, xpos, linetaps),
		LineRun(xystart.y + stepxy.y * 3.0, xpos, linetaps));
	return float4( mul(columntaps, m), 1.0);
}

float4 LanczosScalerPass(float4 color)
{
	color = LanczosScaler(GetResolution());
	return color;
}

/*------------------------------------------------------------------------------
[BILINEAR FILTERING CODE SECTION]
------------------------------------------------------------------------------*/

float4 SampleBilinear(float2 texcoord)
{
	float2 texSize = GetResolution();
	float texelSizeX = 1.0 / texSize.x;
	float texelSizeY = 1.0 / texSize.y;

	int nX = int(texcoord.x * texSize.x);
	int nY = int(texcoord.y * texSize.y);

	float2 uvCoord = float2(float(nX) / texSize.x, float(nY) / texSize.y);

	// Take nearest two data in current row.
	float4 SampleA = SampleLocation(uvCoord);
	float4 SampleB = SampleLocation(uvCoord + float2(texelSizeX, 0.0));

	// Take nearest two data in bottom row.
	float4 SampleC = SampleLocation(uvCoord + float2(0.0, texelSizeY));
	float4 SampleD = SampleLocation(uvCoord + float2(texelSizeX, texelSizeY));

	float LX = frac(texcoord.x * texSize.x); //interpolation factor for X direction.
	float LY = frac(texcoord.y * texSize.y); //interpolation factor for Y direction.

	// Interpolate in X direction.
	float4 InterpolateA = lerp(SampleA, SampleB, LX); //Top row in X direction.
	float4 InterpolateB = lerp(SampleC, SampleD, LX); //Bottom row in X direction.

	return lerp(InterpolateA, InterpolateB, LY); //Interpolate in Y direction.
}

float4 BilinearPass(float4 color)
{
	color = SampleBilinear(GetCoordinates());
	return color;
}

/*------------------------------------------------------------------------------
[GAMMA CORRECTION CODE SECTION]
------------------------------------------------------------------------------*/

float3 EncodeGamma(float3 color, float gamma)
{
	color = saturate(color);
	color.r = (color.r <= 0.0404482362771082) ?
		color.r / 12.92 : pow((color.r + 0.055) / 1.055, gamma);
	color.g = (color.g <= 0.0404482362771082) ?
		color.g / 12.92 : pow((color.g + 0.055) / 1.055, gamma);
	color.b = (color.b <= 0.0404482362771082) ?
		color.b / 12.92 : pow((color.b + 0.055) / 1.055, gamma);

	return color;
}

float3 DecodeGamma(float3 color, float gamma)
{
	color = saturate(color);
	color.r = (color.r <= 0.00313066844250063) ?
		color.r * 12.92 : 1.055 * pow(color.r, 1.0 / gamma) - 0.055;
	color.g = (color.g <= 0.00313066844250063) ?
		color.g * 12.92 : 1.055 * pow(color.g, 1.0 / gamma) - 0.055;
	color.b = (color.b <= 0.00313066844250063) ?
		color.b * 12.92 : 1.055 * pow(color.b, 1.0 / gamma) - 0.055;

	return color;
}

float4 GammaPass(float4 color)
{
	const float GammaConst = 2.233333;
	color.rgb = EncodeGamma(color.rgb, GammaConst);
	color.rgb = DecodeGamma(color.rgb, GetOption(A_GAMMA));
	color.a = AvgLuminance(color.rgb);

	return color;
}

/*------------------------------------------------------------------------------
[BLENDED BLOOM CODE SECTION]
------------------------------------------------------------------------------*/

float3 BlendAddLight(float3 bloom, float3 blend)
{
	return saturate(bloom + blend);
}

float3 BlendScreen(float3 bloom, float3 blend)
{
	return (bloom + blend) - (bloom * blend);
}

float3 BlendAddGlow(float3 bloom, float3 blend)
{
	float glow = smootherstep(0.0, 1.0, AvgLuminance(bloom));
	return lerp(saturate(bloom + blend),
		(blend + blend) - (blend * blend), glow);
}

float3 BlendGlow(float3 bloom, float3 blend)
{
	float glow = smootherstep(0.0, 1.0, AvgLuminance(bloom));
	return lerp((bloom + blend) - (bloom * blend),
		(blend + blend) - (blend * blend), glow);
}

float3 BlendLuma(float3 bloom, float3 blend)
{
	float lumavg = smootherstep(0.0, 1.0, AvgLuminance(bloom + blend));
	return lerp((bloom * blend), (1.0 -
		((1.0 - bloom) * (1.0 - blend))), lumavg);
}

float3 BlendOverlay(float3 bloom, float3 blend)
{
	float3 overlay = step(0.5, bloom);
	return lerp((bloom * blend * 2.0), (1.0 - (2.0 *
		(1.0 - bloom) * (1.0 - blend))), overlay);
}

float3 BloomCorrection(float3 color)
{
	float3 bloom = color;

	bloom.r = 2.0 / 3.0 * (1.0 - (bloom.r * bloom.r));
	bloom.g = 2.0 / 3.0 * (1.0 - (bloom.g * bloom.g));
	bloom.b = 2.0 / 3.0 * (1.0 - (bloom.b * bloom.b));

	bloom.r = saturate(color.r + GetOption(E_BLOOM_REDS) * bloom.r);
	bloom.g = saturate(color.g + GetOption(F_BLOOM_GREENS) * bloom.g);
	bloom.b = saturate(color.b + GetOption(G_BLOOM_BLUES) * bloom.b);

	color = saturate(bloom);

	return color;
}

float4 PyramidFilter(float2 texcoord, float2 width)
{
	float4 X = SampleLocation(texcoord + float2(0.5, 0.5) * width);
	float4 Y = SampleLocation(texcoord + float2(-0.5, 0.5) * width);
	float4 Z = SampleLocation(texcoord + float2(0.5, -0.5) * width);
	float4 W = SampleLocation(texcoord + float2(-0.5, -0.5) * width);

	return (X + Y + Z + W) / 4.0;
}

float3 Blend(float3 bloom, float3 blend)
{
	if (GetOption(A_BLOOM_TYPE) == 0) { return BlendGlow(bloom, blend); }
	else if (GetOption(A_BLOOM_TYPE) == 1) { return BlendAddGlow(bloom, blend); }
	else if (GetOption(A_BLOOM_TYPE) == 2) { return BlendAddLight(bloom, blend); }
	else if (GetOption(A_BLOOM_TYPE) == 3) { return BlendScreen(bloom, blend); }
	else if (GetOption(A_BLOOM_TYPE) == 4) { return BlendLuma(bloom, blend); }
	else { return BlendOverlay(bloom, blend); }
}

float4 BloomPass(float4 color)
{
	float2 pixelSize = GetInvResolution();
	float anflare = 4.0;
	float2 texcoord = GetCoordinates();
	float2 defocus = float2(GetOption(D_B_DEFOCUS), GetOption(D_B_DEFOCUS));
	float4 bloom = PyramidFilter(texcoord, pixelSize * defocus);

	float2 dx = float2(pixelSize.x * GetOption(D_BLOOM_WIDTH), 0.0);
	float2 dy = float2(0.0, pixelSize.y * GetOption(D_BLOOM_WIDTH));

	float2 mdx = mul(dx, 2.0);
	float2 mdy = mul(dy, 2.0);

	float4 blend = bloom * 0.22520613262190495;

	blend += 0.002589001911021066 * SampleLocation(texcoord - mdx + mdy);
	blend += 0.010778807494659370 * SampleLocation(texcoord - dx + mdy);
	blend += 0.024146616900339800 * SampleLocation(texcoord + mdy);
	blend += 0.010778807494659370 * SampleLocation(texcoord + dx + mdy);
	blend += 0.002589001911021066 * SampleLocation(texcoord + mdx + mdy);

	blend += 0.010778807494659370 * SampleLocation(texcoord - mdx + dy);
	blend += 0.044875475183061630 * SampleLocation(texcoord - dx + dy);
	blend += 0.100529757860782610 * SampleLocation(texcoord + dy);
	blend += 0.044875475183061630 * SampleLocation(texcoord + dx + dy);
	blend += 0.010778807494659370 * SampleLocation(texcoord + mdx + dy);

	blend += 0.024146616900339800 * SampleLocation(texcoord - mdx);
	blend += 0.100529757860782610 * SampleLocation(texcoord - dx);
	blend += 0.100529757860782610 * SampleLocation(texcoord + dx);
	blend += 0.024146616900339800 * SampleLocation(texcoord + mdx);

	blend += 0.010778807494659370 * SampleLocation(texcoord - mdx - dy);
	blend += 0.044875475183061630 * SampleLocation(texcoord - dx - dy);
	blend += 0.100529757860782610 * SampleLocation(texcoord - dy);
	blend += 0.044875475183061630 * SampleLocation(texcoord + dx - dy);
	blend += 0.010778807494659370 * SampleLocation(texcoord + mdx - dy);

	blend += 0.002589001911021066 * SampleLocation(texcoord - mdx - mdy);
	blend += 0.010778807494659370 * SampleLocation(texcoord - dx - mdy);
	blend += 0.024146616900339800 * SampleLocation(texcoord - mdy);
	blend += 0.010778807494659370 * SampleLocation(texcoord + dx - mdy);
	blend += 0.002589001911021066 * SampleLocation(texcoord + mdx - mdy);
	blend = lerp(color, blend, GetOption(C_BLEND_STRENGTH));

	bloom.xyz = Blend(bloom.xyz, blend.xyz);
	bloom.xyz = BloomCorrection(bloom.xyz);

	color.a = AvgLuminance(color.xyz);
	bloom.a = AvgLuminance(bloom.xyz);
	bloom.a *= anflare;

	color = lerp(color, bloom, GetOption(B_BLOOM_STRENGTH));

	return color;
}

/*------------------------------------------------------------------------------
[SCENE TONE MAPPING CODE SECTION]
------------------------------------------------------------------------------*/

float3 FilmicCurve(float3 color)
{
	float3 T = color;
	float tnamn = GetOption(B_TONE_AMOUNT);

	float A = 0.100;
	float B = 0.300;
	float C = 0.100;
	float D = tnamn;
	float E = 0.020;
	float F = 0.300;
	float W = 1.012;

	T.r = ((T.r*(A*T.r + C*B) + D*E) / (T.r*(A*T.r + B) + D*F)) - E / F;
	T.g = ((T.g*(A*T.g + C*B) + D*E) / (T.g*(A*T.g + B) + D*F)) - E / F;
	T.b = ((T.b*(A*T.b + C*B) + D*E) / (T.b*(A*T.b + B) + D*F)) - E / F;

	float denom = ((W*(A*W + C*B) + D*E) / (W*(A*W + B) + D*F)) - E / F;
	float3 white = float3(denom, denom, denom);

	T = T / white;
	color = saturate(T);

	return color;
}

float3 FilmicTonemap(float3 color)
{
	float3 tone = color;

	float3 black = float3(0.0, 0.0, 0.0);
	tone = max(black, tone);

	tone.r = (tone.r * (6.2 * tone.r + 0.5)) / (tone.r * (6.2 * tone.r + 1.66) + 0.066);
	tone.g = (tone.g * (6.2 * tone.g + 0.5)) / (tone.g * (6.2 * tone.g + 1.66) + 0.066);
	tone.b = (tone.b * (6.2 * tone.b + 0.5)) / (tone.b * (6.2 * tone.b + 1.66) + 0.066);

	const float gamma = 2.42;
	tone = EncodeGamma(tone, gamma);

	color = lerp(color, tone, GetOption(B_TONE_FAMOUNT));

	return color;
}

float4 TonemapPass(float4 color)
{
	float luminanceAverage = GetOption(E_LUMINANCE);
	float bmax = max(color.r, max(color.g, color.b));

	float blevel = pow(saturate(bmax), GetOption(C_BLACK_LEVELS));
	color.rgb = color.rgb * blevel;

	if (OptionEnabled(A_TONEMAP_FILM)) { color.rgb = FilmicTonemap(color.rgb); }
	if (GetOption(A_TONEMAP_TYPE) == 1) { color.rgb = FilmicCurve(color.rgb); }

	float3 XYZ = RGBtoXYZ(color.rgb);

	// XYZ -> Yxy conversion
	float3 Yxy = XYZtoYxy(XYZ);

	// (Wt) Tone mapped scaling of the initial wp before input modifiers
	float Wt = saturate(Yxy.r / AvgLuminance(XYZ));

	if (GetOption(A_TONEMAP_TYPE) == 2) { Yxy.r = FilmicCurve(Yxy).r; }

	// (Lp) Map average luminance to the middlegrey zone by scaling pixel luminance
	float Lp = Yxy.r * GetOption(D_EXPOSURE) / (luminanceAverage + Epsilon);

	// (Wp) White point calculated, based on the toned white, and input modifier
	float Wp = abs(Wt) * GetOption(F_WHITEPOINT);

	// (Ld) Scale all luminance within a displayable range of 0 to 1
	Yxy.r = (Lp * (1.0 + Lp / (Wp * Wp))) / (1.0 + Lp);

	// Yxy -> XYZ conversion
	XYZ = YxytoXYZ(Yxy);

	color.rgb = XYZtoRGB(XYZ);
	color.a = AvgLuminance(color.rgb);

	return color;
}

/*------------------------------------------------------------------------------
[CROSS PROCESSING CODE SECTION]
------------------------------------------------------------------------------*/

float3 CrossShift(float3 color)
{
	float3 cross;

	float2 CrossMatrix[3] = {
		float2(0.960, 0.040 * color.x),
		float2(0.980, 0.020 * color.y),
		float2(0.970, 0.030 * color.z), };

	cross.x = GetOption(B_RED_SHIFT) * CrossMatrix[0].x + CrossMatrix[0].y;
	cross.y = GetOption(C_GREEN_SHIFT) * CrossMatrix[1].x + CrossMatrix[1].y;
	cross.z = GetOption(D_BLUE_SHIFT) * CrossMatrix[2].x + CrossMatrix[2].y;

	float lum = MidLuminance(color);
	float3 black = float3(0.0, 0.0, 0.0);
	float3 white = float3(1.0, 1.0, 1.0);

	cross = lerp(black, cross, saturate(lum * 2.0));
	cross = lerp(cross, white, saturate(lum - 0.5) * 2.0);
	color = lerp(color, cross, saturate(lum * GetOption(E_SHIFT_RATIO)));

	return color;
}

float4 CrossPass(float4 color)
{
	if (GetOption(A_FILMIC) == 0) {
		color.rgb = CrossShift(color.rgb);
	}

	else if (GetOption(A_FILMIC) == 1) {
		float3 XYZ = RGBtoXYZ(color.rgb);
		float3 Yxy = XYZtoYxy(XYZ);

		Yxy = CrossShift(Yxy);
		XYZ = YxytoXYZ(Yxy);

		color.rgb = XYZtoRGB(XYZ);
	}

	else if (GetOption(A_FILMIC) == 2) {
		float3 XYZ = RGBtoXYZ(color.rgb);
		float3 Yxy = XYZtoYxy(XYZ);

		XYZ = YxytoXYZ(Yxy);
		XYZ = CrossShift(XYZ);

		color.rgb = XYZtoRGB(XYZ);
	}

	color.a = AvgLuminance(color.rgb);

	return saturate(color);
}

/*------------------------------------------------------------------------------
[COLOR CORRECTION CODE SECTION]
------------------------------------------------------------------------------*/

// Converting pure hue to RGB
float3 HUEtoRGB(float H)
{
	float R = abs(H * 6.0 - 3.0) - 1.0;
	float G = 2.0 - abs(H * 6.0 - 2.0);
	float B = 2.0 - abs(H * 6.0 - 4.0);

	return saturate(float3(R, G, B));
}

// Converting RGB to hue/chroma/value
float3 RGBtoHCV(float3 RGB)
{
	float4 BG = float4(RGB.bg, -1.0, 2.0 / 3.0);
	float4 GB = float4(RGB.gb, 0.0, -1.0 / 3.0);

	float4 P = (RGB.g < RGB.b) ? BG : GB;

	float4 XY = float4(P.xyw, RGB.r);
	float4 YZ = float4(RGB.r, P.yzx);

	float4 Q = (RGB.r < P.x) ? XY : YZ;

	float C = Q.x - min(Q.w, Q.y);
	float H = abs((Q.w - Q.y) / (6.0 * C + Epsilon) + Q.z);

	return float3(H, C, Q.x);
}

// Converting RGB to HSV
float3 RGBtoHSV(float3 RGB)
{
	float3 HCV = RGBtoHCV(RGB);
	float S = HCV.y / (HCV.z + Epsilon);

	return float3(HCV.x, S, HCV.z);
}

// Converting HSV to RGB
float3 HSVtoRGB(float3 HSV)
{
	float3 RGB = HUEtoRGB(HSV.x);
	return ((RGB - 1.0) * HSV.y + 1.0) * HSV.z;
}

// Pre correction color mask
float3 PreCorrection(float3 color)
{
	float3 RGB = color;

	RGB.r = 2.0 / 3.0 * (1.0 - (RGB.r * RGB.r));
	RGB.g = 2.0 / 3.0 * (1.0 - (RGB.g * RGB.g));
	RGB.b = 2.0 / 3.0 * (1.0 - (RGB.b * RGB.b));

	RGB.r = saturate(color.r + (GetOption(B_RED_CORRECTION) / 200.0) * RGB.r);
	RGB.g = saturate(color.g + (GetOption(C_GREEN_CORRECTION) / 200.0) * RGB.g);
	RGB.b = saturate(color.b + (GetOption(D_BLUE_CORRECTION) / 200.0) * RGB.b);

	color = saturate(RGB);

	return color;
}

float3 ColorCorrection(float3 color)
{
	float X = 1.0 / (1.0 + exp(GetOption(B_RED_CORRECTION) / 2.0));
	float Y = 1.0 / (1.0 + exp(GetOption(C_GREEN_CORRECTION) / 2.0));
	float Z = 1.0 / (1.0 + exp(GetOption(D_BLUE_CORRECTION) / 2.0));

	color.r = (1.0 / (1.0 + exp(-GetOption(B_RED_CORRECTION) * (color.r - 0.5))) - X) / (1.0 - 2.0 * X);
	color.g = (1.0 / (1.0 + exp(-GetOption(C_GREEN_CORRECTION) * (color.g - 0.5))) - Y) / (1.0 - 2.0 * Y);
	color.b = (1.0 / (1.0 + exp(-GetOption(D_BLUE_CORRECTION) * (color.b - 0.5))) - Z) / (1.0 - 2.0 * Z);

	return saturate(color);
}

float4 CorrectionPass(float4 color)
{
	float3 colorspace = PreCorrection(color.rgb);

	if (GetOption(A_PALETTE) == 0) {
		colorspace = ColorCorrection(colorspace);
	}

	else if (GetOption(A_PALETTE) == 1) {
		float3 XYZ = RGBtoXYZ(colorspace);
		float3 Yxy = XYZtoYxy(XYZ);

		Yxy = ColorCorrection(Yxy);
		XYZ = YxytoXYZ(Yxy);
		colorspace = XYZtoRGB(XYZ);
	}

	else if (GetOption(A_PALETTE) == 2) {
		float3 XYZ = RGBtoXYZ(colorspace);
		float3 Yxy = XYZtoYxy(XYZ);

		XYZ = YxytoXYZ(Yxy);
		XYZ = ColorCorrection(XYZ);
		colorspace = XYZtoRGB(XYZ);
	}

	else if (GetOption(A_PALETTE) == 3) {
		float3 hsv = RGBtoHSV(colorspace);
		hsv = ColorCorrection(hsv);
		colorspace = HSVtoRGB(hsv);
	}

	else if (GetOption(A_PALETTE) == 4) {
		float3 yuv = RGBtoYUV(colorspace);
		yuv = ColorCorrection(yuv);
		colorspace = YUVtoRGB(yuv);
	}

	color.rgb = lerp(color.rgb, colorspace, GetOption(E_CORRECT_STR));
	color.a = AvgLuminance(color.rgb);

	return color;
}

/*------------------------------------------------------------------------------
[TEXTURE SHARPEN CODE SECTION]
------------------------------------------------------------------------------*/

float Cubic(float coeff)
{
	float4 n = float4(1.0, 2.0, 3.0, 4.0) - coeff;
	float4 s = n * n * n;

	float x = s.x;
	float y = s.y - 4.0 * s.x;
	float z = s.z - 4.0 * s.y + 6.0 * s.x;
	float w = 6.0 - x - y - z;

	return (x + y + z + w) / 4.0;
}

float4 SampleSharpenBicubic(float2 texcoord)
{
	float2 texSize = GetResolution();
	float texelSizeX = (1.0 / texSize.x) * GetOption(C_SHARPEN_BIAS);
	float texelSizeY = (1.0 / texSize.y) * GetOption(C_SHARPEN_BIAS);

	float4 nSum = float4(0.0, 0.0, 0.0, 0.0);
	float4 nDenom = float4(0.0, 0.0, 0.0, 0.0);

	float a = frac(texcoord.x * texSize.x);
	float b = frac(texcoord.y * texSize.y);

	int nX = int(texcoord.x * texSize.x);
	int nY = int(texcoord.y * texSize.y);

	float2 uvCoord = float2(float(nX) / texSize.x, float(nY) / texSize.y);

	for (int m = -1; m <= 2; m++) {
		for (int n = -1; n <= 2; n++) {

			float4 Samples = SampleLocation(uvCoord +
				float2(texelSizeX * float(m), texelSizeY * float(n)));

			float vc1 = Cubic(float(m) - a);
			float4 vecCoeff1 = float4(vc1, vc1, vc1, vc1);

			float vc2 = Cubic(-(float(n) - b));
			float4 vecCoeff2 = float4(vc2, vc2, vc2, vc2);

			nSum = nSum + (Samples * vecCoeff2 * vecCoeff1);
			nDenom = nDenom + (vecCoeff2 * vecCoeff1);
		}
	}

	return nSum / nDenom;
}

float4 TexSharpenPass(float4 color)
{
	float3 calcSharpen = lumCoeff * GetOption(A_SHARPEN_STRENGTH);

	float4 blurredColor = SampleSharpenBicubic(GetCoordinates());
	float3 sharpenedColor = (color.rgb - blurredColor.rgb);

	float sharpenLuma = dot(sharpenedColor, calcSharpen);
	sharpenLuma = clamp(sharpenLuma, -GetOption(B_SHARPEN_CLAMP), GetOption(B_SHARPEN_CLAMP));

	color.rgb = color.rgb + sharpenLuma;
	color.a = AvgLuminance(color.rgb);

	if (GetOption(D_SEDGE_DETECTION) == 1)
	{
		color = (0.5 + (sharpenLuma * 4)).xxxx;
	}

	return color;
}

/*------------------------------------------------------------------------------
[PIXEL VIBRANCE CODE SECTION]
------------------------------------------------------------------------------*/

float4 VibrancePass(float4 color)
{
	float vib = GetOption(A_VIBRANCE);
	float luma = AvgLuminance(color.rgb);

	float colorMax = max(color.r, max(color.g, color.b));
	float colorMin = min(color.r, min(color.g, color.b));

	float colorSaturation = colorMax - colorMin;
	float3 colorCoeff = float3(GetOption(B_R_VIBRANCE)*
		vib, GetOption(C_G_VIBRANCE) * vib, GetOption(D_B_VIBRANCE) * vib);

	color.rgb = lerp(float3(luma, luma, luma), color.rgb, (1.0 + (colorCoeff * (1.0 - (sign(colorCoeff) * colorSaturation)))));
	color.a = AvgLuminance(color.rgb);

	return saturate(color); //Debug: return colorSaturation.xxxx;
}

/*------------------------------------------------------------------------------
[LOCAL CONTRAST CODE SECTION]
------------------------------------------------------------------------------*/

float4 ContrastPass(float4 color)
{
	float lum = AvgLuminance(color.rgb);
	float3 luma = float3(lum, lum, lum);
	float3 chroma = color.rgb - luma;
	float3 x = luma;

	//Cubic Bezier spline
	float3 a = float3(0.00, 0.00, 0.00);    //start point
	float3 b = float3(0.25, 0.25, 0.25);    //control point 1
	float3 c = float3(0.80, 0.80, 0.80);    //control point 2
	float3 d = float3(1.00, 1.00, 1.00);    //endpoint

	float3 ab = lerp(a, b, x);              //point between a and b (green)
	float3 bc = lerp(b, c, x);              //point between b and c (green)
	float3 cd = lerp(c, d, x);              //point between c and d (green)
	float3 abbc = lerp(ab, bc, x);          //point between ab and bc (blue)
	float3 bccd = lerp(bc, cd, x);          //point between bc and cd (blue)
	float3 dest = lerp(abbc, bccd, x);      //point on the bezier-curve (black)

	x = dest;
	x = lerp(luma, x, GetOption(A_CONTRAST));
	color.rgb = x + chroma;

	color.a = AvgLuminance(color.rgb);

	return saturate(color);
}

/*------------------------------------------------------------------------------
[CEL SHADING CODE SECTION]
------------------------------------------------------------------------------*/

float4 CelPass(float4 color)
{
	float3 yuv;
	float3 sum = color.rgb;

	const int NUM = 9;
	const float2 RoundingOffset = float2(0.25, 0.25);
	const float3 thresholds = float3(9.0, 8.0, 6.0);

	float lum[NUM];
	float3 col[NUM];
	float2 set[NUM] = {
		float2(-0.0078125, -0.0078125),
		float2(0.00, -0.0078125),
		float2(0.0078125, -0.0078125),
		float2(-0.0078125, 0.00),
		float2(0.00, 0.00),
		float2(0.0078125, 0.00),
		float2(-0.0078125, 0.0078125),
		float2(0.00, 0.0078125),
		float2(0.0078125, 0.0078125)};

	for (int i = 0; i < NUM; i++)
	{
		col[i] = SampleLocation(GetCoordinates() + set[i] * RoundingOffset).rgb;

		if (GetOption(G_COLOR_ROUNDING) == 1) {
			col[i].r = round(col[i].r * thresholds.r) / thresholds.r;
			col[i].g = round(col[i].g * thresholds.g) / thresholds.g;
			col[i].b = round(col[i].b * thresholds.b) / thresholds.b;
		}

		lum[i] = AvgLuminance(col[i].xyz);
		yuv = RGBtoYUV(col[i]);

		if (GetOption(E_YUV_LUMA) == 0)
		{
			yuv.r = round(yuv.r * thresholds.r) / thresholds.r;
		}
		else
		{
			yuv.r = saturate(round(yuv.r * lum[i]) / thresholds.r + lum[i]);
		}

		yuv = YUVtoRGB(yuv);
		sum += yuv;
	}

	float3 shadedColor = (sum / NUM);
	float2 texSize = GetResolution();
	float2 pixel = (1.0 / texSize.xy) * GetOption(C_EDGE_THICKNESS);

	float edgeX = dot(SampleLocation(GetCoordinates() + pixel).rgb, lumCoeff);
	edgeX = dot(float4(SampleLocation(GetCoordinates() - pixel).rgb, edgeX), float4(lumCoeff, -1.0));

	float edgeY = dot(SampleLocation(GetCoordinates() + float2(pixel.x, -pixel.y)).rgb, lumCoeff);
	edgeY = dot(float4(SampleLocation(GetCoordinates() + float2(-pixel.x, pixel.y)).rgb, edgeY), float4(lumCoeff, -1.0));

	float edge = dot(float2(edgeX, edgeY), float2(edgeX, edgeY));

	if (GetOption(D_PALETTE_TYPE) == 0)
	{
		color.rgb = lerp(color.rgb, color.rgb + pow(edge, GetOption(B_EDGE_FILTER)) * -GetOption(A_EDGE_STRENGTH), GetOption(A_EDGE_STRENGTH));
	}
	else if (GetOption(D_PALETTE_TYPE) == 1)
	{
		color.rgb = lerp(color.rgb + pow(edge, GetOption(B_EDGE_FILTER)) * -GetOption(A_EDGE_STRENGTH), shadedColor, 0.25);
	}
	else if (GetOption(D_PALETTE_TYPE) == 2)
	{
		color.rgb = lerp(shadedColor + edge * -GetOption(A_EDGE_STRENGTH), pow(edge, GetOption(B_EDGE_FILTER)) * -GetOption(A_EDGE_STRENGTH) + color.rgb, 0.50);
	}

	color.a = AvgLuminance(color.rgb);

	return saturate(color);
}

/*------------------------------------------------------------------------------
[FILM GRAIN CODE SECTION]
------------------------------------------------------------------------------*/



float Fade(float t)
{
	return t*t*t*(t*(t*6.0 - 15.0) + 10.0);
}

float2 CoordRot(float2 tc, float angle)
{
	float2 screenSize = GetResolution();
	float aspect = screenSize.x / screenSize.y;
	float rotX = ((tc.x * 2.0 - 1.0) * aspect * cos(angle)) - ((tc.y * 2.0 - 1.0) * sin(angle));
	float rotY = ((tc.y * 2.0 - 1.0) * cos(angle)) + ((tc.x * 2.0 - 1.0) * aspect * sin(angle));

	rotX = ((rotX / aspect) * 0.5 + 0.5);
	rotY = rotY * 0.5 + 0.5;

	return float2(rotX, rotY);
}

float4 Perm(float2 texcoord)
{
	float noise = RandomSeedfloat(texcoord);

	float noiseR = noise * 2.0 - 1.0;
	float noiseG = frac(noise * 1.2154) * 2.0 - 1.0;
	float noiseB = frac(noise * 1.3453) * 2.0 - 1.0;
	float noiseA = frac(noise * 1.3647) * 2.0 - 1.0;

	return float4(noiseR, noiseG, noiseB, noiseA);
}

float PerNoise(float3 p)
{
	const float permTexUnit = 1.0 / 256.0;
	const float permTexUnitHalf = 0.5 / 256.0;

	float3 pf = frac(p);
	float3 pi = permTexUnit * floor(p) + permTexUnitHalf;

	// Noise contributions from (x=0, y=0), z=0 and z=1
	float perm00 = Perm(pi.xy).a;
	float3  grad000 = Perm(float2(perm00, pi.z)).rgb * 4.0 - 1.0;
	float n000 = dot(grad000, pf);
	float3  grad001 = Perm(float2(perm00, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
	float n001 = dot(grad001, pf - float3(0.0, 0.0, 1.0));

	// Noise contributions from (x=0, y=1), z=0 and z=1
	float perm01 = Perm(pi.xy + float2(0.0, permTexUnit)).a;
	float3  grad010 = Perm(float2(perm01, pi.z)).rgb * 4.0 - 1.0;
	float n010 = dot(grad010, pf - float3(0.0, 1.0, 0.0));
	float3  grad011 = Perm(float2(perm01, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
	float n011 = dot(grad011, pf - float3(0.0, 1.0, 1.0));

	// Noise contributions from (x=1, y=0), z=0 and z=1
	float perm10 = Perm(pi.xy + float2(permTexUnit, 0.0)).a;
	float3  grad100 = Perm(float2(perm10, pi.z)).rgb * 4.0 - 1.0;
	float n100 = dot(grad100, pf - float3(1.0, 0.0, 0.0));
	float3  grad101 = Perm(float2(perm10, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
	float n101 = dot(grad101, pf - float3(1.0, 0.0, 1.0));

	// Noise contributions from (x=1, y=1), z=0 and z=1
	float perm11 = Perm(pi.xy + float2(permTexUnit, permTexUnit)).a;
	float3  grad110 = Perm(float2(perm11, pi.z)).rgb * 4.0 - 1.0;
	float n110 = dot(grad110, pf - float3(1.0, 1.0, 0.0));
	float3  grad111 = Perm(float2(perm11, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
	float n111 = dot(grad111, pf - float3(1.0, 1.0, 1.0));

	float4 n_x = lerp(float4(n000, n001, n010, n011), float4(n100, n101, n110, n111), Fade(pf.x));

	float2 n_xy = lerp(n_x.xy, n_x.zw, Fade(pf.y));
	float n_xyz = lerp(n_xy.x, n_xy.y, Fade(pf.z));

	return n_xyz;
}

float4 GrainPass(float4 color)
{
	float2 screenSize = GetResolution();
	float3 rotOffset = float3(1.425, 3.892, 5.835); //rotation offset values	
	float2 rotCoordsR = CoordRot(GetCoordinates(), GetTime() + rotOffset.x);
	float noiseval = PerNoise(float3(rotCoordsR * (screenSize / GetOption(A_GRAIN_SIZE)), 0.0));
	float3 noise = float3(noiseval, noiseval, noiseval);

	if (GetOption(C_COLORED) == 1) {
		float2 rotCoordsG = CoordRot(GetCoordinates(), GetTime() + rotOffset.y);
		float2 rotCoordsB = CoordRot(GetCoordinates(), GetTime() + rotOffset.z);
		noise.g = lerp(noise.r, PerNoise(float3(rotCoordsG *
			float2(screenSize.x / GetOption(A_GRAIN_SIZE), screenSize.y / GetOption(A_GRAIN_SIZE)), 1.0)), GetOption(D_COLOR_AMOUNT));
		noise.b = lerp(noise.r, PerNoise(float3(rotCoordsB *
			float2(screenSize.x / GetOption(A_GRAIN_SIZE), screenSize.y / GetOption(A_GRAIN_SIZE)), 2.0)), GetOption(D_COLOR_AMOUNT));
	}

	//noisiness response curve based on scene luminance
	float lamount = GetOption(E_LUMA_AMOUNT);	
	float luminance = ColorLuminance(color.rgb);
	luminance = lerp(0.0, luminance, lamount);
	float lum = smoothstep(0.2, 0.0, luminance);
	lum += luminance;
	lum = pow(lum, 4.0);
	noise = lerp(noise, float3(0.0, 0.0, 0.0), lum);
	color.rgb = color.rgb + noise * GetOption(B_GRAIN_AMOUNT);

	return color;
}

/*------------------------------------------------------------------------------
[VIGNETTE CODE SECTION]
------------------------------------------------------------------------------*/

float4 VignettePass(float4 color)
{
	const float2 VignetteCenter = float2(0.500, 0.500);
	float2 tc = GetCoordinates() - VignetteCenter;

	// hardcoded pre ratio calculations, for uniform output on arbitrary resolutions.
	tc *= float2((2560.0 / 1440.0), GetOption(A_VIG_RATIO));
	tc /= GetOption(B_VIG_RADIUS);

	float v = dot(tc, tc);

	color.rgb *= (1.0 + pow(v, GetOption(D_VIG_SLOPE) * 0.25) * -GetOption(C_VIG_AMOUNT));

	return color;
}

/*------------------------------------------------------------------------------
[SCANLINES CODE SECTION]
------------------------------------------------------------------------------*/

float4 ScanlinesPass(float4 color)
{
	float4 intensity;
	float2 fragcoord = GetFragmentCoord();
	if (GetOption(A_SCANLINE_TYPE) == 0) { //X coord scanlines
		if (frac(fragcoord.y * GetOption(B_SCANLINE_SPACING)) > GetOption(B_SCANLINE_THICKNESS))
		{
			intensity = float4(0.0, 0.0, 0.0, 0.0);
		}
		else
		{
			intensity = smoothstep(0.2, GetOption(B_SCANLINE_BRIGHTNESS), color) +
				normalize(float4(color.xyz, AvgLuminance(color.xyz)));
		}
	}

	else if (GetOption(A_SCANLINE_TYPE) == 1) { //Y coord scanlines
		if (frac(fragcoord.x * GetOption(B_SCANLINE_SPACING)) > GetOption(B_SCANLINE_THICKNESS))
		{
			intensity = float4(0.0, 0.0, 0.0, 0.0);
		}
		else
		{
			intensity = smoothstep(0.2, GetOption(B_SCANLINE_BRIGHTNESS), color) +
				normalize(float4(color.xyz, AvgLuminance(color.xyz)));
		}
	}

	else if (GetOption(A_SCANLINE_TYPE) == 2) { //XY coord scanlines
		if (frac(fragcoord.x * GetOption(B_SCANLINE_SPACING)) > GetOption(B_SCANLINE_THICKNESS) &&
			frac(fragcoord.y * GetOption(B_SCANLINE_SPACING)) > GetOption(B_SCANLINE_THICKNESS))
		{
			intensity = float4(0.0, 0.0, 0.0, 0.0);
		}
		else
		{
			intensity = smoothstep(0.2, GetOption(B_SCANLINE_BRIGHTNESS), color) +
				normalize(float4(color.xyz, AvgLuminance(color.xyz)));
		}
	}

	float level = (4.0 - GetCoordinates().x) * GetOption(B_SCANLINE_INTENSITY);

	color = intensity * (0.5 - level) + color * 1.1;

	return saturate(color);
}

/*------------------------------------------------------------------------------
[DITHERING CODE SECTION]
------------------------------------------------------------------------------*/

float4 DitherPass(float4 color)
{
	float ditherBits = 8.0;
	float2 fragcoord = GetFragmentCoord();
	if (GetOption(A_DITHER_TYPE) == 1) {  //random dithering
		
		float noise = Rndfloat();
		float ditherShift = (1.0 / (pow(2.0, ditherBits) - 1.0));
		float ditherHalfShift = (ditherShift * 0.5);
		ditherShift = ditherShift * noise - ditherHalfShift;

		color.rgb += float3(-ditherShift, ditherShift, -ditherShift);

		if (GetOption(B_DITHER_SHOW) == 1)
		{
			color.rgb = float3(noise, noise,noise);
		}
	}

	else if (GetOption(A_DITHER_TYPE) == 0) { //ordered dithering

		float2 ditherSize = float2(1.0 / 16.0, 10.0 / 36.0);
		float gridPosition = frac(dot(fragcoord.xy, ditherSize) + 0.25);
		float ditherShift = (0.25) * (1.0 / (pow(2.0, ditherBits) - 1.0));

		float3 RGBShift = float3(ditherShift, -ditherShift, ditherShift);
		RGBShift = lerp(2.0 * RGBShift, -2.0 * RGBShift, gridPosition);

		color.rgb += RGBShift;

		if (GetOption(B_DITHER_SHOW) == 1)
		{
			color.rgb = float3(gridPosition, gridPosition, gridPosition);
		}
	}

	color.a = AvgLuminance(color.rgb);

	return color;
}


/*------------------------------------------------------------------------------
[FXAA CODE SECTION]
------------------------------------------------------------------------------*/

#define FXAA_QUALITY__PS 9
#define FXAA_QUALITY__P0 1.0
#define FXAA_QUALITY__P1 1.5
#define FXAA_QUALITY__P2 2.0
#define FXAA_QUALITY__P3 2.0
#define FXAA_QUALITY__P4 2.0
#define FXAA_QUALITY__P5 2.0
#define FXAA_QUALITY__P6 2.0
#define FXAA_QUALITY__P7 4.0
#define FXAA_QUALITY__P8 8.0

float FxaaLuma(float4 rgba) { return rgba.y; }

float4 FxaaPixelShader(float2 RcpFrame, float Subpix, float EdgeThreshold, float EdgeThresholdMin)
{
	float2 posM = GetCoordinates();
	float4 rgbyM = Sample();
	float lumaM = FxaaLuma(rgbyM);
	float lumaS = FxaaLuma(SampleOffset(int2( 0,  1)));
	float lumaE = FxaaLuma(SampleOffset(int2( 1,  0)));
	float lumaN = FxaaLuma(SampleOffset(int2( 0, -1)));
	float lumaW = FxaaLuma(SampleOffset(int2(-1,  0)));

	float maxSM = max(lumaS, lumaM);
	float minSM = min(lumaS, lumaM);
	float maxESM = max(lumaE, maxSM);
	float minESM = min(lumaE, minSM);
	float maxWN = max(lumaN, lumaW);
	float minWN = min(lumaN, lumaW);
	float rangeMax = max(maxWN, maxESM);
	float rangeMin = min(minWN, minESM);
	float rangeMaxScaled = rangeMax * EdgeThreshold;
	float range = rangeMax - rangeMin;
	float rangeMaxClamped = max(EdgeThresholdMin, rangeMaxScaled);
	bool earlyExit = range < rangeMaxClamped;

	if (earlyExit)
		return rgbyM;

	float lumaNW = FxaaLuma(SampleOffset(int2(-1,-1)));
	float lumaSE = FxaaLuma(SampleOffset(int2( 1, 1)));
	float lumaNE = FxaaLuma(SampleOffset(int2( 1,-1)));
	float lumaSW = FxaaLuma(SampleOffset(int2(-1, 1)));

	float lumaNS = lumaN + lumaS;
	float lumaWE = lumaW + lumaE;
	float subpixRcpRange = 1.0 / range;
	float subpixNSWE = lumaNS + lumaWE;
	float edgeHorz1 = (-2.0 * lumaM) + lumaNS;
	float edgeVert1 = (-2.0 * lumaM) + lumaWE;

	float lumaNESE = lumaNE + lumaSE;
	float lumaNWNE = lumaNW + lumaNE;
	float edgeHorz2 = (-2.0 * lumaE) + lumaNESE;
	float edgeVert2 = (-2.0 * lumaN) + lumaNWNE;

	float lumaNWSW = lumaNW + lumaSW;
	float lumaSWSE = lumaSW + lumaSE;
	float edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);
	float edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);
	float edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;
	float edgeVert3 = (-2.0 * lumaS) + lumaSWSE;
	float edgeHorz = abs(edgeHorz3) + edgeHorz4;
	float edgeVert = abs(edgeVert3) + edgeVert4;

	float subpixNWSWNESE = lumaNWSW + lumaNESE;
	float lengthSign = RcpFrame.x;
	bool horzSpan = edgeHorz >= edgeVert;
	float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;

	if (!horzSpan) lumaN = lumaW;
	if (!horzSpan) lumaS = lumaE;
	if (horzSpan) lengthSign = RcpFrame.y;
	float subpixB = (subpixA * (1.0 / 12.0)) - lumaM;

	float gradientN = lumaN - lumaM;
	float gradientS = lumaS - lumaM;
	float lumaNN = lumaN + lumaM;
	float lumaSS = lumaS + lumaM;
	bool pairN = abs(gradientN) >= abs(gradientS);
	float gradient = max(abs(gradientN), abs(gradientS));
	if (pairN) lengthSign = -lengthSign;
	float subpixC = saturate(abs(subpixB) * subpixRcpRange);

	float2 posB;
	posB.x = posM.x;
	posB.y = posM.y;
	float2 offNP;
	offNP.x = (!horzSpan) ? 0.0 : RcpFrame.x;
	offNP.y = (horzSpan) ? 0.0 : RcpFrame.y;
	if (!horzSpan) posB.x += lengthSign * 0.5;
	if (horzSpan) posB.y += lengthSign * 0.5;

	float2 posN;
	posN.x = posB.x - offNP.x * FXAA_QUALITY__P0;
	posN.y = posB.y - offNP.y * FXAA_QUALITY__P0;
	float2 posP;
	posP.x = posB.x + offNP.x * FXAA_QUALITY__P0;
	posP.y = posB.y + offNP.y * FXAA_QUALITY__P0;
	float subpixD = ((-2.0)*subpixC) + 3.0;
	float lumaEndN = FxaaLuma(SampleLocation(posN));
	float subpixE = subpixC * subpixC;
	float lumaEndP = FxaaLuma(SampleLocation(posP));

	if (!pairN) lumaNN = lumaSS;
	float gradientScaled = gradient * 1.0 / 4.0;
	float lumaMM = lumaM - lumaNN * 0.5;
	float subpixF = subpixD * subpixE;
	bool lumaMLTZero = lumaMM < 0.0;

	lumaEndN -= lumaNN * 0.5;
	lumaEndP -= lumaNN * 0.5;
	bool doneN = abs(lumaEndN) >= gradientScaled;
	bool doneP = abs(lumaEndP) >= gradientScaled;
	if (!doneN) posN.x -= offNP.x * FXAA_QUALITY__P1;
	if (!doneN) posN.y -= offNP.y * FXAA_QUALITY__P1;
	bool doneNP = (!doneN) || (!doneP);
	if (!doneP) posP.x += offNP.x * FXAA_QUALITY__P1;
	if (!doneP) posP.y += offNP.y * FXAA_QUALITY__P1;

	if (doneNP) {
		if (!doneN) lumaEndN = FxaaLuma(SampleLocation(posN.xy));
		if (!doneP) lumaEndP = FxaaLuma(SampleLocation(posP.xy));
		if (!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
		if (!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
		doneN = abs(lumaEndN) >= gradientScaled;
		doneP = abs(lumaEndP) >= gradientScaled;
		if (!doneN) posN.x -= offNP.x * FXAA_QUALITY__P2;
		if (!doneN) posN.y -= offNP.y * FXAA_QUALITY__P2;
		doneNP = (!doneN) || (!doneP);
		if (!doneP) posP.x += offNP.x * FXAA_QUALITY__P2;
		if (!doneP) posP.y += offNP.y * FXAA_QUALITY__P2;

		if (doneNP) {
			if (!doneN) lumaEndN = FxaaLuma(SampleLocation(posN.xy));
			if (!doneP) lumaEndP = FxaaLuma(SampleLocation(posP.xy));
			if (!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
			if (!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
			doneN = abs(lumaEndN) >= gradientScaled;
			doneP = abs(lumaEndP) >= gradientScaled;
			if (!doneN) posN.x -= offNP.x * FXAA_QUALITY__P3;
			if (!doneN) posN.y -= offNP.y * FXAA_QUALITY__P3;
			doneNP = (!doneN) || (!doneP);
			if (!doneP) posP.x += offNP.x * FXAA_QUALITY__P3;
			if (!doneP) posP.y += offNP.y * FXAA_QUALITY__P3;

			if (doneNP) {
				if (!doneN) lumaEndN = FxaaLuma(SampleLocation(posN.xy));
				if (!doneP) lumaEndP = FxaaLuma(SampleLocation(posP.xy));
				if (!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
				if (!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
				doneN = abs(lumaEndN) >= gradientScaled;
				doneP = abs(lumaEndP) >= gradientScaled;
				if (!doneN) posN.x -= offNP.x * FXAA_QUALITY__P4;
				if (!doneN) posN.y -= offNP.y * FXAA_QUALITY__P4;
				doneNP = (!doneN) || (!doneP);
				if (!doneP) posP.x += offNP.x * FXAA_QUALITY__P4;
				if (!doneP) posP.y += offNP.y * FXAA_QUALITY__P4;

				if (doneNP) {
					if (!doneN) lumaEndN = FxaaLuma(SampleLocation(posN.xy));
					if (!doneP) lumaEndP = FxaaLuma(SampleLocation(posP.xy));
					if (!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
					if (!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
					doneN = abs(lumaEndN) >= gradientScaled;
					doneP = abs(lumaEndP) >= gradientScaled;
					if (!doneN) posN.x -= offNP.x * FXAA_QUALITY__P5;
					if (!doneN) posN.y -= offNP.y * FXAA_QUALITY__P5;
					doneNP = (!doneN) || (!doneP);
					if (!doneP) posP.x += offNP.x * FXAA_QUALITY__P5;
					if (!doneP) posP.y += offNP.y * FXAA_QUALITY__P5;

					if (doneNP) {
						if (!doneN) lumaEndN = FxaaLuma(SampleLocation(posN.xy));
						if (!doneP) lumaEndP = FxaaLuma(SampleLocation(posP.xy));
						if (!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
						if (!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
						doneN = abs(lumaEndN) >= gradientScaled;
						doneP = abs(lumaEndP) >= gradientScaled;
						if (!doneN) posN.x -= offNP.x * FXAA_QUALITY__P6;
						if (!doneN) posN.y -= offNP.y * FXAA_QUALITY__P6;
						doneNP = (!doneN) || (!doneP);
						if (!doneP) posP.x += offNP.x * FXAA_QUALITY__P6;
						if (!doneP) posP.y += offNP.y * FXAA_QUALITY__P6;

						if (doneNP) {
							if (!doneN) lumaEndN = FxaaLuma(SampleLocation(posN.xy));
							if (!doneP) lumaEndP = FxaaLuma(SampleLocation(posP.xy));
							if (!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
							if (!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
							doneN = abs(lumaEndN) >= gradientScaled;
							doneP = abs(lumaEndP) >= gradientScaled;
							if (!doneN) posN.x -= offNP.x * FXAA_QUALITY__P7;
							if (!doneN) posN.y -= offNP.y * FXAA_QUALITY__P7;
							doneNP = (!doneN) || (!doneP);
							if (!doneP) posP.x += offNP.x * FXAA_QUALITY__P7;
							if (!doneP) posP.y += offNP.y * FXAA_QUALITY__P7;

							if (doneNP) {
								if (!doneN) lumaEndN = FxaaLuma(SampleLocation(posN.xy));
								if (!doneP) lumaEndP = FxaaLuma(SampleLocation(posP.xy));
								if (!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
								if (!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
								doneN = abs(lumaEndN) >= gradientScaled;
								doneP = abs(lumaEndP) >= gradientScaled;
								if (!doneN) posN.x -= offNP.x * FXAA_QUALITY__P8;
								if (!doneN) posN.y -= offNP.y * FXAA_QUALITY__P8;
								doneNP = (!doneN) || (!doneP);
								if (!doneP) posP.x += offNP.x * FXAA_QUALITY__P8;
								if (!doneP) posP.y += offNP.y * FXAA_QUALITY__P8;
							}
						}
					}
				}
			}
		}
	}

	float dstN = posM.x - posN.x;
	float dstP = posP.x - posM.x;
	if (!horzSpan) dstN = posM.y - posN.y;
	if (!horzSpan) dstP = posP.y - posM.y;

	bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
	float spanLength = (dstP + dstN);
	bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
	float spanLengthRcp = 1.0 / spanLength;

	bool directionN = dstN < dstP;
	float dst = min(dstN, dstP);
	bool goodSpan = directionN ? goodSpanN : goodSpanP;
	float subpixG = subpixF * subpixF;
	float pixelOffset = (dst * (-spanLengthRcp)) + 0.5;
	float subpixH = subpixG * Subpix;

	float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
	float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);
	if (!horzSpan) posM.x += pixelOffsetSubpix * lengthSign;
	if (horzSpan) posM.y += pixelOffsetSubpix * lengthSign;

	if (GetOption(C_FXAA_SHOW_EDGES) == 1)
	{
		return -rgbyM;
	}
	else
	{
		return float4(SampleLocation(posM).xyz, lumaM);
	}
}

float4 FxaaPass(float4 color)
{
	color = FxaaPixelShader(1.0 / GetResolution(), GetOption(A_FXAA_SUBPIX_MAX), GetOption(B_FXAA_EDGE_THRESHOLD), 0.000);

	return color;
}

/*------------------------------------------------------------------------------
[PAINT SHADING CODE SECTION]
------------------------------------------------------------------------------*/

float3 PaintShading(float3 color, float2 texcoord)
{
	float2 pixelSize = GetInvResolution();
	if (GetOption(PaintMethod) == 1)
	{
		float2	tex;
		int	k, j, lum, cmax = 0;

		float	C0 = 0, C1 = 0, C2 = 0, C3 = 0, C4 = 0, C5 = 0, C6 = 0, C7 = 0, C8 = 0, C9 = 0;
		float3	A = float3(0.0,0.0,0.0), B = float3(0.0, 0.0, 0.0), C = float3(0.0, 0.0, 0.0), D = float3(0.0, 0.0, 0.0), E = float3(0.0, 0.0, 0.0), F = float3(0.0, 0.0, 0.0), G = float3(0.0, 0.0, 0.0), H = float3(0.0, 0.0, 0.0), I = float3(0.0, 0.0, 0.0), J = float3(0.0, 0.0, 0.0), shade = float3(0.0, 0.0, 0.0);

		for (k = int(-PaintRadius); k < (int(PaintRadius) + 1); k++) {
			for (j = int(-PaintRadius); j < (int(PaintRadius) + 1); j++) {

				tex.x = texcoord.x + pixelSize.x * k;
				tex.y = texcoord.y + pixelSize.y * j;

				shade = SampleLocation(tex).xyz;

				lum = int(AvgLuminance(shade) * 9.0);

				C0 = (lum == 0) ? C0 + 1 : C0;
				C1 = (lum == 1) ? C1 + 1 : C1;
				C2 = (lum == 2) ? C2 + 1 : C2;
				C3 = (lum == 3) ? C3 + 1 : C3;
				C4 = (lum == 4) ? C4 + 1 : C4;
				C5 = (lum == 5) ? C5 + 1 : C5;
				C6 = (lum == 6) ? C6 + 1 : C6;
				C7 = (lum == 7) ? C7 + 1 : C7;
				C8 = (lum == 8) ? C8 + 1 : C8;
				C9 = (lum == 9) ? C9 + 1 : C9;

				A = (lum == 0) ? A + shade : A;
				B = (lum == 1) ? B + shade : B;
				C = (lum == 2) ? C + shade : C;
				D = (lum == 3) ? D + shade : D;
				E = (lum == 4) ? E + shade : E;
				F = (lum == 5) ? F + shade : F;
				G = (lum == 6) ? G + shade : G;
				H = (lum == 7) ? H + shade : H;
				I = (lum == 8) ? I + shade : I;
				J = (lum == 9) ? J + shade : J;
			}
		}

		if (C0 > cmax) { cmax = int(C0); color.xyz = A / cmax; }
		if (C1 > cmax) { cmax = int(C1); color.xyz = B / cmax; }
		if (C2 > cmax) { cmax = int(C2); color.xyz = C / cmax; }
		if (C3 > cmax) { cmax = int(C3); color.xyz = D / cmax; }
		if (C4 > cmax) { cmax = int(C4); color.xyz = E / cmax; }
		if (C5 > cmax) { cmax = int(C5); color.xyz = F / cmax; }
		if (C6 > cmax) { cmax = int(C6); color.xyz = G / cmax; }
		if (C7 > cmax) { cmax = int(C7); color.xyz = H / cmax; }
		if (C8 > cmax) { cmax = int(C8); color.xyz = I / cmax; }
		if (C9 > cmax) { cmax = int(C9); color.xyz = J / cmax; }
	}
	else
	{
		int j, i;
		float2 screenSize = GetResolution();
		float3 m0, m1, m2, m3, k0, k1, k2, k3, shade;
		float n = float((PaintRadius + 1.0) * (PaintRadius + 1.0));

		for (j = int(-PaintRadius); j <= 0; ++j) {
			for (i = int(-PaintRadius); i <= 0; ++i) {

				shade = SampleLocation(texcoord + float2(i, j) / screenSize).rgb;
				m0 += shade; k0 += shade * shade;
			}
		}

		for (j = int(-PaintRadius); j <= 0; ++j) {
			for (i = 0; i <= int(PaintRadius); ++i) {
				shade = SampleLocation(texcoord + float2(i, j) / screenSize).rgb;
				m1 += shade; k1 += shade * shade;
			}
		}

		for (j = 0; j <= int(PaintRadius); ++j) {
			for (i = 0; i <= int(PaintRadius); ++i) {
				shade = SampleLocation(texcoord + float2(i, j) / screenSize).rgb;
				m2 += shade; k2 += shade * shade;
			}
		}

		float min_sigma2 = 1e+2;
		m0 /= n; k0 = abs(k0 / n - m0 * m0);

		float sigma2 = k0.r + k0.g + k0.b;
		if (sigma2 < min_sigma2) {
			min_sigma2 = sigma2; color = m0;
		}

		m1 /= n; k1 = abs(k1 / n - m1 * m1);
		sigma2 = k1.r + k1.g + k1.b;

		if (sigma2 < min_sigma2) {
			min_sigma2 = sigma2;
			color = m1;
		}

		m2 /= n; k2 = abs(k2 / n - m2 * m2);
		sigma2 = k2.r + k2.g + k2.b;

		if (sigma2 < min_sigma2) {
			min_sigma2 = sigma2;
			color = m2;
		}
	}
	return color;
}

float4 PaintPass(float4 color, float2 texcoord)
{
	float3 paint = PaintShading(color.rgb, texcoord);
	color.rgb = lerp(color.rgb, paint, GetOption(PaintStrength));
	color.a = AvgLuminance(color.rgb);

	return color;
}

/*------------------------------------------------------------------------------
[PX BORDER CODE SECTION]
------------------------------------------------------------------------------*/

float4 BorderPass(float4 colorInput, float2 tex)
{
	float3 border_color_float = GetOption(BorderColor);

	float2 border = (GetInvResolution().xy * GetOption(BorderWidth));
	float2 within_border = saturate((-tex * tex + tex) - (-border * border + border));

#if API_OPENGL == 1
	bvec2 cond = notEqual(within_border, vec2(0.0f, 0.0f));
	colorInput.rgb = all(cond) ? colorInput.rgb : border_color_float;
#else
	colorInput.rgb = all(within_border) ? colorInput.rgb : border_color_float;
#endif

	return colorInput;

}


/*------------------------------------------------------------------------------
[MAIN() & COMBINE PASS CODE SECTION]
------------------------------------------------------------------------------*/

void main()
{
	float4 color = Sample();
	Randomize();
	if (!OptionEnabled(DISABLE_EFFECTS))
	{
		if (OptionEnabled(A_FXAA_PASS)) { color = FxaaPass(color); }
		if (OptionEnabled(A_BILINEAR_FILTER)) { color = BilinearPass(color); }
		if (OptionEnabled(B_BICUBLIC_SCALER)) { color = BicubicScalerPass(color); }
		if (OptionEnabled(B_LANCZOS_SCALER)) { color = LanczosScalerPass(color); }
		if (OptionEnabled(G_TEXTURE_SHARPEN)) { color = TexSharpenPass(color); }
		if (OptionEnabled(J_CEL_SHADING)) { color = CelPass(color); }
		if (OptionEnabled(F_GAMMA_CORRECTION)) { color = GammaPass(color); }
		if (OptionEnabled(H_PIXEL_VIBRANCE)) { color = VibrancePass(color); }
		if (OptionEnabled(C_TONEMAP_PASS)) { color = TonemapPass(color); }
		if (OptionEnabled(D_COLOR_CORRECTION)) { color = CorrectionPass(color); }
		if (OptionEnabled(E_FILMIC_PROCESS)) { color = CrossPass(color); }
		if (OptionEnabled(B_BLOOM_PASS)) { color = BloomPass(color); }
		if (OptionEnabled(I_CONTRAST_ENHANCEMENT)) { color = ContrastPass(color); }
		if (OptionEnabled(L_FILM_GRAIN_PASS)) { color = GrainPass(color); }
		if (OptionEnabled(L_VIGNETTE_PASS)) { color = VignettePass(color); }
		if (OptionEnabled(K_SCAN_LINES)) { color = ScanlinesPass(color); }
		if (OptionEnabled(M_DITHER_PASS)) { color = DitherPass(color); }
		if (OptionEnabled(J_PAINT_SHADING)) { color = PaintPass(color, GetCoordinates()); }
		if (OptionEnabled(N_PIXEL_BORDER)) { color = BorderPass(color, GetCoordinates()); }
	}

	SetOutput(color);
}