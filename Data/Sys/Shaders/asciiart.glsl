/*
[configuration]

[OptionBool]
GUIName = Use target window resolution
OptionName = USE_WINDOW_RES
DefaultValue = true

[OptionBool]
GUIName = Debug: Calculate only one character per subgroup
OptionName = DEBUG_ONLY_ONE_CHAR
DefaultValue = false

[/configuration]
*/

const uint MAX_CHARS = 96u;                     // max 96, must be a multiple of 32
const bool HAVE_FULL_FEATURE_FALLBACK = false;  // terrible slow, can easily softlock the GPU
const uint UNROLL_FALLBACK = 4;
const uint UNROLL_SIMD = 3;  // max MAX_CHARS / 32

// #undef SUPPORTS_SUBGROUP_REDUCTION

#ifdef API_VULKAN
// By default, subgroupBroadcast only supports compile time constants as index.
// However we need an uniform instead. This is always supported in OpenGL,
// but in Vulkan only in SPIR-V >= 1.5.
// So fall back to subgroupShuffle on Vulkan instead.
#define subgroupBroadcast subgroupShuffle
#endif

/*
The header-only font
We have 96 (ASCII) characters, each of them is 12 pixels high and 8 pixels wide.
To store the boolean value per pixel, 96 bits per character is needed.
So three 32 bit integers are used per character.
This takes in total roughly 1 kB of constant buffer.
The first character must be all-one for the optimized implementation below.
*/
const uint char_width = 8;
const uint char_height = 12;
const uint char_count = 96;
const uint char_pixels = char_width * char_height;
const float2 char_dim = float2(char_width, char_height);

