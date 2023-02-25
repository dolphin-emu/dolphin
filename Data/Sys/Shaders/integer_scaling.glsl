// Set aspect ratio to 'stretch'

// Integer Scaling shader by One_More_Try / TryTwo
// Uses Sharp Bilinear from
// https://github.com/libretro/slang-shaders/blob/master/interpolation/shaders/sharp-bilinear.slang
// by Themaister, Public Domain license

/*
[configuration]
[OptionBool]
GUIName = Please set aspect ratio to stretch
OptionName = ASPECT_MSG
DefaultValue = false

[OptionBool]
GUIName = Use non-integer width
OptionName = WIDTH_UNLOCK
DefaultValue = false

[OptionBool]
GUIName = Stretch width to window
OptionName = WIDTH_SKIP
DependentOption = WIDTH_UNLOCK
DefaultValue = false

[OptionBool]
GUIName = Scale width to fit 4:3
OptionName = WIDTH_43
DependentOption = WIDTH_UNLOCK
DefaultValue = false

[OptionBool]
GUIName = Scale width to fit 16:9
OptionName = WIDTH_169
DependentOption = WIDTH_UNLOCK
DefaultValue = false

[OptionBool]
GUIName = Apply sharp bilinear for custom widths
OptionName = SHARP_BILINEAR
DefaultValue = false

[OptionRangeFloat]
GUIName = Sharp bilinear factor (0 = auto)
OptionName = SHARP_PRESCALE
MinValue = 0.0
MaxValue = 16.0
StepAmount = 1.0
DefaultValue = 0.0

[OptionBool]
GUIName = Manual scale - Set IR first
OptionName = MANUALSCALE
DefaultValue = false

[OptionRangeFloat]
GUIName = Integer scale - No higher than IR
OptionName = INTEGER_SCALE
DependentOption = MANUALSCALE
MaxValue = 5.0
MinValue = 1.0
DefaultValue = 1.0
StepAmount = 1.0

[OptionRangeFloat]
GUIName = Scale width
OptionName = WIDTH_SCALE
DependentOption = MANUALSCALE
MaxValue = 5.0
MinValue = -2.0
DefaultValue = 0.0
StepAmount = 1.0

[OptionBool]
GUIName = Auto downscaling
OptionName = DOWNSCALE
DefaultValue = false

[/configuration]
*/

void main()
{
	float4 c0 = float4(0.0, 0.0, 0.0, 0.0);
	float2 scale = float2(1.0, 1.0);
	float2 xfb_res = GetResolution();
	float2 win_res = GetWindowResolution();
	float2 coords = GetCoordinates();

	// ratio is used to rescale the coords to the xfb size, which allows for integer scaling.
	// ratio can then be multiplied by an integer to upscale/downscale, but upscale isn't supported by
	// point-sampling.
	float2 ratio = win_res / xfb_res;

	if (OptionEnabled(WIDTH_UNLOCK))
	{
		if (OptionEnabled(WIDTH_SKIP))
			ratio.x = 1.0;
		else if (OptionEnabled(WIDTH_43))
			ratio.x = win_res.x / (xfb_res.y * 4 / 3);
		else if (OptionEnabled(WIDTH_169))
			ratio.x = win_res.x / (xfb_res.y * 16 / 9);
	}

	if (OptionEnabled(MANUALSCALE))
	{
		// There's no IR variable, so this guesses the IR, but may be off for some games.
		float calc_ir = ceil(xfb_res.y / 500);
		scale.y = calc_ir / GetOption(INTEGER_SCALE);
		float manual_width = GetOption(WIDTH_SCALE);

		if (manual_width < 0.0)
			scale.x = scale.y * (abs(manual_width) + 1);
		else
			scale.x = scale.y / (manual_width + 1);

		ratio = ratio * scale;
	}
	else if (OptionEnabled(DOWNSCALE) && (ratio.x < 1 || ratio.y < 1))
	{
		scale.x = ceil(max(1.0 / ratio.y, 1.0 / ratio.x));
		scale.y = scale.x;
		ratio = ratio * scale;
	}

	// y and x are used to determine black bars vs drawable space.
	float y = win_res.y - xfb_res.y / scale.y;
	float y_top = (y / 2.0) * GetInvWindowResolution().y;
	float y_bottom = (win_res.y - y / 2.0) * GetInvWindowResolution().y;
	float yloc = (coords.y - y_top) * ratio.y;

	float x = win_res.x - xfb_res.x / scale.x;

	if (OptionEnabled(WIDTH_UNLOCK))
	{
		if (OptionEnabled(WIDTH_SKIP))
			x = 0.0;
		else if (OptionEnabled(WIDTH_43))
			x = win_res.x - xfb_res.y / scale.y * 4 / 3;
		else if (OptionEnabled(WIDTH_169))
			x = win_res.x - xfb_res.y / scale.y * 16 / 9;
	}

	float x_left = (x / 2.0) * GetInvWindowResolution().x;
	float x_right = (win_res.x - x / 2.0) * GetInvWindowResolution().x;
	float xloc = (coords.x - x_left) * ratio.x;

	if (OptionEnabled(SHARP_BILINEAR) &&
		(OptionEnabled(WIDTH_SKIP) || OptionEnabled(WIDTH_43) || OptionEnabled(WIDTH_169) ||
			(OptionEnabled(MANUALSCALE) && GetOption(WIDTH_SCALE) != 0.0)))
	{
		float texel = xloc * xfb_res.x;
		float texel_floored = floor(texel);
		float s = frac(texel);
		float scale_sharp = GetOption(SHARP_PRESCALE);

		if (scale_sharp == 0)
		{
			if (OptionEnabled(WIDTH_43))
				scale_sharp = (4 / 3 * xfb_res.y / xfb_res.x);
			else if (OptionEnabled(WIDTH_169))
				scale_sharp = (16 / 9 * xfb_res.y / xfb_res.x);
			else
				scale_sharp = ceil(win_res.x / xfb_res.x);
		}

		float region_range = 0.5 - 0.5 / scale_sharp;
		float center_dist = s - 0.5;
		float f = (center_dist - clamp(center_dist, -region_range, region_range)) * scale_sharp + 0.5;

		float mod_texel = texel_floored + f;
		xloc = mod_texel / xfb_res.x;
	}

	if (coords.x >= x_left && x_right >= coords.x && coords.y >= y_top && y_bottom >= coords.y)
	{
		float2 sample_loc = float2(xloc, yloc);
		c0 = SampleLocation(sample_loc);
	}

	SetOutput(c0);
}
