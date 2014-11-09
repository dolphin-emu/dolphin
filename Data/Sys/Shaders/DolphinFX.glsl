/*===============================================================================*\
|########################     [Dolphin FX Suite 2.00]      #######################|
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
||          GNU General Public License for more details. (C)2014                 ||
||                                                                               ||
|#################################################################################|
\*===============================================================================*/
#extension GL_ARB_gpu_shader5 : enable

/*
[configuration]

[OptionBool]
GUIName = Disable All Effects (Debugging)
OptionName = DISABLE_EFFECTS
DefaultValue = false

[OptionBool]
GUIName = Scaling Filters
OptionName = A_SCALARS
DefaultValue = false

[OptionRangeInteger]
GUIName = Bicubic Scaling
OptionName = A_BICUBLIC_SCALAR
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = A_SCALARS

[OptionRangeInteger]
GUIName = Lanczos Scaling
OptionName = A_LANCZOS_SCALAR
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = A_SCALARS

[OptionBool]
GUIName = Note: You can use FXAA or a Scaling Filter, not both together.
OptionName = PASS_WARN
DefaultValue = true
DependentOption = A_SCALARS

[OptionBool]
GUIName = FXAA
OptionName = A_FXAA_PASS
DefaultValue = true

[OptionRangeFloat]
GUIName = SubpixelMax
OptionName = FXAA_SUBPIX_MAX
MinValue = 0.00
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.25
DependentOption = A_FXAA_PASS

[OptionRangeFloat]
GUIName = EdgeThreshold
OptionName = FXAA_XEDGE_THRESHOLD
MinValue = 0.010
MaxValue = 0.500
StepAmount = 0.001
DefaultValue = 0.050
DependentOption = A_FXAA_PASS

[OptionRangeInteger]
GUIName = ShowEdgeDetection
OptionName = FXAA_ZSHOW_EDGES
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
MaxValue = 4
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
MaxValue = 1.500
StepAmount = 0.010
DefaultValue = 1.000
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomWidth
OptionName = D_BLOOM_WIDTH
MinValue = 1.000
MaxValue = 8.000
StepAmount = 0.100
DefaultValue = 4.000
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomReds
OptionName = E_BLOOM_REDS
MinValue = 0.100
MaxValue = 8.000
StepAmount = 0.100
DefaultValue = 1.000
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomGreens
OptionName = F_BLOOM_GREENS
MinValue = 0.100
MaxValue = 8.000
StepAmount = 0.100
DefaultValue = 1.000
DependentOption = B_BLOOM_PASS

[OptionRangeFloat]
GUIName = BloomBlues
OptionName = G_BLOOM_BLUES
MinValue = 0.100
MaxValue = 8.000
StepAmount = 0.100
DefaultValue = 1.000
DependentOption = B_BLOOM_PASS

[OptionBool]
GUIName = Scene Tonemapping
OptionName = C_TONEMAP_PASS
DefaultValue = true

[OptionRangeInteger]
GUIName = TonemapType
OptionName = A_TONEMAP_TYPE
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = ToneAmount
OptionName = B_TONE_AMOUNT
MinValue = 0.05
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 0.40
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = BlackLevels
OptionName = C_BLACK_LEVELS
MinValue = 0.05
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.30
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = Exposure
OptionName = D_EXPOSURE
MinValue = 0.25
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = Luminance
OptionName = E_LUMINANCE
MinValue = 0.25
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = C_TONEMAP_PASS

[OptionRangeFloat]
GUIName = WhitePoint
OptionName = F_WHITEPOINT
MinValue = 0.25
MaxValue = 2.00
StepAmount = 0.01
DefaultValue = 1.00
DependentOption = C_TONEMAP_PASS

[OptionBool]
GUIName = Colour Correction
OptionName = D_COLOUR_CORRECTION
DefaultValue = true

[OptionRangeInteger]
GUIName = CorrectionPalette
OptionName = A_PALETTE
MinValue = 0
MaxValue = 3
StepAmount = 1
DefaultValue = 1
DependentOption = D_COLOUR_CORRECTION

[OptionRangeFloat]
GUIName = RedCorrection
OptionName = B_RED_CORRECTION
MinValue = 0.100
MaxValue = 8.000
StepAmount = 0.100
DefaultValue = 1.000
DependentOption = D_COLOUR_CORRECTION

[OptionRangeFloat]
GUIName = GreenCorrection
OptionName = C_GREEN_CORRECTION
MinValue = 0.100
MaxValue = 8.000
StepAmount = 0.100
DefaultValue = 1.000
DependentOption = D_COLOUR_CORRECTION

[OptionRangeFloat]
GUIName = BlueCorrection
OptionName = D_BLUE_CORRECTION
MinValue = 0.100
MaxValue = 8.000
StepAmount = 0.100
DefaultValue = 1.000
DependentOption = D_COLOUR_CORRECTION

[OptionBool]
GUIName = Filmic Process
OptionName = E_FILMIC_PROCESS
DefaultValue = true

[OptionRangeInteger]
GUIName = FilmicProcess
OptionName = A_FILMIC
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 0
DependentOption = E_FILMIC_PROCESS

[OptionRangeFloat]
GUIName = RedShift
OptionName = B_RED_SHIFT
MinValue = 0.10
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.50
DependentOption = E_FILMIC_PROCESS

[OptionRangeFloat]
GUIName = GreenShift
OptionName = C_GREEN_SHIFT
MinValue = 0.10
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.45
DependentOption = E_FILMIC_PROCESS

[OptionRangeFloat]
GUIName = BlueShift
OptionName = D_BLUE_SHIFT
MinValue = 0.10
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.45
DependentOption = E_FILMIC_PROCESS

[OptionRangeFloat]
GUIName = ShiftRatio
OptionName = E_SHIFT_RATIO
MinValue = 0.00
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.30
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
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.50
DependentOption = G_TEXTURE_SHARPEN

[OptionRangeFloat]
GUIName = SharpenClamp
OptionName = B_SHARPEN_CLAMP
MinValue = 0.000
MaxValue = 0.500
StepAmount = 0.001
DefaultValue = 0.020
DependentOption = G_TEXTURE_SHARPEN

[OptionRangeFloat]
GUIName = SharpenBias
OptionName = C_SHARPEN_BIAS
MinValue = 1.00
MaxValue = 4.00
StepAmount = 0.05
DefaultValue = 1.25
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
MinValue = 0.00
MaxValue = 1.00
StepAmount = 0.01
DefaultValue = 0.20
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
DefaultValue = 1.50
DependentOption = J_CEL_SHADING

[OptionRangeFloat]
GUIName = EdgeFilter
OptionName = B_EDGE_FILTER
MinValue = 0.15
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
GUIName = LumaConversion
OptionName = F_LUMA_CONVERSION
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 1
DependentOption = J_CEL_SHADING

[OptionRangeInteger]
GUIName = ColorRounding
OptionName = G_COLOR_ROUNDING
MinValue = 0
MaxValue = 1
StepAmount = 1
DefaultValue = 1
DependentOption = J_CEL_SHADING

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
MinValue = 0.10
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
MaxValue = 4.00
StepAmount = 0.01
DefaultValue = 1.00
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

[/configuration]
*/

