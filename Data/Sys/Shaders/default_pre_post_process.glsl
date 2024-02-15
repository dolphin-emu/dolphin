/***** COLOR CORRECTION *****/

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

/***** COLOR SAMPLING *****/

// Non filtered gamma corrected sample (nearest neighbor)
float4 QuickSample(float3 uvw, float gamma)
{
#if 0 // Test sampling range
	const float threshold = 0.00000001;
	float2 xy = uvw.xy * GetResolution();
	// Sampling outside the valid range, draw in yellow
	if (xy.x < (0.0 - threshold) || xy.x > (GetResolution().x + threshold) || xy.y < (0.0 - threshold) || xy.y > (GetResolution().y + threshold))
		return float4(1.0, 1.0, 0.0, 1);
	// Sampling at the edges, draw in purple
	if (xy.x < 1.0 || xy.x > (GetResolution().x - 1.0) || xy.y < 1.0 || xy.y > (GetResolution().y - 1.0))
		return float4(0.5, 0, 0.5, 1);
#endif

	float4 color = texture(samp1, uvw);
	color.rgb = pow(color.rgb, float3(gamma));
	return color;
}
float4 QuickSample(float2 uv, float w, float gamma)
{
	return QuickSample(float3(uv, w), gamma);
}
float4 QuickSampleByPixel(float2 xy, float w, float gamma)
{
	float3 uvw = float3(xy * GetInvResolution(), w);
	return QuickSample(uvw, gamma);
}

/***** Bilinear Interpolation *****/

float4 BilinearSample(float3 uvw, float gamma)
{
	// This emulates the (bi)linear filtering done directly from GPUs HW.
	// Note that GPUs might natively filter red green and blue differently, but we don't do it.
	// They might also use different filtering between upscaling and downscaling.
	float2 source_size = GetResolution();
	float2 pixel = (uvw.xy * source_size) - 0.5; // Try to find the matching pixel top left corner

	// Find the integer and floating point parts
	float2 int_pixel = floor(pixel);
	float2 frac_pixel = fract(pixel);

	// Take 4 samples around the original uvw
	float4 c11 = QuickSampleByPixel(int_pixel + float2(0.5, 0.5), uvw.z, gamma);
	float4 c21 = QuickSampleByPixel(int_pixel + float2(1.5, 0.5), uvw.z, gamma);
	float4 c12 = QuickSampleByPixel(int_pixel + float2(0.5, 1.5), uvw.z, gamma);
	float4 c22 = QuickSampleByPixel(int_pixel + float2(1.5, 1.5), uvw.z, gamma);

	// Blend the 4 samples by their weight
	return lerp(lerp(c11, c21, frac_pixel.x), lerp(c12, c22, frac_pixel.x), frac_pixel.y);
}

/***** Bicubic Interpolation *****/

// Formula derived from:
// https://en.wikipedia.org/wiki/Mitchell%E2%80%93Netravali_filters#Definition
// Values from:
// https://guideencodemoe-mkdocs.readthedocs.io/encoding/resampling/#mitchell-netravali-bicubic
// Other references:
// https://www.codeproject.com/Articles/236394/Bi-Cubic-and-Bi-Linear-Interpolation-with-GLSL
// https://github.com/ValveSoftware/gamescope/pull/740
// https://stackoverflow.com/questions/13501081/efficient-bicubic-filtering-code-in-glsl
#define CUBIC_COEFF_GEN(B, C)																						\
	(mat4(/* t^0 */ ((B) / 6.0), (-(B) / 3.0 + 1.0), ((B) / 6.0), (0.0),	\
		/* t^1 */ (-(B) / 2.0 - (C)), (0.0), ((B) / 2.0 + (C)), (0.0),			\
		/* t^2 */ ((B) / 2.0 + 2.0 * (C)), (2.0 * (B) + (C)-3.0),						\
		(-5.0 * (B) / 2.0 - 2.0 * (C) + 3.0), (-(C)),												\
		/* t^3 */ (-(B) / 6.0 - (C)), (-3.0 * (B) / 2.0 - (C) + 2.0),				\
		(3.0 * (B) / 2.0 + (C)-2.0), ((B) / 6.0 + (C))))

float4 CubicCoeffs(float t, mat4 coeffs)
{
	return coeffs * float4(1.0, t, t * t, t * t * t);
}

float4 CubicMix(float4 c0, float4 c1, float4 c2, float4 c3, float4 coeffs)
{
	return c0 * coeffs[0] + c1 * coeffs[1] + c2 * coeffs[2] + c3 * coeffs[3];
}

