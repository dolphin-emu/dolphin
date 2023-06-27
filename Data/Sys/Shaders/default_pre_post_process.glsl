// Color Space references:
// https://www.unravel.com.au/understanding-color-spaces

// SMPTE 170M - BT.601 (NTSC-M) -> BT.709
mat3 from_NTSCM = transpose(mat3(
	0.939497225737661,		0.0502268452914346,		0.0102759289709032,
	0.0177558637510127,		0.965824605885027,		0.0164195303639603,
	-0.00162163209967010,	-0.00437400622653655,	1.00599563832621));

// ARIB TR-B9 (9300K+27MPCD with chromatic adaptation) (NTSC-J) -> BT.709
mat3 from_NTSCJ = transpose(mat3(
	0.823613036967492,		-0.0943227111084757,	0.00799341532931119,
	0.0289258355537324,		1.02310733489462,			0.00243547111576797,
	-0.00569501554980891,	0.0161828357559315,		1.22328453915712));

// EBU - BT.470BG/BT.601 (PAL) -> BT.709
mat3 from_PAL = transpose(mat3(
	1.04408168421813,		-0.0440816842181253,	0.000000000000000,
	0.000000000000000,	1.00000000000000,			0.000000000000000,
	0.000000000000000,	0.0118044782106489,		0.988195521789351));

float3 LinearTosRGBGamma(float3 color)
{
	const float a = 0.055;

	for (int i = 0; i < 3; ++i)
	{
		float x = color[i];
		if (x <= 0.0031308)
			x = x * 12.92;
		else
			x = (1.0 + a) * pow(x, 1.0 / 2.4) - a;
		color[i] = x;
	}

	return color;
}

// Non filtered gamma corrected sample (nearest neighbor)
float4 QuickSample(float3 uvw, float gamma)
{
	float4 color = texture(samp1, uvw);
	color.rgb = pow(color.rgb, float3(gamma));
	return color;
}

float4 QuickSample(float2 uv, float w, float gamma)
{
	return QuickSample(float3(uv, w), gamma);
}

float4 BilinearSample(float3 uvw, float gamma)
{
	// This emulates the (bi)linear filtering done directly from GPUs HW.
	// Note that GPUs might natively filter red green and blue differently, but we don't do it.
	// They might also use different filtering between upscaling and downscaling.
	
	float2 source_size = GetResolution();
	float2 inverted_source_size = GetInvResolution();
	float2 pixel = (uvw.xy * source_size) - 0.5; // Try to find the matching pixel top left corner

	// Find the integer and floating point parts
	float2 int_pixel = floor(pixel);
	float2 frac_pixel = fract(pixel);

	// Take 4 samples around the original uvw
	float4 c11 = QuickSample((int_pixel + float2(0.5, 0.5)) * inverted_source_size, uvw.z, gamma);
	float4 c21 = QuickSample((int_pixel + float2(1.5, 0.5)) * inverted_source_size, uvw.z, gamma);
	float4 c12 = QuickSample((int_pixel + float2(0.5, 1.5)) * inverted_source_size, uvw.z, gamma);
	float4 c22 = QuickSample((int_pixel + float2(1.5, 1.5)) * inverted_source_size, uvw.z, gamma);

	// Blend the 4 samples by their weight
	return lerp(lerp(c11, c21, frac_pixel.x), lerp(c12, c22, frac_pixel.x), frac_pixel.y);
}

// Based on https://github.com/libretro/slang-shaders/blob/master/interpolation/shaders/sharp-bilinear.slang
// by Themaister, Public Domain license
// Does a bilinear stretch, with a preapplied Nx nearest-neighbor scale,
// giving a sharper image than plain bilinear.
float4 SharpBilinearSample(float3 uvw, float gamma)
{
	float2 source_size = GetResolution();
	float2 inverted_source_size = GetInvResolution();
	float2 target_size = GetWindowResolution();
	float2 texel = uvw.xy * source_size;
	float2 texel_floored = floor(texel);
	float2 s = fract(texel);
	float scale = ceil(max(target_size.x * inverted_source_size.x, target_size.y * inverted_source_size.y));
	float region_range = 0.5 - (0.5 / scale);

	// Figure out where in the texel to sample to get correct pre-scaled bilinear.

	float2 center_dist = s - 0.5;
	float2 f = ((center_dist - clamp(center_dist, -region_range, region_range)) * scale) + 0.5;

	float2 mod_texel = texel_floored + f;

	uvw.xy = mod_texel * inverted_source_size;
	return BilinearSample(uvw, gamma);
}