/*------------------------------------------------------------------------------
                            [GLOBALS|FUNCTIONS]
------------------------------------------------------------------------------*/

float2 texcoord = GetCoordinates();
float2 screenSize = GetResolution();
float2 pixelSize = GetInvResolution();
uint timer = GetTime();

#define mul(x, y) y * x
#define FIX(c) max(abs(c), 1e-5)
#define saturate(x) clamp(x, 0.0, 1.0)

const float PI = 3.1415926535897932384626433832795;
float2 invDefocus = float2(1.0 / 3840.0, 1.0 / 2160.0);
const float3 lumCoeff = float3(0.2126729, 0.7151522, 0.0721750);

float RGBLuminance(float3 color)
{
    return dot(color.rgb, lumCoeff);
}

/*------------------------------------------------------------------------------
                         [BICUBIC SCALAR CODE SECTION]
------------------------------------------------------------------------------*/

float4 BicubicScalar(sampler2D tex, float2 uv, float2 texSize)
{
    float2 rec_nrCP = float2(1.0/texSize.x, 1.0/texSize.y);

    float2 coord_hg = uv * texSize - 0.5;
    float2 index = floor(coord_hg);
    float2 f = coord_hg - index;

    mat4 M = mat4( -1.0, 3.0,-3.0, 1.0, 3.0,-6.0, 3.0, 0.0,
                   -3.0, 0.0, 3.0, 0.0, 1.0, 4.0, 1.0, 0.0 );
    M /= 6.0;

    float4 wx = M * float4(f.x*f.x*f.x, f.x*f.x, f.x, 1.0);
    float4 wy = M * float4(f.y*f.y*f.y, f.y*f.y, f.y, 1.0);
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

    coord00 = (coord00 + 0.5) * rec_nrCP;
    coord10 = (coord10 + 0.5) * rec_nrCP;
    coord01 = (coord01 + 0.5) * rec_nrCP;
    coord11 = (coord11 + 0.5) * rec_nrCP;

    float4 tex00 = textureLod(tex, coord00, 0.0);
    float4 tex10 = textureLod(tex, coord10, 0.0);
    float4 tex01 = textureLod(tex, coord01, 0.0);
    float4 tex11 = textureLod(tex, coord11, 0.0);

    tex00 = lerp(tex01, tex00, float4(g0.y, g0.y, g0.y, g0.y));
    tex10 = lerp(tex11, tex10, float4(g0.y, g0.y, g0.y, g0.y));
    float4 res = lerp(tex10, tex00, float4(g0.x, g0.x, g0.x, g0.x));

    return res;
}

float4 BicubicScalarPass(float4 color)
{
    color = BicubicScalar(samp9, texcoord, screenSize);
    return color;
}

/*------------------------------------------------------------------------------
                         [LANCZOS SCALAR CODE SECTION]
------------------------------------------------------------------------------*/

float3 pixel(float xpos, float ypos)
{
	return texture(samp9, float2(xpos, ypos)).rgb;
}

float4 weight4(float x)
{
	float4 saample = FIX(PI * float4(1.0 + x, x, 1.0 - x, 2.0 - x));
	float4 ret = sin(saample) * sin(saample / 2.0) / (saample * saample);

	return ret / dot(ret, float4(1.0, 1.0, 1.0, 1.0));
}

float3 line_run(float ypos, float4 xpos, float4 linetaps)
{
    return mat4x3(
    pixel(xpos.x, ypos),
    pixel(xpos.y, ypos),
    pixel(xpos.z, ypos),
    pixel(xpos.w, ypos)) * linetaps;
}

float4 LanczosScalar(float2 inputSize)
{
    float2 stepxy = 1.0 / inputSize;
    float2 pos = texcoord + stepxy * 0.5;
    float2 f = frac(pos / stepxy);

    float2 xystart = (-1.0 - f) * stepxy + pos;
    float4 xpos = float4(
    xystart.x,
    xystart.x + stepxy.x,
    xystart.x + stepxy.x * 2.0,
    xystart.x + stepxy.x * 3.0);

    float4 linetaps = weight4(f.x);
    float4 columntaps = weight4(f.y);

    return float4(mat4x3(
    line_run(xystart.y, xpos, linetaps),
    line_run(xystart.y + stepxy.y, xpos, linetaps),
    line_run(xystart.y + stepxy.y * 2.0, xpos, linetaps),
    line_run(xystart.y + stepxy.y * 3.0, xpos, linetaps)) * columntaps, 1.0);
}

float4 LanczosScalarPass(float4 color)
{
    color = LanczosScalar(screenSize);
    return color;
}

/*------------------------------------------------------------------------------
                       [GAMMA CORRECTION CODE SECTION]
------------------------------------------------------------------------------*/

float3 RGBGammaToLinear(float3 color, float gamma)
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

float3 LinearToRGBGamma(float3 color, float gamma)
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
    const float GammaConst = 2.233;
    color.rgb = RGBGammaToLinear(color.rgb, GammaConst);
    color.rgb = LinearToRGBGamma(color.rgb, GetOption(A_GAMMA));
    color.a = RGBLuminance(color.rgb);

    return color;
}

/*------------------------------------------------------------------------------
                        [BLENDED BLOOM CODE SECTION]
------------------------------------------------------------------------------*/

float4 PyramidFilter(sampler2D tex, float2 texcoord, float2 width)
{
    float4 color = texture(tex, texcoord + float2(0.5, 0.5) * width);
    color += texture(tex, texcoord + float2(-0.5, 0.5) * width);
    color += texture(tex, texcoord + float2(0.5, -0.5) * width);
    color += texture(tex, texcoord + float2(-0.5, -0.5) * width);
    color *= 0.25;

    return color;
}

float3 BloomCorrection(float3 color)
{
    float X = 1.0 / (1.0 + exp(GetOption(E_BLOOM_REDS) / 2.0));
    float Y = 1.0 / (1.0 + exp(GetOption(F_BLOOM_GREENS) / 2.0));
    float Z = 1.0 / (1.0 + exp(GetOption(G_BLOOM_BLUES) / 2.0));

    color.r = (1.0 / (1.0 + exp(-GetOption(E_BLOOM_REDS) * (color.r - 0.5))) - X) / (1.0 - 2.0 * X);
    color.g = (1.0 / (1.0 + exp(-GetOption(F_BLOOM_GREENS) * (color.g - 0.5))) - Y) / (1.0 - 2.0 * Y);
    color.b = (1.0 / (1.0 + exp(-GetOption(G_BLOOM_BLUES) * (color.b - 0.5))) - Z) / (1.0 - 2.0 * Z);

    return color;
}