const uint rasters[char_count][(char_pixels + 31) / 32] = {
    {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}, {0x18181818, 0x00181818, 0x00181800},
    {0x6c6c6c6c, 0x00000000, 0x00000000}, {0x66660000, 0xff6666ff, 0x00006666},
    {0x1bff7e18, 0xd8f87e1f, 0x00187eff}, {0x6edb1b0e, 0x760c1830, 0x0070d8db},
    {0x3333361c, 0x1b0e0e1b, 0x00fe63f3}, {0x18383070, 0x00000000, 0x00000000},
    {0x0c0c1830, 0x0c0c0c0c, 0x0030180c}, {0x3030180c, 0x30303030, 0x000c1830},
    {0x5a990000, 0x5a3cff3c, 0x00000099}, {0x18180000, 0x18ffff18, 0x00001818},
    {0x00000000, 0x38000000, 0x000c1838}, {0x00000000, 0x00ffff00, 0x00000000},
    {0x00000000, 0x00000000, 0x00001c1c}, {0x6060c0c0, 0x18183030, 0x06060c0c},
    {0xe3c3663c, 0xc7cfdbf3, 0x003c66c3}, {0x181e1c18, 0x18181818, 0x007e1818},
    {0x60c0e77e, 0x060c1830, 0x00ff0303}, {0xc0c0e77e, 0xc0e07ee0, 0x007ee7c0},
    {0x363c3830, 0x3030ff33, 0x00303030}, {0x030303ff, 0xc0e07f03, 0x007ee7c0},
    {0x0303e77e, 0xc3e37f03, 0x007ee7c3}, {0xc0c0c0ff, 0x0c183060, 0x000c0c0c},
    {0xc3c3e77e, 0xc3e77ee7, 0x007ee7c3}, {0xc3c3e77e, 0xc0c0fee7, 0x007ee7c0},
    {0x00000000, 0x00001c1c, 0x00001c1c}, {0x38000000, 0x38000038, 0x000c1838},
    {0x0c183060, 0x0c060306, 0x00603018}, {0x00000000, 0xff00ffff, 0x000000ff},
    {0x30180c06, 0x3060c060, 0x00060c18}, {0xc0c3c37e, 0x18183060, 0x00180000},
    {0x7e000000, 0xdbcbbbc3, 0x00fc06f3}, {0xc3663c18, 0xc3ffc3c3, 0x00c3c3c3},
    {0xc3c3e37f, 0xc3e37fe3, 0x007fe3c3}, {0x0303e77e, 0x03030303, 0x007ee703},
    {0xc3e3733f, 0xc3c3c3c3, 0x003f73e3}, {0x030303ff, 0x03033f03, 0x00ff0303},
    {0x030303ff, 0x0303033f, 0x00030303}, {0x0303e77e, 0xc3f30303, 0x007ee7c3},
    {0xc3c3c3c3, 0xc3c3ffc3, 0x00c3c3c3}, {0x1818187e, 0x18181818, 0x007e1818},
    {0x60606060, 0x60606060, 0x003e7763}, {0x1b3363c3, 0x1b0f070f, 0x00c36333},
    {0x03030303, 0x03030303, 0x00ff0303}, {0xffffe7c3, 0xc3c3c3db, 0x00c3c3c3},
    {0xcfcfc7c7, 0xf3fbdbdf, 0x00e3e3f3}, {0xc3c3e77e, 0xc3c3c3c3, 0x007ee7c3},
    {0xc3c3e37f, 0x03037fe3, 0x00030303}, {0xc3c3663c, 0xdbc3c3c3, 0x00fc76fb},
    {0xc3c3e37f, 0x1b0f7fe3, 0x00c36333}, {0x0303e77e, 0xc0e07e07, 0x007ee7c0},
    {0x181818ff, 0x18181818, 0x00181818}, {0xc3c3c3c3, 0xc3c3c3c3, 0x007ee7c3},
    {0xc3c3c3c3, 0x6666c3c3, 0x00183c3c}, {0xc3c3c3c3, 0xffdbdbc3, 0x00c3e7ff},
    {0x3c6666c3, 0x3c3c183c, 0x00c36666}, {0x3c6666c3, 0x1818183c, 0x00181818},
    {0x60c0c0ff, 0x060c7e30, 0x00ff0303}, {0x0c0c0c3c, 0x0c0c0c0c, 0x003c0c0c},
    {0x0c0c0606, 0x30301818, 0xc0c06060}, {0x3030303c, 0x30303030, 0x003c3030},
    {0xc3663c18, 0x00000000, 0x00000000}, {0x00000000, 0x00000000, 0xff000000},
    {0x181c0c0e, 0x00000000, 0x00000000}, {0x00000000, 0xfec0c37e, 0x00fec3c3},
    {0x03030303, 0xc3c37f03, 0x007fc3c3}, {0x00000000, 0x0303c37e, 0x007ec303},
    {0xc0c0c0c0, 0xc3c3fec0, 0x00fec3c3}, {0x00000000, 0x7fc3c37e, 0x00fe0303},
    {0x0c0ccc78, 0x0c0c3f0c, 0x000c0c0c}, {0x00000000, 0xc3c3c37e, 0xc3c0c0fe},
    {0x03030303, 0xc3c3c37f, 0x00c3c3c3}, {0x00001800, 0x18181818, 0x00181818},
    {0x00003000, 0x30303030, 0x36303030}, {0x03030303, 0x0f1b3363, 0x0063331f},
    {0x1818181e, 0x18181818, 0x007e1818}, {0x00000000, 0xdbdbdb7f, 0x00dbdbdb},
    {0x00000000, 0x6363633f, 0x00636363}, {0x00000000, 0x6363633e, 0x003e6363},
    {0x00000000, 0xc3c3c37f, 0x03037fc3}, {0x00000000, 0xc3c3c3fe, 0xc0c0fec3},
    {0x00000000, 0x0303077f, 0x00030303}, {0x00000000, 0x7e0303fe, 0x007fc0c0},
    {0x0c0c0c00, 0x0c0c0c3f, 0x00386c0c}, {0x00000000, 0x63636363, 0x007e6363},
    {0x00000000, 0x6666c3c3, 0x00183c3c}, {0x00000000, 0xdbc3c3c3, 0x00c3e7ff},
    {0x00000000, 0x183c66c3, 0x00c3663c}, {0x00000000, 0x3c6666c3, 0x06060c18},
    {0x00000000, 0x183060ff, 0x00ff060c}, {0x181818f0, 0x181c0f1c, 0x00f01818},
    {0x18181818, 0x18181818, 0x18181818}, {0x1818180f, 0x1838f038, 0x000f1818},
    {0x06000000, 0x0060f18f, 0x00000000}, {0x00000000, 0x00000000, 0x00000000}};