float4 Cubic(float v)
{
	float4 n = float4(1.0, 2.0, 3.0, 4.0) - v;
	float4 s = n * n * n;
	float x = s.x;
	float y = s.y - 4.0 * s.x;
	float z = s.z - 4.0 * s.y + 6.0 * s.x;
	float w = 6.0 - x - y - z;
	return float4(x, y, z, w) * (1.0 / 6.0);
}

// https://stackoverflow.com/questions/13501081/efficient-bicubic-filtering-code-in-glsl
float4 BicubicSample(float3 uvw, float2 in_source_resolution, float2 in_inverted_source_resolution, float gamma)
{
	float2 pixel = (uvw.xy * in_source_resolution) - 0.5;
	float2 int_pixel = floor(pixel);
	float2 frac_pixel = fract(pixel);

	float4 xcubic = Cubic(frac_pixel.x);
	float4 ycubic = Cubic(frac_pixel.y);
	
	float4 c = float4(int_pixel.x - 0.5, int_pixel.x + 1.5, int_pixel.y - 0.5, int_pixel.y + 1.5);
	float4 s = float4(xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w);
	float4 offset = c + float4(xcubic.y, xcubic.w, ycubic.y, ycubic.w) / s;

	offset *= float4(in_inverted_source_resolution.x, in_inverted_source_resolution.x, in_inverted_source_resolution.y, in_inverted_source_resolution.y);

	float4 sample0 = QuickSample(offset.xz, uvw.z, gamma);
	float4 sample1 = QuickSample(offset.yz, uvw.z, gamma);
	float4 sample2 = QuickSample(offset.xw, uvw.z, gamma);
	float4 sample3 = QuickSample(offset.yw, uvw.z, gamma);

	float sx = s.x / (s.x + s.y);
	float sy = s.z / (s.z + s.w);

	return lerp(lerp(sample3, sample2, sx), lerp(sample1, sample0, sx), sy);
}

float4 CubicHermite(float4 A, float4 B, float4 C, float4 D, float t)
{
	float t2 = t * t;
	float t3 = t * t * t;
	float4 a = (-A / 2.0) + ((3.0 * B) / 2.0) - ((3.0 * C) / 2.0) + (D / 2.0);
	float4 b = A - ((5.0 * B) / 2.0 ) + (2.0 * C) - (D / 2.0);
	float4 c = (-A / 2.0) + (C / 2.0);
	float4 d = B;

	return (a * t3) + (b * t2) + (c * t) + d;
}