float3 Blend(float3 color, float3 bloom)
{
    float3 BlendAddLight = (color + bloom) * 0.75f;
    float3 BlendScreen = (color + bloom) - (color * bloom);
    float3 BlendLuma = lerp((color * bloom), (1.0 - ((1.0 - color) * (1.0 - bloom))), RGBLuminance(color + bloom));

    float3 BlendGlow = step(0.5, color);
    BlendGlow = lerp((color + bloom) - (color * bloom), (bloom + bloom) - (bloom * bloom), BlendGlow);

    float3 BlendOverlay = step(0.5, color);
    BlendOverlay = lerp((color * bloom * 2.0), (1.0 - (2.0 * (1.0 - color) * (1.0 - bloom))), BlendOverlay);

         if (GetOption(A_BLOOM_TYPE) == 0) { return BlendGlow; }
    else if (GetOption(A_BLOOM_TYPE) == 1) { return BlendAddLight; }
    else if (GetOption(A_BLOOM_TYPE) == 2) { return BlendScreen; }
    else if (GetOption(A_BLOOM_TYPE) == 3) { return BlendLuma; }
    else if (GetOption(A_BLOOM_TYPE) == 4) { return BlendOverlay; }
}

float4 BloomPass(float4 color)
{
    const float defocus = 1.25;
    float4 bloom = PyramidFilter(samp9, texcoord, pixelSize * defocus);

    float2 dx = float2(invDefocus.x * GetOption(D_BLOOM_WIDTH), 0.0);
    float2 dy = float2(0.0, invDefocus.y * GetOption(D_BLOOM_WIDTH));

    float2 mdx = (dx * 2.0);
    float2 mdy = (dy * 2.0);

    float4 bloomBlend = bloom * 0.22520613262190495;

    bloomBlend += 0.002589001911021066 * texture(samp9, texcoord - mdx + mdy);
    bloomBlend += 0.010778807494659370 * texture(samp9, texcoord - dx + mdy);
    bloomBlend += 0.024146616900339800 * texture(samp9, texcoord + mdy);
    bloomBlend += 0.010778807494659370 * texture(samp9, texcoord + dx + mdy);
    bloomBlend += 0.002589001911021066 * texture(samp9, texcoord + mdx + mdy);

    bloomBlend += 0.010778807494659370 * texture(samp9, texcoord - mdx + dy);
    bloomBlend += 0.044875475183061630 * texture(samp9, texcoord - dx + dy);
    bloomBlend += 0.100529757860782610 * texture(samp9, texcoord + dy);
    bloomBlend += 0.044875475183061630 * texture(samp9, texcoord + dx + dy);
    bloomBlend += 0.010778807494659370 * texture(samp9, texcoord + mdx + dy);

    bloomBlend += 0.024146616900339800 * texture(samp9, texcoord - mdx);
    bloomBlend += 0.100529757860782610 * texture(samp9, texcoord - dx);
    bloomBlend += 0.100529757860782610 * texture(samp9, texcoord + dx);
    bloomBlend += 0.024146616900339800 * texture(samp9, texcoord + mdx);

    bloomBlend += 0.010778807494659370 * texture(samp9, texcoord - mdx - dy);
    bloomBlend += 0.044875475183061630 * texture(samp9, texcoord - dx - dy);
    bloomBlend += 0.100529757860782610 * texture(samp9, texcoord - dy);
    bloomBlend += 0.044875475183061630 * texture(samp9, texcoord + dx - dy);
    bloomBlend += 0.010778807494659370 * texture(samp9, texcoord + mdx - dy);

    bloomBlend += 0.002589001911021066 * texture(samp9, texcoord - mdx - mdy);
    bloomBlend += 0.010778807494659370 * texture(samp9, texcoord - dx - mdy);
    bloomBlend += 0.024146616900339800 * texture(samp9, texcoord - mdy);
    bloomBlend += 0.010778807494659370 * texture(samp9, texcoord + dx - mdy);
    bloomBlend += 0.002589001911021066 * texture(samp9, texcoord + mdx - mdy);
    bloomBlend = lerp(color, bloomBlend, GetOption(C_BLEND_STRENGTH));

    bloom.rgb = Blend(bloom.rgb, bloomBlend.rgb);
    bloom.rgb = BloomCorrection(bloom.rgb);

    color.a = RGBLuminance(color.rgb);
    bloom.a = RGBLuminance(bloom.rgb);

    color = lerp(color, bloom, GetOption(B_BLOOM_STRENGTH));

    return color;
}

/*------------------------------------------------------------------------------
                 [COLOR CORRECTION/TONE MAPPING CODE SECTION]
------------------------------------------------------------------------------*/

float3 FilmicTonemap(float3 color)
{
    float3 Q = color.xyz;

    float A = 0.10;
    float B = GetOption(C_BLACK_LEVELS);
    float C = 0.10;
    float D = GetOption(B_TONE_AMOUNT);
    float E = 0.02;
    float F = 0.30;
    float W = GetOption(F_WHITEPOINT);

    float3 numerator = ((Q*(A*Q + C*B) + D*E) / (Q*(A*Q + B) + D*F)) - E / F;
    float3 denominator = ((float3(W)*(A*W + C*B) + D*E) / (W*(A*W + B) + D*F)) - E / F;

    color.xyz = numerator / denominator;

    return color;
}

float3 ColorShift(float3 color)
{
    float3 colMood;

    colMood.r = GetOption(B_RED_SHIFT);
    colMood.g = GetOption(C_GREEN_SHIFT);
    colMood.b = GetOption(D_BLUE_SHIFT);

    float fLum = RGBLuminance(color.rgb);
    colMood = lerp(float3(0.0), colMood, (fLum * 2.0));
    colMood = lerp(colMood, float3(1.0), (fLum - 0.5) * 2.0);
    float3 colOutput = lerp(color, colMood, (fLum * GetOption(E_SHIFT_RATIO)));

    return colOutput;
}

float3 ColorCorrection(float3 color)
{
    float X = 1.0 / (1.0 + exp(GetOption(B_RED_CORRECTION) / 2.0));
    float Y = 1.0 / (1.0 + exp(GetOption(C_GREEN_CORRECTION) / 2.0));
    float Z = 1.0 / (1.0 + exp(GetOption(D_BLUE_CORRECTION) / 2.0));

    color.r = (1.0 / (1.0 + exp(-GetOption(B_RED_CORRECTION) * (color.r - 0.5))) - X) / (1.0 - 2.0 * X);
    color.g = (1.0 / (1.0 + exp(-GetOption(C_GREEN_CORRECTION) * (color.g - 0.5))) - Y) / (1.0 - 2.0 * Y);
    color.b = (1.0 / (1.0 + exp(-GetOption(D_BLUE_CORRECTION) * (color.b - 0.5))) - Z) / (1.0 - 2.0 * Z);

    return color;
}

