// Based on https://github.com/libretro/slang-shaders/blob/master/interpolation/shaders/sharp-bilinear.slang
// by Themaister, Public Domain license
// Does a bilinear stretch, with a preapplied Nx nearest-neighbor scale,
// giving a sharper image than plain bilinear.

/*
[configuration]
[OptionRangeFloat]
GUIName = Prescale Factor (set to 0 for automatic)
OptionName = PRESCALE_FACTOR
MinValue = 0.0
MaxValue = 16.0
StepAmount = 1.0
DefaultValue = 0.0
[/configuration]
*/

float CalculatePrescale(float config_scale) {
  if (config_scale == 0.0) {
    float2 source_size = GetResolution();
    float2 window_size = GetWindowResolution();
    return ceil(max(window_size.x / source_size.x, window_size.y / source_size.y));
  } else {
    return config_scale;
  }
}

void main()
{
  float2 source_size = GetResolution();
  float2 texel = GetCoordinates() * source_size;
  float2 texel_floored = floor(texel);
  float2 s = fract(texel);
  float config_scale = GetOption(PRESCALE_FACTOR);
  float scale = CalculatePrescale(config_scale);
  float region_range = 0.5 - 0.5 / scale;

  // Figure out where in the texel to sample to get correct pre-scaled bilinear.
  // Uses the hardware bilinear interpolator to avoid having to sample 4 times manually.

  float2 center_dist = s - 0.5;
  float2 f = (center_dist - clamp(center_dist, -region_range, region_range)) * scale + 0.5;

  float2 mod_texel = texel_floored + f;

  SetOutput(SampleLocation(mod_texel / source_size));
}