float4 BicubicHermiteSample(float3 uvw, float2 in_source_resolution, float2 in_inverted_source_resolution, float gamma)
{
	float2 pixel = (uvw.xy * in_source_resolution) + 0.5;
	float2 frac_pixel = fract(pixel);
	float2 uv = (floor(pixel) * in_inverted_source_resolution) - (in_inverted_source_resolution / 2.0);
	
	float2 inverted_source_resolution_double = in_inverted_source_resolution * 2.0;

	float4 c00 = QuickSample(uv + float2(-in_inverted_source_resolution.x,			-in_inverted_source_resolution.y), uvw.z, gamma);
	float4 c10 = QuickSample(uv + float2(	0.0,																	-in_inverted_source_resolution.y), uvw.z, gamma);
	float4 c20 = QuickSample(uv + float2(	in_inverted_source_resolution.x,			-in_inverted_source_resolution.y), uvw.z, gamma);
	float4 c30 = QuickSample(uv + float2(	inverted_source_resolution_double.x,	-in_inverted_source_resolution.y), uvw.z, gamma);
	
	float4 c01 = QuickSample(uv + float2(-in_inverted_source_resolution.x,			0.0), uvw.z, gamma);
	float4 c11 = QuickSample(uv + float2(	0.0,																	0.0), uvw.z, gamma);
	float4 c21 = QuickSample(uv + float2(	in_inverted_source_resolution.x,			0.0), uvw.z, gamma);
	float4 c31 = QuickSample(uv + float2(	inverted_source_resolution_double.x,	0.0), uvw.z, gamma);
	
	float4 c02 = QuickSample(uv + float2(-in_inverted_source_resolution.x,			in_inverted_source_resolution.y), uvw.z, gamma);
	float4 c12 = QuickSample(uv + float2(	0.0,																	in_inverted_source_resolution.y), uvw.z, gamma);
	float4 c22 = QuickSample(uv + float2(	in_inverted_source_resolution.x,			in_inverted_source_resolution.y), uvw.z, gamma);
	float4 c32 = QuickSample(uv + float2(	inverted_source_resolution_double.x,	in_inverted_source_resolution.y), uvw.z, gamma);
	
	float4 c03 = QuickSample(uv + float2(-in_inverted_source_resolution.x,			inverted_source_resolution_double.y), uvw.z, gamma);
	float4 c13 = QuickSample(uv + float2(	0.0,																	inverted_source_resolution_double.y), uvw.z, gamma);
	float4 c23 = QuickSample(uv + float2(	in_inverted_source_resolution.x,			inverted_source_resolution_double.y), uvw.z, gamma);
	float4 c33 = QuickSample(uv + float2(	inverted_source_resolution_double.x,	inverted_source_resolution_double.y), uvw.z, gamma);

	float4 cp0x = CubicHermite(c00, c10, c20, c30, frac_pixel.x);
	float4 cp1x = CubicHermite(c01, c11, c21, c31, frac_pixel.x);
	float4 cp2x = CubicHermite(c02, c12, c22, c32, frac_pixel.x);
	float4 cp3x = CubicHermite(c03, c13, c23, c33, frac_pixel.x);

	return CubicHermite(cp0x, cp1x, cp2x, cp3x, frac_pixel.y);
}

float CatmullRom(float B, float C, float x)
{
	float f = x;

	if (f < 0.0)
		f = -f;

	if (f < 1.0)
	{
		return ((12 - 9 * B - 6 * C) * (f * f * f) +
			(-18 + 12 * B + 6 * C) * (f * f) +
			(6 - 2 * B)) / 6.0;
	}
	else if (f >= 1.0 && f < 2.0)
	{
		return ((-B - 6 * C) * (f * f * f)
			+ (6 * B + 30 * C) * (f * f) +
			( - (12 * B) - 48 * C) * f +
			8 * B + 24 * C) / 6.0;
	}
	else
	{
		return 0.0;
	}
}

// https://www.codeproject.com/Articles/236394/Bi-Cubic-and-Bi-Linear-Interpolation-with-GLSL
// https://github.com/ValveSoftware/gamescope/pull/740
float4 BicubicCatmullRomSample(float3 uvw, float2 in_source_resolution, float2 in_inverted_source_resolution, float gamma)
{
	const float offset = 0.5;
	float2 pixel = (uvw.xy * in_source_resolution) - offset;
	float2 int_pixel = floor(pixel);
	float2 frac_pixel = fract(pixel);
	float2 int_uvw = (int_pixel + offset) * in_inverted_source_resolution;

	// B and C can be any value between 0 and 1,
	// though they are meant to be 0 and 0.5 for Catmull-Rom.
	// https://en.wikipedia.org/wiki/Mitchell%E2%80%93Netravali_filters
	// https://guideencodemoe-mkdocs.readthedocs.io/encoding/resampling/
	const float B = 0.0;
	const float C = 0.5;

	// Take 16 (4x4) samples, each with a different weight.
	// This loop can be replaced with any other bicubic formula (e.g. Hermite).
	float4 color_sum = float4(0.0, 0.0, 0.0, 0.0);
	float4 color_denominator = float4(0.0, 0.0, 0.0, 0.0);
	for (int m = -1; m <= 2; m++)
	{
		for (int n = -1; n <= 2; n++)
		{
			float4 color = QuickSample(int_uvw + (float2(m, n) * in_inverted_source_resolution), uvw.z, gamma);
			float f1 = CatmullRom(B, C, float(m) - frac_pixel.x);
			float f2 = CatmullRom(B, C, -float(n) + frac_pixel.y);
			float4 cooef1 = float4(f1, f1, f1, f1);
			float4 cooef2 = float4(f2, f2, f2, f2);
			color_sum += color * (cooef2 * cooef1);
			color_denominator += cooef2 * cooef1;
		}
	}
	return color_sum / color_denominator;
}