float4 TonemapPass(float4 color)
{
    const float delta = 0.001f;
    const float wpoint = pow(1.002f, 2.0f);
    
    if (OptionEnabled(D_COLOUR_CORRECTION) && GetOption(A_PALETTE) == 1)
    { color.rgb = ColorCorrection(color.rgb); }
    if (OptionEnabled(E_FILMIC_PROCESS) && GetOption(A_FILMIC) == 1)
    { color.rgb = ColorShift(color.rgb); }

    color.rgb = FilmicTonemap(color.rgb);

    // RGB -> XYZ conversion
    const mat3 RGB2XYZ = mat3( 0.4124564, 0.3575761, 0.1804375,
                               0.2126729, 0.7151522, 0.0721750,
                               0.0193339, 0.1191920, 0.9503041 );

    float3 XYZ = (color.rgb * RGB2XYZ);

    // XYZ -> Yxy conversion
    float3 Yxy;

    Yxy.r = XYZ.g;                              // copy luminance Y
    Yxy.g = XYZ.r / (XYZ.r + XYZ.g + XYZ.b);    // x = X / (X + Y + Z)
    Yxy.b = XYZ.g / (XYZ.r + XYZ.g + XYZ.b);    // y = Y / (X + Y + Z)

    if (OptionEnabled(D_COLOUR_CORRECTION) && GetOption(A_PALETTE) == 2)
    { Yxy.rgb = ColorCorrection(Yxy.rgb); }

    // (Lp) Map average luminance to the middlegrey zone by scaling pixel luminance
    float Lp;

    if (GetOption(A_TONEMAP_TYPE) == 1)
    { Lp = Yxy.r * FilmicTonemap(Yxy.rrr).r / RGBLuminance(Yxy.rrr) *
         GetOption(D_EXPOSURE) / (GetOption(E_LUMINANCE) + delta); }
    else
    { Lp = Yxy.r * GetOption(D_EXPOSURE) / (GetOption(E_LUMINANCE) + delta); }

    // (Ld) Scale all luminance within a displayable range of 0 to 1
    Yxy.r = (Lp * (1.0 + Lp / wpoint)) / (1.0 + Lp);

    // Yxy -> XYZ conversion
    XYZ.r = Yxy.r * Yxy.g / Yxy.b;                  // X = Y * x / y
    XYZ.g = Yxy.r;                                  // copy luminance Y
    XYZ.b = Yxy.r * (1.0 - Yxy.g - Yxy.b) / Yxy.b;  // Z = Y * (1-x-y) / y

    if (OptionEnabled(D_COLOUR_CORRECTION) && GetOption(A_PALETTE) == 3) 
    { XYZ.rgb = ColorCorrection(XYZ.rgb); }

    // XYZ -> RGB conversion
    const mat3 XYZ2RGB = mat3( 3.2404542,-1.5371385,-0.4985314,
                              -0.9692660, 1.8760108, 0.0415560,
                               0.0556434,-0.2040259, 1.0572252 );

    color.rgb = (XYZ * XYZ2RGB);
    color.a = RGBLuminance(color.rgb);

    return color;
}

/*------------------------------------------------------------------------------
                       [TEXTURE SHARPEN CODE SECTION]
------------------------------------------------------------------------------*/

float Cubic(float x)
{
    float x2 = x * x;
    float x3 = x2 * x;

    float cx = -x3 + 3.0 * x2 - 3.0 * x + 1.0;
    float cy = 3.0 * x3 - 6.0 * x2 + 4.0;
    float cz = -3.0 * x3 + 3.0 * x2 + 3.0 * x + 1.0;
    float cw = x3;

    return (lerp(cx, cy, 0.5) + lerp(cz, cw, 0.5)) / 6.0;
}