// Precalculated sum of all pixels per character
const uint raster_active_pixels[char_count] = {
    96, 18, 16, 40, 56, 42, 46, 10, 22, 22, 32, 28, 10, 16, 6,  24, 52, 29, 36, 44, 35, 42, 50, 28,
    58, 51, 12, 16, 22, 32, 22, 26, 41, 46, 57, 38, 52, 38, 32, 46, 48, 30, 31, 43, 28, 56, 64, 52,
    42, 52, 52, 44, 28, 48, 42, 58, 42, 32, 38, 26, 24, 26, 14, 8,  10, 34, 40, 26, 40, 32, 30, 33,
    39, 16, 20, 37, 28, 43, 30, 30, 34, 34, 20, 28, 27, 30, 26, 36, 26, 24, 26, 30, 24, 30, 14, 0};

// Get one sample of the font: (pixel index, character index)
float SampleFont(uint2 pos)
{
  return (rasters[pos.y][pos.x / 32] >> (pos.x % 32)) & uint(1);
}

// Get one sample of the framebuffer: (character position in screen space, pixel index)
float3 SampleTex(uint2 char_pos, uint pixel)
{
  float2 inv_resoltion =
      OptionEnabled(USE_WINDOW_RES) ? GetInvWindowResolution() : GetInvResolution();
  float2 tex_pos = char_pos * char_dim + float2(pixel % char_width, pixel / char_width) + 0.5;
  return SampleLocation(tex_pos * inv_resoltion).xyz;
}

struct CharResults
{
  float3 fg;  // font color
  float3 bg;  // background color
  float err;  // MSE of this configuration
  uint c;     // character index
};

// Calculate the font and background color and the MSE for a given character
CharResults CalcCharRes(uint c, float3 t, float3 ft)
{
  CharResults o;
  o.c = c;

  // Inputs:
  // tt: sum of all texture samples squared
  // t: sum of all texture samples
  // ff: sum of all font samples squared
  // f: sum of all font samples
  // ft: sum of all font samples * texture samples

  // The font is either 1.0 or 0.0, so ff == f
  // As the font is constant, this is pre-calculated
  float f = raster_active_pixels[c];
  float ff = f;

  // The calculation isn't stable if the font is all-one. Return max err
  // instead.
  if (f == char_pixels)
  {
    o.err = char_pixels * char_pixels;
    return o;
  }

  // tt is only used as constant offset for the error, define it as zero
  float3 tt = float3(0.0, 0.0, 0.0);

  // The next lines are a bit harder, hf :-)

  // The idea is to find the perfect char with the perfect background color
  // and the perfect font color. As this is an equation with three unknowns,
  // we can't just try all chars and color combinations.

  // As criterion how "perfect" the selection is, we compare the "mean
  // squared error" of the resulted colors of all chars. So, now the big
  // issue: how to calculate the MSE without knowing the two colors ...

  // In the next steps, "a" is the font color, "b" is the background color,
  // "f" is the font value at this pixel, "t" is the texture value

  // So the square error of one pixel is:
  // e = ( t - a⋅f - b⋅(1-f) ) ^ 2

  // In longer:
  // e = a^2⋅f^2 - 2⋅a⋅b⋅f^2 + 2⋅a⋅b⋅f - 2⋅a⋅f⋅t + b^2⋅f^2 - 2⋅b^2⋅f + b^2 +
  // 2⋅b⋅f⋅t - 2⋅b⋅t + t^2

  // The sum of all errors is: (as shortcut, ff,f,ft,t,tt are now the sums
  // like declared above, sum(1) is the count of pixels) sum(e) = a^2⋅ff -
  // 2⋅a^2⋅ff + 2⋅a⋅b⋅f - 2⋅a⋅ft + b^2⋅ff - 2⋅b^2⋅f + b^2⋅sum(1) + 2⋅b⋅ft -
  // 2⋅b⋅t + tt

  // tt is only used as a constant offset, so its value has no effect on a,b or
  // on the relative error. So it can be completely dropped.

  // To find the minimum, we have to derive this by "a" and "b":
  // d/da sum(e) = 2⋅a⋅ff + 2⋅b⋅f - 2⋅b⋅ff - 2⋅ft
  // d/db sum(e) = 2⋅a⋅f - 2⋅a⋅ff - 4⋅b⋅f + 2⋅b⋅ff + 2⋅b⋅sum(1) + 2⋅ft - 2⋅t

  // So, both equations must be zero at minimum and there is only one
  // solution.

  float3 a = (ft * (f - float(char_pixels)) + t * (f - ff)) / (f * f - ff * float(char_pixels));
  float3 b = (ft * f - t * ff) / (f * f - ff * float(char_pixels));

  float3 e = a * a * ff + 2.0 * a * b * (f - ff) - 2.0 * a * ft +
             b * b * (-2.0 * f + ff + float(char_pixels)) + 2.0 * b * ft - 2.0 * b * t + tt;
  o.err = dot(e, float3(1.0, 1.0, 1.0));

  o.fg = a;
  o.bg = b;
  o.c = c;

  return o;
}