// Returns an accurate (gamma corrected) sample of a gamma space space texture.
// Outputs in linear space for simplicity.
float4 LinearGammaCorrectedSample(float gamma)
{
	float3 uvw = v_tex0;
	float4 color = float4(0, 0, 0, 1);
	
	if (resampling_method <= 1) // Bilinear
	{
		color = BilinearSample(uvw, gamma);
	}
	else if (resampling_method == 2) // "Simple" Bicubic
	{
		color = BicubicSample(uvw, GetResolution(), GetInvResolution(), gamma);
	}
	else if (resampling_method == 3) // Hermite
	{
		color = BicubicHermiteSample(uvw, GetResolution(), GetInvResolution(), gamma);
	}
	else if (resampling_method == 4) // Catmull-Rom
	{
		color = BicubicCatmullRomSample(uvw, GetResolution(), GetInvResolution(), gamma);
	}
	else if (resampling_method == 5) // Nearest Neighbor
	{
		color = QuickSample(uvw, gamma);
	}
	else if (resampling_method == 6) // Sharp Bilinear
	{
		color = SharpBilinearSample(uvw, gamma);
	}

	return color;
}

void main()
{
	// This tries to fall back on GPU HW sampling if it can (it won't be gamma corrected).
	bool raw_resampling = resampling_method <= 0;
	bool needs_rescaling = GetResolution() != GetWindowResolution();

	bool needs_resampling = needs_rescaling && (OptionEnabled(hdr_output) || OptionEnabled(correct_gamma) || !raw_resampling);

	float4 color;

	if (needs_resampling)
	{
		// Doing linear sampling in "gamma space" on linear texture formats isn't correct.
		// If the source and target resolutions don't match, the GPU will return a color
		// that is the average of 4 gamma space colors, but gamma space colors can't be blended together,
		// gamma neeeds to be de-applied first. This makes a big difference if colors change
		// drastically between two pixels.

		color = LinearGammaCorrectedSample(game_gamma);
	}
	else
	{
		// Default GPU HW sampling. Bilinear is identical to Nearest Neighbor if the input and output resolutions match.
		if (needs_rescaling)
			color = texture(samp0, v_tex0);
		else
			color = texture(samp1, v_tex0);
		
		// Convert to linear before doing any other of follow up operations.
		color.rgb = pow(color.rgb, float3(game_gamma));
	}

	if (OptionEnabled(correct_color_space))
	{
		if (game_color_space == 0)
			color.rgb = color.rgb * from_NTSCM;
		else if (game_color_space == 1)
			color.rgb = color.rgb * from_NTSCJ;
		else if (game_color_space == 2)
			color.rgb = color.rgb * from_PAL;
	}
	
	if (OptionEnabled(hdr_output))
	{
		float hdr_paper_white = hdr_paper_white_nits / hdr_sdr_white_nits;
		color.rgb *= hdr_paper_white;
	}
	
	if (OptionEnabled(linear_space_output))
	{
		// Nothing to do here
	}
	// Correct the SDR gamma for sRGB (PC/Monitor) or ~2.2 (Common TV gamma)
	else if (OptionEnabled(correct_gamma))
	{
		if (OptionEnabled(sdr_display_gamma_sRGB))
			color.rgb = LinearTosRGBGamma(color.rgb);
		else
			color.rgb = pow(color.rgb, float3(1.0 / sdr_display_custom_gamma));
	}
	// Restore the original gamma without changes
	else
	{
			color.rgb = pow(color.rgb, float3(1.0 / game_gamma));
	}

	SetOutput(color);
}