float4 SampleBicubic(sampler2D texSample, float2 texcoord)
{
    float texelSizeX = pixelSize.x * GetOption(C_SHARPEN_BIAS);
    float texelSizeY = pixelSize.y * GetOption(C_SHARPEN_BIAS);

    float4 nSum = float4(0.0, 0.0, 0.0, 0.0);
    float4 nDenom = float4(0.0, 0.0, 0.0, 0.0);

    float a = frac(texcoord.x * screenSize.x);
    float b = frac(texcoord.y * screenSize.y);

    int nX = int(texcoord.x * screenSize.x);
    int nY = int(texcoord.y * screenSize.y);

    float2 uvCoord = float2(float(nX) / screenSize.x, float(nY) / screenSize.y);

    for (int m = -1; m <= 2; m++)
    {
        for (int n = -1; n <= 2; n++)
        {
            float4 Samples = texture(texSample, uvCoord +
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

    float4 blurredColor = SampleBicubic(samp9, texcoord);
    float3 sharpenedColor = (color.rgb - blurredColor.rgb);

    float sharpenLuma = dot(sharpenedColor, calcSharpen);
    sharpenLuma = clamp(sharpenLuma, -GetOption(B_SHARPEN_CLAMP), GetOption(B_SHARPEN_CLAMP));

    color.rgb = color.rgb + sharpenLuma;
    color.a = RGBLuminance(color.rgb);

    if (GetOption(D_SEDGE_DETECTION) == 1)
    { color = (0.5f + (sharpenLuma * 4)).xxxx; }

    return color;
}

/*------------------------------------------------------------------------------
                           [VIBRANCE CODE SECTION]
------------------------------------------------------------------------------*/

float4 VibrancePass(float4 color)
{
    float luma = RGBLuminance(color.rgb);

    float colorMax = max(color.r, max(color.g, color.b));
    float colorMin = min(color.r, min(color.g, color.b));

    float colorSaturation = colorMax - colorMin;

    color.rgb = lerp(float3(luma), color.rgb, (1.0 + (GetOption(A_VIBRANCE) * (1.0 - (sign(GetOption(A_VIBRANCE)) * colorSaturation)))));
    color.a = RGBLuminance(color.rgb);

    return color; //Debug: return colorSaturation.xxxx;
}

/*------------------------------------------------------------------------------
                       [LOCAL CONTRAST CODE SECTION]
------------------------------------------------------------------------------*/

float4 ContrastPass(float4 color)
{
    float3 luma = float3(RGBLuminance(color.rgb));
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

    color.a = RGBLuminance(color.rgb);

    return saturate(color);
}

/*------------------------------------------------------------------------------
                       [CEL SHADING CODE SECTION]
------------------------------------------------------------------------------*/

float3 GetYUV(float3 rgb)
{
    mat3 RGB2YUV;
if (GetOption(F_LUMA_CONVERSION) == 0) {
    RGB2YUV = mat3(
              0.299, 0.587, 0.114,
             -0.14713, -0.28886f, 0.436,
              0.615, -0.51499, -0.10001 ); }

else {
    RGB2YUV = mat3(
              0.2126, 0.7152, 0.0722,
             -0.09991, -0.33609, 0.436,
              0.615, -0.55861, -0.05639 ); }

    return mul(RGB2YUV, rgb);
}

float3 GetRGB(float3 yuv)
{
    mat3 YUV2RGB;
if (GetOption(F_LUMA_CONVERSION) == 0) {
    YUV2RGB = mat3(
             1.000, 0.000, 1.13983,
             1.000, -0.39465, -0.58060,
             1.000, 2.03211, 0.000 ); }

else {
    YUV2RGB = mat3(
             1.000, 0.000, 1.28033,
             1.000, -0.21482, -0.38059,
             1.000, 2.12798, 0.000 ); }

    return mul(YUV2RGB, yuv);
}

float4 CelPass(float4 color)
{   
    float3 yuv;
    float3 sum = color.rgb;
    float2 pixel = pixelSize * GetOption(C_EDGE_THICKNESS);
    const float2 RoundingOffset = float2(0.25, 0.25);
    const float3 thresholds = float3(5.0, 8.0, 6.0);

    const int NUM = 9;
    float2 c[NUM] = float2[NUM](
    float2(-0.0078125, -0.0078125),
    float2(0.00, -0.0078125),
    float2(0.0078125, -0.0078125),
    float2(-0.0078125, 0.00),
    float2(0.00, 0.00),
    float2(0.0078125, 0.00),
    float2(-0.0078125, 0.0078125),
    float2(0.00, 0.0078125),
    float2(0.0078125, 0.0078125) );

    float3 col[NUM];
    float lum[NUM];

    for (int i = 0; i < NUM; i++)
    {
        col[i] = texture(samp9, texcoord + c[i] * RoundingOffset).rgb;

        if (GetOption(G_COLOR_ROUNDING) == 1) {
        col[i].r = (round(col[i].r * thresholds.r) / thresholds.r);
        col[i].g = (round(col[i].g * thresholds.g) / thresholds.g);
        col[i].b = (round(col[i].b * thresholds.b) / thresholds.b); }

        lum[i] = RGBLuminance(col[i].xyz);
        yuv = GetYUV(col[i]);

        if (GetOption(E_YUV_LUMA) == 0)
        { yuv.r = saturate(round(yuv.r * lum[i]) / thresholds.r + lum[i]); }
        else
        { yuv.r = saturate(round(yuv.r * thresholds.r) / thresholds.r + lum[i] / (255.0 / 5.0)); }

        yuv = GetRGB(yuv);
        sum += yuv;
    }

    float3 shadedColor = (sum / NUM);

    float edgeX = dot(texture(samp9, texcoord + pixel).rgb, lumCoeff);
    edgeX = dot(float4(texture(samp9, texcoord - pixel).rgb, edgeX), float4(lumCoeff, -1.0));

    float edgeY = dot(texture(samp9, texcoord + float2(pixel.x, -pixel.y)).rgb, lumCoeff);
    edgeY = dot(float4(texture(samp9, texcoord + float2(-pixel.x, pixel.y)).rgb, edgeY), float4(lumCoeff, -1.0));

    float edge = dot(float2(edgeX, edgeY), float2(edgeX, edgeY));

    if (GetOption(D_PALETTE_TYPE) == 0)
        { color.rgb = lerp(color.rgb, color.rgb + pow(edge, GetOption(B_EDGE_FILTER)) * -GetOption(A_EDGE_STRENGTH), GetOption(A_EDGE_STRENGTH)); }
    else if (GetOption(D_PALETTE_TYPE) == 1)
        { color.rgb = lerp(color.rgb + pow(edge, GetOption(B_EDGE_FILTER)) * -GetOption(A_EDGE_STRENGTH), shadedColor, 0.25); }
    else if (GetOption(D_PALETTE_TYPE) == 2)
        { color.rgb = lerp(shadedColor + edge * -GetOption(A_EDGE_STRENGTH), pow(edge, GetOption(B_EDGE_FILTER)) * -GetOption(A_EDGE_STRENGTH) + color.rgb, 0.5); }

    color.a = RGBLuminance(color.rgb);

    return saturate(color);
}

/*------------------------------------------------------------------------------
                           [FILM GRAIN CODE SECTION]
------------------------------------------------------------------------------*/

float width = screenSize.x;
float height = screenSize.y;

const float permTexUnit = 1.0/256.0;
const float permTexUnitHalf = 0.5/256.0;
    
float fade(in float t)
{
    return t*t*t*(t*(t*6.0 - 15.0) + 10.0);
}

float2 coordRot(in float2 tc, in float angle)
{
    float aspect = width/height;
    float rotX = ((tc.x * 2.0-1.0) * aspect * cos(angle)) - ((tc.y * 2.0-1.0) * sin(angle));
    float rotY = ((tc.y * 2.0-1.0) * cos(angle)) + ((tc.x * 2.0-1.0) * aspect * sin(angle));
    rotX = ((rotX/aspect) * 0.5+0.5);
    rotY = rotY * 0.5+0.5;

    return float2(rotX,rotY);
}

float4 randomize(in float2 texcoord) 
{
    float noise = (frac(sin(dot(texcoord, float2(12.9898, 78.233)*2.0)) * 43758.5453));

    float noiseR = fract(noise) * 2.0-1.0;
    float noiseG = fract(noise * 1.2154) * 2.0-1.0; 
    float noiseB = fract(noise * 1.3453) * 2.0-1.0;
    float noiseA = fract(noise * 1.3647) * 2.0-1.0;

    return float4(noiseR, noiseG, noiseB, noiseA);
}

float pnoise3D(in float3 p)
{
    float3 pi = permTexUnit*floor(p)+permTexUnitHalf;
    float3 pf = fract(p);

    // Noise contributions from (x=0, y=0), z=0 and z=1
    float perm00 = randomize(pi.xy).a ;
    float3  grad000 = randomize(float2(perm00, pi.z)).rgb * 4.0 - 1.0;
    float n000 = dot(grad000, pf);
    float3  grad001 = randomize(float2(perm00, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n001 = dot(grad001, pf - float3(0.0, 0.0, 1.0));

    // Noise contributions from (x=0, y=1), z=0 and z=1
    float perm01 = randomize(pi.xy + float2(0.0, permTexUnit)).a ;
    float3  grad010 = randomize(float2(perm01, pi.z)).rgb * 4.0 - 1.0;
    float n010 = dot(grad010, pf - float3(0.0, 1.0, 0.0));
    float3  grad011 = randomize(float2(perm01, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n011 = dot(grad011, pf - float3(0.0, 1.0, 1.0));

    // Noise contributions from (x=1, y=0), z=0 and z=1
    float perm10 = randomize(pi.xy + float2(permTexUnit, 0.0)).a ;
    float3  grad100 = randomize(float2(perm10, pi.z)).rgb * 4.0 - 1.0;
    float n100 = dot(grad100, pf - float3(1.0, 0.0, 0.0));
    float3  grad101 = randomize(float2(perm10, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n101 = dot(grad101, pf - float3(1.0, 0.0, 1.0));

    // Noise contributions from (x=1, y=1), z=0 and z=1
    float perm11 = randomize(pi.xy + float2(permTexUnit, permTexUnit)).a ;
    float3  grad110 = randomize(float2(perm11, pi.z)).rgb * 4.0 - 1.0;
    float n110 = dot(grad110, pf - float3(1.0, 1.0, 0.0));
    float3  grad111 = randomize(float2(perm11, pi.z + permTexUnit)).rgb * 4.0 - 1.0;
    float n111 = dot(grad111, pf - float3(1.0, 1.0, 1.0));

    float4 n_x = mix(float4(n000, n001, n010, n011), float4(n100, n101, n110, n111), fade(pf.x));

    float2 n_xy = mix(n_x.xy, n_x.zw, fade(pf.y));
    float n_xyz = mix(n_xy.x, n_xy.y, fade(pf.z));

    return n_xyz;
}

float4 GrainPass(float4 color)
{
    float3 rotOffset = float3(1.425,3.892,5.835); //rotation offset values	
    float2 rotCoordsR = coordRot(texcoord, timer + rotOffset.x);
    float3 noise = float3(pnoise3D(float3(rotCoordsR *
    float2(width / GetOption(A_GRAIN_SIZE), height / GetOption(A_GRAIN_SIZE)), 0.0)));

    if (GetOption(C_COLORED) == 1)
    {
        float2 rotCoordsG = coordRot(texcoord, timer + rotOffset.y);
        float2 rotCoordsB = coordRot(texcoord, timer + rotOffset.z);
        noise.g = mix(noise.r,pnoise3D(float3(rotCoordsG *
        float2(width / GetOption(A_GRAIN_SIZE), height / GetOption(A_GRAIN_SIZE)), 1.0)), GetOption(D_COLOR_AMOUNT));
        noise.b = mix(noise.r,pnoise3D(float3(rotCoordsB *
        float2(width / GetOption(A_GRAIN_SIZE), height / GetOption(A_GRAIN_SIZE)), 2.0)), GetOption(D_COLOR_AMOUNT));
    }

    //noisiness response curve based on scene luminance
    float luminance = mix(0.0, dot(color.rgb, lumCoeff), GetOption(E_LUMA_AMOUNT));
    float lum = smoothstep(0.2, 0.0, luminance);
    lum += luminance;

    noise = lerp(noise, float3(0.0), pow(lum, 4.0));
    color.rgb = color.rgb + noise * GetOption(B_GRAIN_AMOUNT);

    return color;
}

/*------------------------------------------------------------------------------
                           [SCANLINES CODE SECTION]
------------------------------------------------------------------------------*/

vec4 ScanlinesPass(vec4 color)
{
    vec4 intensity;

    if (GetOption(A_SCANLINE_TYPE) == 0) {
    if (fract(gl_FragCoord.y * 0.25) > GetOption(B_SCANLINE_THICKNESS))
    {
        intensity = vec4(0);
    } 
    else
    {
        intensity = smoothstep(0.2, GetOption(B_SCANLINE_BRIGHTNESS), color) +
        normalize(vec4(color.xyz, RGBLuminance(color.xyz)));
    } }

    else if (GetOption(A_SCANLINE_TYPE) == 1) {
    if (fract(gl_FragCoord.x * 0.25) > GetOption(B_SCANLINE_THICKNESS))
    {
        intensity = vec4(0);
    }
    else
    {
        intensity = smoothstep(0.2, GetOption(B_SCANLINE_BRIGHTNESS), color) +
        normalize(vec4(color.xyz, RGBLuminance(color.xyz)));
    } }

    else if (GetOption(A_SCANLINE_TYPE) == 2) {
    if (fract(gl_FragCoord.x * 0.25) > GetOption(B_SCANLINE_THICKNESS) && fract(gl_FragCoord.y * 0.25) > GetOption(B_SCANLINE_THICKNESS))
    {
        intensity = vec4(0);
    }
    else
    {
        intensity = smoothstep(0.2, GetOption(B_SCANLINE_BRIGHTNESS), color) +
        normalize(vec4(color.xyz, RGBLuminance(color.xyz)));
    } }

    float level = (4.0-texcoord.x) * GetOption(B_SCANLINE_INTENSITY);

    color = intensity * (0.5-level) + color * 1.1;

    return color;
}

/*------------------------------------------------------------------------------
                             [FXAA CODE SECTION]
------------------------------------------------------------------------------*/

#define FXAA_GLSL_130 1
#define FXAA_GREEN_AS_LUMA 1

#ifndef FXAA_GLSL_120
    #define FXAA_GLSL_120 0
#endif
#ifndef FXAA_GLSL_130
    #define FXAA_GLSL_130 0
#endif
#ifndef FXAA_GREEN_AS_LUMA
    #define FXAA_GREEN_AS_LUMA 0
#endif
#ifndef FXAA_EARLY_EXIT
    #define FXAA_EARLY_EXIT 1
#endif
#ifndef FXAA_DISCARD
    #define FXAA_DISCARD 0
#endif

#ifndef FXAA_FAST_PIXEL_OFFSET
#ifdef GL_EXT_gpu_shader4
    #define FXAA_FAST_PIXEL_OFFSET 1
#endif
#ifdef GL_NV_gpu_shader5
    #define FXAA_FAST_PIXEL_OFFSET 1
#endif
#ifdef GL_ARB_gpu_shader5
    #define FXAA_FAST_PIXEL_OFFSET 1
#endif
#ifndef FXAA_FAST_PIXEL_OFFSET
    #define FXAA_FAST_PIXEL_OFFSET 0
#endif
#endif

#ifndef FXAA_GATHER4_ALPHA
#if (FXAA_HLSL_5 == 1)
    #define FXAA_GATHER4_ALPHA 1
#endif
#ifdef GL_ARB_gpu_shader5
    #define FXAA_GATHER4_ALPHA 1
#endif
#ifdef GL_NV_gpu_shader5
    #define FXAA_GATHER4_ALPHA 1
#endif
#ifndef FXAA_GATHER4_ALPHA
    #define FXAA_GATHER4_ALPHA 0
#endif
#endif

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

#define FxaaBool bool
#define FxaaDiscard discard
#define FxaaFloat float
#define FxaaFloat2 vec2
#define FxaaFloat3 vec3
#define FxaaFloat4 vec4
#define FxaaHalf float
#define FxaaHalf2 vec2
#define FxaaHalf3 vec3
#define FxaaHalf4 vec4
#define FxaaInt2 ivec2
#define FxaaSat(x) clamp(x, 0.0, 1.0)
#define FxaaTex sampler2D

#if (FXAA_GLSL_120 == 1)
#define FxaaTexTop(t, p) texture2DLod(t, p, 0.0)
#if (FXAA_FAST_PIXEL_OFFSET == 1)
    #define FxaaTexOff(t, p, o, r) texture2DLodOffset(t, p, 0.0, o)
#else
    #define FxaaTexOff(t, p, o, r) texture2DLod(t, p + (o * r), 0.0)
#endif
#if (FXAA_GATHER4_ALPHA == 1)
    // use #extension GL_ARB_gpu_shader5 : enable
    #define FxaaTexAlpha4(t, p) textureGather(t, p, 3)
    #define FxaaTexOffAlpha4(t, p, o) textureGatherOffset(t, p, o, 3)
    #define FxaaTexGreen4(t, p) textureGather(t, p, 1)
    #define FxaaTexOffGreen4(t, p, o) textureGatherOffset(t, p, o, 1)
#endif
#endif

#if (FXAA_GLSL_130 == 1)
    #define FxaaTexTop(t, p) textureLod(t, p, 0.0)
    #define FxaaTexOff(t, p, o, r) textureLodOffset(t, p, 0.0, o)
#if (FXAA_GATHER4_ALPHA == 1)
    // use #extension GL_ARB_gpu_shader5 : enable
    #define FxaaTexAlpha4(t, p) textureGather(t, p, 3)
    #define FxaaTexOffAlpha4(t, p, o) textureGatherOffset(t, p, o, 3)
    #define FxaaTexGreen4(t, p) textureGather(t, p, 1)
    #define FxaaTexOffGreen4(t, p, o) textureGatherOffset(t, p, o, 1)
#endif
#endif

#if (FXAA_GREEN_AS_LUMA == 0)
    FxaaFloat FxaaLuma(FxaaFloat4 rgba) { return rgba.w; }
#else
    FxaaFloat FxaaLuma(FxaaFloat4 rgba) { return rgba.y; }
#endif    

FxaaFloat4 FxaaPixelShader(FxaaTex tex, FxaaFloat2 RcpFrame, FxaaFloat Subpix, FxaaFloat EdgeThreshold, FxaaFloat EdgeThresholdMin)
{
    FxaaFloat2 posM;
    posM.x = texcoord.x;
    posM.y = texcoord.y;
    #if (FXAA_GATHER4_ALPHA == 1)
    #if (FXAA_DISCARD == 0)
    FxaaFloat4 rgbyM = FxaaTexTop(tex, posM);
    #if (FXAA_GREEN_AS_LUMA == 0)
    #define lumaM rgbyM.w
    #else
    #define lumaM rgbyM.y
    #endif
    #endif
    #if (FXAA_GREEN_AS_LUMA == 0)
    FxaaFloat4 luma4A = FxaaTexAlpha4(tex, posM);
    FxaaFloat4 luma4B = FxaaTexOffAlpha4(tex, posM, FxaaInt2(-1, -1));
    #else
    FxaaFloat4 luma4A = FxaaTexGreen4(tex, posM);
    FxaaFloat4 luma4B = FxaaTexOffGreen4(tex, posM, FxaaInt2(-1, -1));
    #endif
    #if (FXAA_DISCARD == 1)
    #define lumaM luma4A.w
    #endif
    #define lumaE luma4A.z
    #define lumaS luma4A.x
    #define lumaSE luma4A.y
    #define lumaNW luma4B.w
    #define lumaN luma4B.z
    #define lumaW luma4B.x
    #else
    FxaaFloat4 rgbyM = FxaaTexTop(tex, posM);
    #if (FXAA_GREEN_AS_LUMA == 0)
    #define lumaM rgbyM.w
    #else
    #define lumaM rgbyM.y
    #endif
    FxaaFloat lumaS = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 0, 1), RcpFrame.xy));
    FxaaFloat lumaE = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 1, 0), RcpFrame.xy));
    FxaaFloat lumaN = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 0,-1), RcpFrame.xy));
    FxaaFloat lumaW = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2(-1, 0), RcpFrame.xy));
    #endif

    FxaaFloat maxSM = max(lumaS, lumaM);
    FxaaFloat minSM = min(lumaS, lumaM);
    FxaaFloat maxESM = max(lumaE, maxSM);
    FxaaFloat minESM = min(lumaE, minSM);
    FxaaFloat maxWN = max(lumaN, lumaW);
    FxaaFloat minWN = min(lumaN, lumaW);
    FxaaFloat rangeMax = max(maxWN, maxESM);
    FxaaFloat rangeMin = min(minWN, minESM);
    FxaaFloat rangeMaxScaled = rangeMax * EdgeThreshold;
    FxaaFloat range = rangeMax - rangeMin;
    FxaaFloat rangeMaxClamped = max(EdgeThresholdMin, rangeMaxScaled);
    FxaaBool earlyExit = range < rangeMaxClamped;

    if(earlyExit)
    #if (FXAA_DISCARD == 1)
    FxaaDiscard;
    #else
    return rgbyM;
    #endif

    #if (FXAA_GATHER4_ALPHA == 0)
    FxaaFloat lumaNW = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2(-1,-1), RcpFrame.xy));
    FxaaFloat lumaSE = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 1, 1), RcpFrame.xy));
    FxaaFloat lumaNE = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2( 1,-1), RcpFrame.xy));
    FxaaFloat lumaSW = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2(-1, 1), RcpFrame.xy));
    #else
    FxaaFloat lumaNE = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2(1, -1), RcpFrame.xy));
    FxaaFloat lumaSW = FxaaLuma(FxaaTexOff(tex, posM, FxaaInt2(-1, 1), RcpFrame.xy));
    #endif

    FxaaFloat lumaNS = lumaN + lumaS;
    FxaaFloat lumaWE = lumaW + lumaE;
    FxaaFloat subpixRcpRange = 1.0/range;
    FxaaFloat subpixNSWE = lumaNS + lumaWE;
    FxaaFloat edgeHorz1 = (-2.0 * lumaM) + lumaNS;
    FxaaFloat edgeVert1 = (-2.0 * lumaM) + lumaWE;

    FxaaFloat lumaNESE = lumaNE + lumaSE;
    FxaaFloat lumaNWNE = lumaNW + lumaNE;
    FxaaFloat edgeHorz2 = (-2.0 * lumaE) + lumaNESE;
    FxaaFloat edgeVert2 = (-2.0 * lumaN) + lumaNWNE;

    FxaaFloat lumaNWSW = lumaNW + lumaSW;
    FxaaFloat lumaSWSE = lumaSW + lumaSE;
    FxaaFloat edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);
    FxaaFloat edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);
    FxaaFloat edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;
    FxaaFloat edgeVert3 = (-2.0 * lumaS) + lumaSWSE;
    FxaaFloat edgeHorz = abs(edgeHorz3) + edgeHorz4;
    FxaaFloat edgeVert = abs(edgeVert3) + edgeVert4;

    FxaaFloat subpixNWSWNESE = lumaNWSW + lumaNESE;
    FxaaFloat lengthSign = RcpFrame.x;
    FxaaBool horzSpan = edgeHorz >= edgeVert;
    FxaaFloat subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;

    if(!horzSpan) lumaN = lumaW;
    if(!horzSpan) lumaS = lumaE;
    if(horzSpan) lengthSign = RcpFrame.y;
    FxaaFloat subpixB = (subpixA * (1.0/12.0)) - lumaM;

    FxaaFloat gradientN = lumaN - lumaM;
    FxaaFloat gradientS = lumaS - lumaM;
    FxaaFloat lumaNN = lumaN + lumaM;
    FxaaFloat lumaSS = lumaS + lumaM;
    FxaaBool pairN = abs(gradientN) >= abs(gradientS);
    FxaaFloat gradient = max(abs(gradientN), abs(gradientS));
    if(pairN) lengthSign = -lengthSign;
    FxaaFloat subpixC = FxaaSat(abs(subpixB) * subpixRcpRange);

    FxaaFloat2 posB;
    posB.x = posM.x;
    posB.y = posM.y;
    FxaaFloat2 offNP;
    offNP.x = (!horzSpan) ? 0.0 : RcpFrame.x;
    offNP.y = ( horzSpan) ? 0.0 : RcpFrame.y;
    if(!horzSpan) posB.x += lengthSign * 0.5;
    if( horzSpan) posB.y += lengthSign * 0.5;

    FxaaFloat2 posN;
    posN.x = posB.x - offNP.x * FXAA_QUALITY__P0;
    posN.y = posB.y - offNP.y * FXAA_QUALITY__P0;
    FxaaFloat2 posP;
    posP.x = posB.x + offNP.x * FXAA_QUALITY__P0;
    posP.y = posB.y + offNP.y * FXAA_QUALITY__P0;
    FxaaFloat subpixD = ((-2.0)*subpixC) + 3.0;
    FxaaFloat lumaEndN = FxaaLuma(FxaaTexTop(tex, posN));
    FxaaFloat subpixE = subpixC * subpixC;
    FxaaFloat lumaEndP = FxaaLuma(FxaaTexTop(tex, posP));

    if(!pairN) lumaNN = lumaSS;
    FxaaFloat gradientScaled = gradient * 1.0/4.0;
    FxaaFloat lumaMM = lumaM - lumaNN * 0.5;
    FxaaFloat subpixF = subpixD * subpixE;
    FxaaBool lumaMLTZero = lumaMM < 0.0;

    lumaEndN -= lumaNN * 0.5;
    lumaEndP -= lumaNN * 0.5;
    FxaaBool doneN = abs(lumaEndN) >= gradientScaled;
    FxaaBool doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P1;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P1;
    FxaaBool doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P1;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P1;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P2;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P2;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P2;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P2;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P3;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P3;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P3;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P3;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P4;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P4;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P4;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P4;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P5;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P5;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P5;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P5;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P6;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P6;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P6;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P6;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P7;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P7;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P7;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P7;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P8;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P8;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P8;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P8; }}}}}}}

    FxaaFloat dstN = posM.x - posN.x;
    FxaaFloat dstP = posP.x - posM.x;
    if(!horzSpan) dstN = posM.y - posN.y;
    if(!horzSpan) dstP = posP.y - posM.y;

    FxaaBool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
    FxaaFloat spanLength = (dstP + dstN);
    FxaaBool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
    FxaaFloat spanLengthRcp = 1.0/spanLength;

    FxaaBool directionN = dstN < dstP;
    FxaaFloat dst = min(dstN, dstP);
    FxaaBool goodSpan = directionN ? goodSpanN : goodSpanP;
    FxaaFloat subpixG = subpixF * subpixF;
    FxaaFloat pixelOffset = (dst * (-spanLengthRcp)) + 0.5;
    FxaaFloat subpixH = subpixG * Subpix;

    FxaaFloat pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
    FxaaFloat pixelOffsetSubpix = max(pixelOffsetGood, subpixH);
    if(!horzSpan) posM.x += pixelOffsetSubpix * lengthSign;
    if( horzSpan) posM.y += pixelOffsetSubpix * lengthSign;

    #if (FXAA_DISCARD == 1)
        return FxaaTexTop(tex, posM);
    #else
        if (GetOption(FXAA_ZSHOW_EDGES) == 1)
        { return -rgbyM; }
        else
        { return FxaaFloat4(FxaaTexTop(tex, posM).xyz, lumaM); }
    #endif
}