// Get the color of the pixel of this invocation based on the character details
float3 GetFinalPixel(CharResults char_out)
{
  float2 resolution = OptionEnabled(USE_WINDOW_RES) ? GetWindowResolution() : GetResolution();
  uint2 char_pos = uint2(floor(GetCoordinates() * resolution / char_dim));
  uint2 pixel_offset = uint2(floor(GetCoordinates() * resolution) - char_pos * char_dim);
  float font = SampleFont(int2(pixel_offset.x + char_width * pixel_offset.y, char_out.c));
  return char_out.fg * font + char_out.bg * (1.0 - font);
}

/*
  This shader performs some kind of brute force evaluation, which character fits best.

  for c in characters:
    for p in pixels:
      ft += font(c,p) * texture(p)
    res = CalcCharRes(ft)
  min(res.err)

  Terrible in performance, only for reference.
  */
CharResults CalcCharTrivial(uint2 char_pos)
{
  float3 t;
  CharResults char_out;
  char_out.err = char_pixels * char_pixels;
  for (uint c = 0; c < MAX_CHARS; c += 1)
  {
    float3 ft = float3(0.0, 0.0, 0.0);
    for (uint pixel = 0; pixel < char_pixels; pixel += 1)
    {
      float3 tex = SampleTex(char_pos, pixel);
      float font = SampleFont(uint2(pixel, c));
      ft += font * tex;
    }
    if (c == 0)
      t = ft;
    CharResults res = CalcCharRes(c, t, ft);
    if (res.err < char_out.err)
      char_out = res;
  }
  return char_out;
}