// By Sam Belliveau. Public Domain license.
// Simple 16 tap, gamma correct, implementation of bicubic filtering.
float4 BicubicSample(float3 uvw, float gamma, mat4 coeffs)
{
	float2 pixel = (uvw.xy * GetResolution()) - 0.5;
	float2 int_pixel = floor(pixel);
	float2 frac_pixel = fract(pixel);

	float4 c00 = QuickSampleByPixel(int_pixel + float2(-0.5, -0.5), uvw.z, gamma);
	float4 c10 = QuickSampleByPixel(int_pixel + float2(+0.5, -0.5), uvw.z, gamma);
	float4 c20 = QuickSampleByPixel(int_pixel + float2(+1.5, -0.5), uvw.z, gamma);
	float4 c30 = QuickSampleByPixel(int_pixel + float2(+2.5, -0.5), uvw.z, gamma);

	float4 c01 = QuickSampleByPixel(int_pixel + float2(-0.5, +0.5), uvw.z, gamma);
	float4 c11 = QuickSampleByPixel(int_pixel + float2(+0.5, +0.5), uvw.z, gamma);
	float4 c21 = QuickSampleByPixel(int_pixel + float2(+1.5, +0.5), uvw.z, gamma);
	float4 c31 = QuickSampleByPixel(int_pixel + float2(+2.5, +0.5), uvw.z, gamma);

	float4 c02 = QuickSampleByPixel(int_pixel + float2(-0.5, +1.5), uvw.z, gamma);
	float4 c12 = QuickSampleByPixel(int_pixel + float2(+0.5, +1.5), uvw.z, gamma);
	float4 c22 = QuickSampleByPixel(int_pixel + float2(+1.5, +1.5), uvw.z, gamma);
	float4 c32 = QuickSampleByPixel(int_pixel + float2(+2.5, +1.5), uvw.z, gamma);

	float4 c03 = QuickSampleByPixel(int_pixel + float2(-0.5, +2.5), uvw.z, gamma);
	float4 c13 = QuickSampleByPixel(int_pixel + float2(+0.5, +2.5), uvw.z, gamma);
	float4 c23 = QuickSampleByPixel(int_pixel + float2(+1.5, +2.5), uvw.z, gamma);
	float4 c33 = QuickSampleByPixel(int_pixel + float2(+2.5, +2.5), uvw.z, gamma);

	float4 cx = CubicCoeffs(frac_pixel.x, coeffs);
	float4 cy = CubicCoeffs(frac_pixel.y, coeffs);

	float4 x0 = CubicMix(c00, c10, c20, c30, cx);
	float4 x1 = CubicMix(c01, c11, c21, c31, cx);
	float4 x2 = CubicMix(c02, c12, c22, c32, cx);
	float4 x3 = CubicMix(c03, c13, c23, c33, cx);

	return CubicMix(x0, x1, x2, x3, cy);
}

/***** Sharp Bilinear Filtering *****/

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
	float scale = max(floor(max(target_size.x * inverted_source_size.x, target_size.y * inverted_source_size.y)), 1.f);
	float region_range = 0.5 - (0.5 / scale);

	// Figure out where in the texel to sample to get correct pre-scaled bilinear.

	float2 center_dist = s - 0.5;
	float2 f = ((center_dist - clamp(center_dist, -region_range, region_range)) * scale) + 0.5;

	float2 mod_texel = texel_floored + f;

	uvw.xy = mod_texel * inverted_source_size;
	return BilinearSample(uvw, gamma);
}

/***** Area Sampling *****/