vec4 FxaaPass(vec4 color)
{
    color = FxaaPixelShader(samp9, pixelSize, GetOption(FXAA_SUBPIX_MAX), GetOption(FXAA_XEDGE_THRESHOLD), 0.000);
    
    return color;
}

/*------------------------------------------------------------------------------
                     [MAIN() & COMBINE PASS CODE SECTION]
------------------------------------------------------------------------------*/

void main()
{
    float4 color = Sample();

    if (!OptionEnabled(DISABLE_EFFECTS))
    {
        if (OptionEnabled(A_FXAA_PASS)) { color = FxaaPass(color); }
        if (OptionEnabled(A_BICUBLIC_SCALAR)) { color = BicubicScalarPass(color); }
        if (OptionEnabled(A_LANCZOS_SCALAR)) { color = LanczosScalarPass(color); }
        if (OptionEnabled(G_TEXTURE_SHARPEN)) { color = TexSharpenPass(color); }
        if (OptionEnabled(J_CEL_SHADING)) { color = CelPass(color); }
        if (OptionEnabled(K_SCAN_LINES)) { color = ScanlinesPass(color); }
        if (OptionEnabled(B_BLOOM_PASS)) { color = BloomPass(color); }
        if (OptionEnabled(C_TONEMAP_PASS)) { color = TonemapPass(color); }
        if (OptionEnabled(F_GAMMA_CORRECTION)) { color = GammaPass(color); }
        if (OptionEnabled(H_PIXEL_VIBRANCE)) { color = VibrancePass(color); }
        if (OptionEnabled(I_CONTRAST_ENHANCEMENT)) { color = ContrastPass(color); }
        if (OptionEnabled(L_FILM_GRAIN_PASS)) { color = GrainPass(color); }
    }

    SetOutput(color);
}
