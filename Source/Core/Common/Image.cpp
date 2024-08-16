// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Image.h"

#include <memory>
#include <string>
#include <vector>

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

bool LoadPNG(const std::vector<u8>& input, std::vector<u8>* data_out, u32* width_out,
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

  constexpr int format = SPNG_FMT_RGBA8;
  size_t decoded_len = 0;
  if (spng_decoded_image_size(ctx.get(), format, &decoded_len))
    return false;

  data_out->resize(decoded_len);
  if (spng_decode_image(ctx.get(), data_out->data(), decoded_len, format, SPNG_DECODE_TRNS))
    return false;

  *width_out = ihdr.width;
  *height_out = ihdr.height;
  return true;
}

bool SavePNG(const std::string& path, const u8* input, ImageByteFormat format, u32 width,
             u32 height, u32 stride, int level)
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

static std::vector<u8> RGBAToRGB(const u8* input, u32 width, u32 height, u32 row_stride)
{
  std::vector<u8> buffer;
  buffer.reserve(width * height * 3);

  for (u32 y = 0; y < height; ++y)
  {
    const u8* pos = input + y * row_stride;
    for (u32 x = 0; x < width; ++x)
    {
      buffer.push_back(pos[x * 4]);
      buffer.push_back(pos[x * 4 + 1]);
      buffer.push_back(pos[x * 4 + 2]);
    }
  }
  return buffer;
}

bool ConvertRGBAToRGBAndSavePNG(const std::string& path, const u8* input, u32 width, u32 height,
                                u32 stride, int level)
{
  const std::vector<u8> data = RGBAToRGB(input, width, height, stride);
  return SavePNG(path, data.data(), ImageByteFormat::RGB, width, height, width * 3, level);
}
}  // namespace Common