// By Sam Belliveau and Filippo Tarpini. Public Domain license.
// Effectively a more accurate sharp bilinear filter when upscaling,
// that also works as a mathematically perfect downscale filter.
// https://entropymine.com/imageworsener/pixelmixing/
// https://github.com/obsproject/obs-studio/pull/1715
// https://legacy.imagemagick.org/Usage/filter/
float4 AreaSampling(float3 uvw, float gamma)
{
	// Determine the sizes of the source and target images.
	float2 source_size = GetResolution();
	float2 target_size = GetWindowResolution();
	float2 inverted_target_size = GetInvWindowResolution();

	// Compute the top-left and bottom-right corners of the target pixel box.
	float2 t_beg = floor(uvw.xy * target_size);
	float2 t_end = t_beg + float2(1.0, 1.0);

	// Convert the target pixel box to source pixel box.
	float2 beg = t_beg * inverted_target_size * source_size;
	float2 end = t_end * inverted_target_size * source_size;

	// Compute the top-left and bottom-right corners of the pixel box.
	float2 f_beg = floor(beg);
	float2 f_end = floor(end);

	// Compute how much of the start and end pixels are covered horizontally & vertically.
	float area_w = 1.0 - fract(beg.x);
	float area_n = 1.0 - fract(beg.y);
	float area_e = fract(end.x);
	float area_s = fract(end.y);

	// Compute the areas of the corner pixels in the pixel box.
	float area_nw = area_n * area_w;
	float area_ne = area_n * area_e;
	float area_sw = area_s * area_w;
	float area_se = area_s * area_e;

	// Initialize the color accumulator.
	float4 avg_color = float4(0.0, 0.0, 0.0, 0.0);

	// Prevents rounding errors due to the coordinates flooring above
	const float2 offset = float2(0.5, 0.5);

	// Accumulate corner pixels.
	avg_color += area_nw * QuickSampleByPixel(float2(f_beg.x, f_beg.y) + offset, uvw.z, gamma);
	avg_color += area_ne * QuickSampleByPixel(float2(f_end.x, f_beg.y) + offset, uvw.z, gamma);
	avg_color += area_sw * QuickSampleByPixel(float2(f_beg.x, f_end.y) + offset, uvw.z, gamma);
	avg_color += area_se * QuickSampleByPixel(float2(f_end.x, f_end.y) + offset, uvw.z, gamma);

	// Determine the size of the pixel box.
	int x_range = int(f_end.x - f_beg.x - 0.5);
	int y_range = int(f_end.y - f_beg.y - 0.5);

	// Workaround to compile the shader with DX11/12.
	// If this isn't done, it will complain that the loop could have too many iterations.
	// This number should be enough to guarantee downscaling from very high to very small resolutions.
	// Note that this number might be referenced in the UI.
	const int max_iterations = 16;

	// Fix up the average calculations in case we reached the upper limit
	x_range = min(x_range, max_iterations);
	y_range = min(y_range, max_iterations);

	// Accumulate top and bottom edge pixels.
	for (int ix = 0; ix < max_iterations; ++ix)
	{
		if (ix < x_range)
		{
			float x = f_beg.x + 1.0 + float(ix);
			avg_color += area_n * QuickSampleByPixel(float2(x, f_beg.y) + offset, uvw.z, gamma);
			avg_color += area_s * QuickSampleByPixel(float2(x, f_end.y) + offset, uvw.z, gamma);
		}
	}

	// Accumulate left and right edge pixels and all the pixels in between.
	for (int iy = 0; iy < max_iterations; ++iy)
	{
		if (iy < y_range)
		{
			float y = f_beg.y + 1.0 + float(iy);

			avg_color += area_w * QuickSampleByPixel(float2(f_beg.x, y) + offset, uvw.z, gamma);
			avg_color += area_e * QuickSampleByPixel(float2(f_end.x, y) + offset, uvw.z, gamma);

			for (int ix = 0; ix < max_iterations; ++ix)
			{
				if (ix < x_range)
				{
					float x = f_beg.x + 1.0 + float(ix);
					avg_color += QuickSampleByPixel(float2(x, y) + offset, uvw.z, gamma);
				}
			}
		}
	}

	// Compute the area of the pixel box that was sampled.
	float area_corners = area_nw + area_ne + area_sw + area_se;
	float area_edges = float(x_range) * (area_n + area_s) + float(y_range) * (area_w + area_e);
	float area_center = float(x_range) * float(y_range);

	// Return the normalized average color.
	return avg_color / (area_corners + area_edges + area_center);
}

/***** Main Functions *****/

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
	else if (resampling_method == 2) // Bicubic: B-Spline
	{
		color = BicubicSample(uvw, gamma, CUBIC_COEFF_GEN(1.0, 0.0));
	}
	else if (resampling_method == 3) // Bicubic: Mitchell-Netravali
	{
		color = BicubicSample(uvw, gamma, CUBIC_COEFF_GEN(1.0 / 3.0, 1.0 / 3.0));
	}
	else if (resampling_method == 4) // Bicubic: Catmull-Rom
	{
		color = BicubicSample(uvw, gamma, CUBIC_COEFF_GEN(0.0, 0.5));
	}
	else if (resampling_method == 5) // Sharp Bilinear
	{
		color = SharpBilinearSample(uvw, gamma);
	}
	else if (resampling_method == 6) // Area Sampling
	{
		color = AreaSampling(uvw, gamma);
	}
	else if (resampling_method == 7) // Nearest Neighbor
	{
		color = QuickSample(uvw, gamma);
	}
	else if (resampling_method == 8) // Bicubic: Hermite
	{
		color = BicubicSample(uvw, gamma, CUBIC_COEFF_GEN(0.0, 0.0));
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