/*
  However for better performance, some characters are tested at once. This saves some expensive
  texture() calls. Also split the loop over the pixels in groups of 32 for only fetching the uint32
  of the font once.
*/
CharResults CalcCharFallback(uint2 char_pos)
{
  float3 t;
  CharResults char_out;
  char_out.err = char_pixels * char_pixels;
  for (uint c = 0; c < MAX_CHARS; c += UNROLL_FALLBACK)
  {
    // Declare ft
    float3 ft[UNROLL_FALLBACK];
    for (uint i = 0; i < UNROLL_FALLBACK; i++)
      ft[i] = float3(0.0, 0.0, 0.0);

    // Split `for p : pixels` in groups of 32. This makes accessing the texture (bit in uint32)
    // easier.
    for (uint pixel = 0; pixel < char_pixels; pixel += 32)
    {
      uint font_i[UNROLL_FALLBACK];
      for (uint i = 0; i < UNROLL_FALLBACK; i++)
        font_i[i] = rasters[c + i][pixel / 32];

      for (uint pixel_offset = 0; pixel_offset < 32; pixel_offset += 1)
      {
        float3 tex = SampleTex(char_pos, pixel + pixel_offset);

        // Inner kernel of `ft += font * tex`. Most time is spend in here.
        for (uint i = 0; i < UNROLL_FALLBACK; i++)
        {
          float font = (font_i[i] >> pixel_offset) & uint(1);

          ft[i] += font * tex;
        }
      }
    }
    if (c == 0)
    {
      // First char has font := 1, so t = ft. Cache this value for the next iterations.
      t = ft[0];
    }

    // Check if this character fits better than the last one.
    for (uint i = 0; i < UNROLL_FALLBACK; i++)
    {
      CharResults res = CalcCharRes(c + i, t, ft[i]);
      if (res.err < char_out.err)
        char_out = res;
    }
  }

  return char_out;
}

/*
  SIMD optimized version with subgroup intrinsics
  - distribute all characters over the lanes and check for them in parallel
  - distribute the uniform texture access and broadcast each back to each lane
*/
CharResults CalcCharSIMD(uint2 char_pos, uint simd_width)
{
  // Font color, bg color, character, error -- of character with minimum error
  CharResults char_out;
  char_out.err = char_pixels * char_pixels;
  float3 t;
#ifdef SUPPORTS_SUBGROUP_REDUCTION

  // Hack: Work in hard-codeded fixed SIMD mode
  if (gl_SubgroupInvocationID < simd_width)
  {
    // Loop over all characters
    for (uint c = 0; c < MAX_CHARS; c += UNROLL_SIMD * simd_width)
    {
      // registers for "sum of font * texture"
      float3 ft[UNROLL_SIMD];
      for (uint i = 0; i < UNROLL_SIMD; i++)
        ft[i] = float3(0.0, 0.0, 0.0);

      for (uint pixel = 0; pixel < char_pixels; pixel += 32)
      {
        // Preload the font uint32 for the next 32 pixels
        uint font_i[UNROLL_SIMD];
        for (uint i = 0; i < UNROLL_SIMD; i++)
          font_i[i] = rasters[c + UNROLL_SIMD * gl_SubgroupInvocationID + i][pixel / 32];

        for (uint pixel_offset = 0; pixel_offset < 32; pixel_offset += simd_width)
        {
          // Copy one full WRAP of textures into registers and shuffle them around for later usage.
          // This avoids one memory transaction per tested pixel & character.
          float3 tex_simd = SampleTex(char_pos, pixel + pixel_offset + gl_SubgroupInvocationID);

          for (uint k = 0; k < simd_width; k += 1)
          {
            float3 tex = subgroupBroadcast(tex_simd, k);

            // Note: As pixel iterates based on power-of-two gl_SubgroupSize,
            // the const memory access to rasters is CSE'd and the inner loop
            // after unrolling only contains: testing one bit + shuffle +
            // conditional add
            for (uint i = 0; i < UNROLL_SIMD; i++)
            {
              float font = (font_i[i] >> (k + pixel_offset % 32)) & uint(1);
              ft[i] += font * tex;
            }
          }
        }
      }
      if (c == 0)
      {
        // font[0] is a hardcoded 1 font, so t = ft
        t = subgroupBroadcast(ft[0], 0);
      }

      for (uint i = 0; i < UNROLL_SIMD; i++)
      {
        CharResults res = CalcCharRes(c + UNROLL_SIMD * gl_SubgroupInvocationID + i, t, ft[i]);
        if (res.err < char_out.err)
          char_out = res;
      }
    }
  }

  // Broadcast to get the best character of all threads
  float err_min = subgroupMin(char_out.err);
  uint smallest = subgroupBallotFindLSB(subgroupBallot(err_min == char_out.err));
  char_out.fg = subgroupBroadcast(char_out.fg, smallest);
  char_out.bg = subgroupBroadcast(char_out.bg, smallest);
  char_out.c = subgroupBroadcast(char_out.c, smallest);
  char_out.err = err_min;

#endif
  return char_out;
}

