// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Software/TextureSampler.h"

#include <algorithm>
#include <cmath>
#include <span>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/SpanUtils.h"
#include "Core/HW/Memmap.h"
#include "Core/System.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"

#define ALLOW_MIPMAP 1

namespace TextureSampler
{
static inline void WrapCoord(int* coordp, WrapMode wrap_mode, int image_size)
{
  int coord = *coordp;
  switch (wrap_mode)
  {
  case WrapMode::Clamp:
    coord = std::clamp(coord, 0, image_size - 1);
    break;
  case WrapMode::Repeat:
    // Per YAGCD's info on TX_SETMODE1_I0 (et al.), mirror "requires the texture size to be a power
    // of two. (wrapping is implemented by a logical AND (SIZE-1))".  So though this doesn't wrap
    // nicely for non-power-of-2 sizes, that's how hardware does it.
    coord = coord & (image_size - 1);
    break;
  case WrapMode::Mirror:
  {
    // YAGCD doesn't mention this, but this seems to be the check used to implement mirroring.
    // With power-of-2 sizes, this correctly checks if it's an even-numbered repeat or an
    // odd-numbered one, and thus can decide whether to reflect.  It fails in unusual ways
    // with non-power-of-2 sizes, but seems to match what happens on actual hardware.
    if ((coord & image_size) != 0)
      coord = ~coord;
    coord = coord & (image_size - 1);
    break;
  }
  default:
    // Hardware testing indicates that wrap_mode set to 3 behaves the same as clamp.
    PanicAlertFmt("Invalid wrap mode: {}", wrap_mode);
    coord = std::clamp(coord, 0, image_size - 1);
    break;
  }
  *coordp = coord;
}

static inline void SetTexel(const u8* inTexel, u32* outTexel, u32 fract)
{
  outTexel[0] = inTexel[0] * fract;
  outTexel[1] = inTexel[1] * fract;
  outTexel[2] = inTexel[2] * fract;
  outTexel[3] = inTexel[3] * fract;
}

static inline void AddTexel(const u8* inTexel, u32* outTexel, u32 fract)
{
  outTexel[0] += inTexel[0] * fract;
  outTexel[1] += inTexel[1] * fract;
  outTexel[2] += inTexel[2] * fract;
  outTexel[3] += inTexel[3] * fract;
}

void Sample(s32 s, s32 t, s32 lod, bool linear, u8 texmap, u8* sample)
{
  int baseMip = 0;
  bool mipLinear = false;

#if (ALLOW_MIPMAP)
  auto texUnit = bpmem.tex.GetUnit(texmap);
  const TexMode0& tm0 = texUnit.texMode0;

  const s32 lodFract = lod & 0xf;

  if (lod > 0 && tm0.mipmap_filter != MipMode::None)
  {
    // use mipmap
    baseMip = lod >> 4;
    mipLinear = (lodFract && tm0.mipmap_filter == MipMode::Linear);

    // if using nearest mip filter and lodFract >= 0.5 round up to next mip
    if (tm0.mipmap_filter == MipMode::Point && lodFract >= 8)
      baseMip++;
  }

  if (mipLinear)
  {
    u8 sampledTex[4];
    u32 texel[4];

    SampleMip(s, t, baseMip, linear, texmap, sampledTex);
    SetTexel(sampledTex, texel, (16 - lodFract));

    SampleMip(s, t, baseMip + 1, linear, texmap, sampledTex);
    AddTexel(sampledTex, texel, lodFract);

    sample[0] = (u8)(texel[0] >> 4);
    sample[1] = (u8)(texel[1] >> 4);
    sample[2] = (u8)(texel[2] >> 4);
    sample[3] = (u8)(texel[3] >> 4);
  }
  else
#endif
  {
    SampleMip(s, t, baseMip, linear, texmap, sample);
  }
}

void SampleMip(s32 s, s32 t, s32 mip, bool linear, u8 texmap, u8* sample)
{
  auto texUnit = bpmem.tex.GetUnit(texmap);

  const TexMode0& tm0 = texUnit.texMode0;
  const TexImage0& ti0 = texUnit.texImage0;
  const TexTLUT& texTlut = texUnit.texTlut;
  const TextureFormat texfmt = ti0.format;
  const TLUTFormat tlutfmt = texTlut.tlut_format;

  std::span<const u8> image_src;
  std::span<const u8> image_src_odd;
  if (texUnit.texImage1.cache_manually_managed)
  {
    image_src = TexDecoder_GetTmemSpan(texUnit.texImage1.tmem_even * TMEM_LINE_SIZE);
    if (texfmt == TextureFormat::RGBA8)
      image_src_odd = TexDecoder_GetTmemSpan(texUnit.texImage2.tmem_odd * TMEM_LINE_SIZE);
  }
  else
  {
    auto& system = Core::System::GetInstance();
    auto& memory = system.GetMemory();

    const u32 imageBase = texUnit.texImage3.image_base << 5;
    image_src = memory.GetSpanForAddress(imageBase);
  }

  int image_width_minus_1 = ti0.width;
  int image_height_minus_1 = ti0.height;

  const int tlutAddress = texTlut.tmem_offset << 9;
  const std::span<const u8> tlut = TexDecoder_GetTmemSpan(tlutAddress);

  // reduce sample location and texture size to mip level
  // move texture pointer to mip location
  if (mip)
  {
    int mipWidth = image_width_minus_1 + 1;
    int mipHeight = image_height_minus_1 + 1;

    const int fmtWidth = TexDecoder_GetBlockWidthInTexels(texfmt);
    const int fmtHeight = TexDecoder_GetBlockHeightInTexels(texfmt);
    const int fmtDepth = TexDecoder_GetTexelSizeInNibbles(texfmt);

    image_width_minus_1 >>= mip;
    image_height_minus_1 >>= mip;
    s >>= mip;
    t >>= mip;

    while (mip)
    {
      mipWidth = std::max(mipWidth, fmtWidth);
      mipHeight = std::max(mipHeight, fmtHeight);
      const u32 size = (mipWidth * mipHeight * fmtDepth) >> 1;

      image_src = Common::SafeSubspan(image_src, size);
      mipWidth >>= 1;
      mipHeight >>= 1;
      mip--;
    }
  }

  if (linear)
  {
    // offset linear sampling
    s -= 64;
    t -= 64;

    // integer part of sample location
    int imageS = s >> 7;
    int imageT = t >> 7;

    // linear sampling
    int imageSPlus1 = imageS + 1;
    const int fractS = s & 0x7f;

    int imageTPlus1 = imageT + 1;
    const int fractT = t & 0x7f;

    u8 sampledTex[4];
    u32 texel[4];

    WrapCoord(&imageS, tm0.wrap_s, image_width_minus_1 + 1);
    WrapCoord(&imageT, tm0.wrap_t, image_height_minus_1 + 1);
    WrapCoord(&imageSPlus1, tm0.wrap_s, image_width_minus_1 + 1);
    WrapCoord(&imageTPlus1, tm0.wrap_t, image_height_minus_1 + 1);

    if (!(texfmt == TextureFormat::RGBA8 && texUnit.texImage1.cache_manually_managed))
    {
      TexDecoder_DecodeTexel(sampledTex, image_src, imageS, imageT, image_width_minus_1, texfmt,
                             tlut, tlutfmt);
      SetTexel(sampledTex, texel, (128 - fractS) * (128 - fractT));

      TexDecoder_DecodeTexel(sampledTex, image_src, imageSPlus1, imageT, image_width_minus_1,
                             texfmt, tlut, tlutfmt);
      AddTexel(sampledTex, texel, (fractS) * (128 - fractT));

      TexDecoder_DecodeTexel(sampledTex, image_src, imageS, imageTPlus1, image_width_minus_1,
                             texfmt, tlut, tlutfmt);
      AddTexel(sampledTex, texel, (128 - fractS) * (fractT));

      TexDecoder_DecodeTexel(sampledTex, image_src, imageSPlus1, imageTPlus1, image_width_minus_1,
                             texfmt, tlut, tlutfmt);
      AddTexel(sampledTex, texel, (fractS) * (fractT));
    }
    else
    {
      TexDecoder_DecodeTexelRGBA8FromTmem(sampledTex, image_src, image_src_odd, imageS, imageT,
                                          image_width_minus_1);
      SetTexel(sampledTex, texel, (128 - fractS) * (128 - fractT));

      TexDecoder_DecodeTexelRGBA8FromTmem(sampledTex, image_src, image_src_odd, imageSPlus1, imageT,
                                          image_width_minus_1);
      AddTexel(sampledTex, texel, (fractS) * (128 - fractT));

      TexDecoder_DecodeTexelRGBA8FromTmem(sampledTex, image_src, image_src_odd, imageS, imageTPlus1,
                                          image_width_minus_1);
      AddTexel(sampledTex, texel, (128 - fractS) * (fractT));

      TexDecoder_DecodeTexelRGBA8FromTmem(sampledTex, image_src, image_src_odd, imageSPlus1,
                                          imageTPlus1, image_width_minus_1);
      AddTexel(sampledTex, texel, (fractS) * (fractT));
    }

    sample[0] = (u8)(texel[0] >> 14);
    sample[1] = (u8)(texel[1] >> 14);
    sample[2] = (u8)(texel[2] >> 14);
    sample[3] = (u8)(texel[3] >> 14);
  }
  else
  {
    // integer part of sample location
    int imageS = s >> 7;
    int imageT = t >> 7;

    // nearest neighbor sampling
    WrapCoord(&imageS, tm0.wrap_s, image_width_minus_1 + 1);
    WrapCoord(&imageT, tm0.wrap_t, image_height_minus_1 + 1);

    if (!(texfmt == TextureFormat::RGBA8 && texUnit.texImage1.cache_manually_managed))
    {
      TexDecoder_DecodeTexel(sample, image_src, imageS, imageT, image_width_minus_1, texfmt, tlut,
                             tlutfmt);
    }
    else
    {
      TexDecoder_DecodeTexelRGBA8FromTmem(sample, image_src, image_src_odd, imageS, imageT,
                                          image_width_minus_1);
    }
  }
}
}  // namespace TextureSampler
