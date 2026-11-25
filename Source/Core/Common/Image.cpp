// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Image.h"

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string>

#include <spng.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/Timer.h"

namespace Common
{
static void spng_free(spng_ctx* ctx)
{
  if (ctx)
    spng_ctx_free(ctx);
}

static auto make_spng_ctx(int flags)
{
  return std::unique_ptr<spng_ctx, decltype(&spng_free)>(spng_ctx_new(flags), spng_free);
}

bool LoadPNG(std::span<const u8> input, Common::UniqueBuffer<u8>* data_out, u32* width_out,
             u32* height_out)
{
  auto ctx = make_spng_ctx(0);
  if (!ctx)
    return false;

  if (spng_set_png_buffer(ctx.get(), input.data(), input.size()))
    return false;

  spng_ihdr ihdr{};
  if (spng_get_ihdr(ctx.get(), &ihdr))
    return false;

  const int format = SPNG_FMT_RGBA8;
  size_t decoded_len = 0;
  if (spng_decoded_image_size(ctx.get(), format, &decoded_len))
    return false;

  data_out->reset(decoded_len);
  if (spng_decode_image(ctx.get(), data_out->data(), decoded_len, format, SPNG_DECODE_TRNS))
    return false;

  *width_out = ihdr.width;
  *height_out = ihdr.height;
  return true;
}

static bool SavePNGWithChunks(const std::string& path, const u8* input, ImageByteFormat format,
                              u32 width, u32 height, u32 stride, int level,
                              std::span<spng_unknown_chunk> ancillary_chunks)
{
  Common::Timer timer;
  timer.Start();

  spng_color_type color_type;
  switch (format)
  {
  case ImageByteFormat::RGB:
    color_type = SPNG_COLOR_TYPE_TRUECOLOR;
    break;
  case ImageByteFormat::RGBA:
    color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
    break;
  default:
    ASSERT_MSG(FRAMEDUMP, false, "Invalid format {}", static_cast<int>(format));
    return false;
  }

  auto ctx = make_spng_ctx(SPNG_CTX_ENCODER);
  if (!ctx)
    return false;

  auto outfile = File::IOFile(path, "wb");
  if (spng_set_png_file(ctx.get(), outfile.GetHandle()))
    return false;

  if (spng_set_option(ctx.get(), SPNG_IMG_COMPRESSION_LEVEL, level))
    return false;

  spng_ihdr ihdr{};
  ihdr.width = width;
  ihdr.height = height;
  ihdr.color_type = color_type;
  ihdr.bit_depth = 8;
  if (spng_set_ihdr(ctx.get(), &ihdr))
    return false;

  // libspng only keeps a pointer to the chunks, which must stay alive until encoding is done.
  if (!ancillary_chunks.empty() &&
      spng_set_unknown_chunks(ctx.get(), ancillary_chunks.data(),
                              static_cast<u32>(ancillary_chunks.size())))
  {
    return false;
  }

  if (spng_encode_image(ctx.get(), nullptr, 0, SPNG_FMT_PNG,
                        SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE))
  {
    return false;
  }
  for (u32 row = 0; row < height; row++)
  {
    const int err = spng_encode_row(ctx.get(), &input[row * stride], stride);
    if (err == SPNG_EOI)
      break;
    if (err)
    {
      ERROR_LOG_FMT(FRAMEDUMP, "Failed to save {} by {} image to {} at level {}: error {}", width,
                    height, path, level, err);
      return false;
    }
  }

  size_t image_len = 0;
  spng_decoded_image_size(ctx.get(), SPNG_FMT_PNG, &image_len);
  INFO_LOG_FMT(FRAMEDUMP, "{} byte {} by {} image saved to {} at level {} in {} ms", image_len,
               width, height, path, level, timer.ElapsedMs());
  return true;
}

bool SavePNG(const std::string& path, const u8* input, ImageByteFormat format, u32 width,
             u32 height, u32 stride, int level)
{
  return SavePNGWithChunks(path, input, format, width, height, stride, level, {});
}

// Drops the alpha channel of width RGBA pixels.
static u8* RGBAToRGB(const u8* input, u32 width, u8* output)
{
  for (u32 x = 0; x < width; ++x)
  {
    *output++ = input[x * 4];
    *output++ = input[x * 4 + 1];
    *output++ = input[x * 4 + 2];
  }
  return output;
}

static Common::UniqueBuffer<u8> RGBAToRGB(const u8* input, u32 width, u32 height, u32 row_stride)
{
  Common::UniqueBuffer<u8> buffer;
  buffer.reset(width * height * 3);

  u8* output = buffer.data();
  for (u32 y = 0; y < height; ++y)
    output = RGBAToRGB(input + y * row_stride, width, output);
  return buffer;
}

bool ConvertRGBAToRGBAndSavePNG(const std::string& path, const u8* input, u32 width, u32 height,
                                u32 stride, int level)
{
  const auto data = RGBAToRGB(input, width, height, stride);
  return SavePNG(path, data.data(), ImageByteFormat::RGB, width, height, width * 3, level);
}

bool ConvertRGBAToRGBAndSaveStereoPNG(const std::string& path, const u8* input, u32 view_width,
                                      u32 height, u32 stride, int level)
{
  if (view_width == 0)
    return false;

  // sTER requires the left edge of the right view to be on a column that is a multiple of 8, so
  // that both views line up with the Adam7 interlacing pattern. In the words of the
  // specification, padding = 7 - ((view_width - 1) mod 8).
  const u32 padding = (8 - view_width % 8) % 8;
  const u32 width = view_width * 2 + padding;
  const u32 row_size = width * 3;

  Common::UniqueBuffer<u8> buffer;
  buffer.reset(row_size * height);

  for (u32 y = 0; y < height; ++y)
  {
    const u8* const row = input + y * stride;
    u8* output = RGBAToRGB(row, view_width, &buffer[y * row_size]);
    output = std::fill_n(output, padding * 3, u8{0});
    RGBAToRGB(row + view_width * 4, view_width, output);
  }

  // Mode 1 is the "diverging-fuse layout": the left eye view is on the left, like with
  // side-by-side 3D displays. Mode 0 would mean the views are swapped for cross-eyed viewing.
  u8 ster_mode = 1;
  std::array<spng_unknown_chunk, 1> chunks{{{
      .type = {'s', 'T', 'E', 'R'},
      .length = sizeof(ster_mode),
      .data = &ster_mode,
      .location = SPNG_AFTER_IHDR,
  }}};
  return SavePNGWithChunks(path, buffer.data(), ImageByteFormat::RGB, width, height, row_size,
                           level, chunks);
}
}  // namespace Common