bool supportsSIMD(uint simd_width)
{
#ifdef SUPPORTS_SUBGROUP_REDUCTION
  const uint mask = simd_width == 32u ? 0xFFFFFFFFu : (1u << simd_width) - 1;
  return (subgroupBallot(true)[0] & mask) == mask;
#else
  return false;
#endif
}

// "Error: The AsciiArt shader requires the missing GPU extention KHR_shader_subgroup."
const uint missing_subgroup_warning_len = 82;
const uint missing_subgroup_warning[missing_subgroup_warning_len] = {
    37, 82, 82, 79, 82, 26, 95, 52, 72, 69, 95, 33, 83, 67, 73, 73, 33, 82, 84, 95, 83,
    72, 65, 68, 69, 82, 95, 82, 69, 81, 85, 73, 82, 69, 83, 95, 84, 72, 69, 95, 77, 73,
    83, 83, 73, 78, 71, 95, 39, 48, 53, 95, 69, 88, 84, 69, 78, 84, 73, 79, 78, 95, 43,
    40, 50, 63, 83, 72, 65, 68, 69, 82, 63, 83, 85, 66, 71, 82, 79, 85, 80, 14};

float3 ShowWarning(uint2 char_pos)
{
  CharResults char_out;
  char_out.fg = float3(1.0, 1.0, 1.0);
  char_out.bg = float3(0.0, 0.0, 0.0);
  char_out.c = 95u;  // just background

  if (char_pos.y == 0u && char_pos.x < missing_subgroup_warning_len)
  {
    char_out.c = missing_subgroup_warning[char_pos.x];
  }

  return GetFinalPixel(char_out);
}

void main()
{
  // Calculate the character position of this pixel
  float2 resolution = OptionEnabled(USE_WINDOW_RES) ? GetWindowResolution() : GetResolution();
  uint2 char_pos_self = uint2(floor(GetCoordinates() * resolution / char_dim));

  float3 color_out;

#ifdef SUPPORTS_SUBGROUP_REDUCTION
  if (supportsSIMD(8))
  {
    // Loop over all character positions covered by this wave
    bool pixel_active = !gl_HelperInvocation;
    CharResults char_out;
    while (true)
    {
      // Fetch the next active character position
      uint4 active_lanes = subgroupBallot(pixel_active);
      if (active_lanes == uint4(0, 0, 0, 0))
      {
        break;
      }
      uint2 char_pos = subgroupBroadcast(char_pos_self, subgroupBallotFindLSB(active_lanes));

      // And calculate everything for this character position
      if (supportsSIMD(32))
      {
        char_out = CalcCharSIMD(char_pos, 32);
      }
      else if (supportsSIMD(16))
      {
        char_out = CalcCharSIMD(char_pos, 16);
      }
      else if (supportsSIMD(8))
      {
        char_out = CalcCharSIMD(char_pos, 8);
      }

      // Draw the character on screen
      if (char_pos == char_pos_self)
      {
        color_out = GetFinalPixel(char_out);
        pixel_active = false;
      }
      if (OptionEnabled(DEBUG_ONLY_ONE_CHAR))
      {
        break;
      }
    }
  }
  else
#else
  if (char_pos_self.y <= 1u)
  {
    color_out = ShowWarning(char_pos_self);
  }
  else
#endif
      if (HAVE_FULL_FEATURE_FALLBACK)
  {
    color_out = GetFinalPixel(CalcCharFallback(char_pos_self));
  }
  else
  {
    color_out = Sample().xyz;
  }

  SetOutput(float4(color_out, 1.0));
}
