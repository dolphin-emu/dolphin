// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureData.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>

#include "Common/Align.h"
#include "Common/IOFile.h"
#include "Common/Image.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
// From https://raw.githubusercontent.com/Microsoft/DirectXTex/master/DirectXTex/DDS.h
//
// This header defines constants and structures that are useful when parsing
// DDS files.  DDS files were originally designed to use several structures
// and constants that are native to DirectDraw and are defined in ddraw.h,
// such as DDSURFACEDESC2 and DDSCAPS2.  This file defines similar
// (compatible) constants and structures so that one can use DDS files
// without needing to include ddraw.h.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
// http://go.microsoft.com/fwlink/?LinkId=248926

#pragma pack(push, 1)

const uint32_t DDS_MAGIC = 0x20534444;  // "DDS "

struct DDS_PIXELFORMAT
{
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwFourCC;
  uint32_t dwRGBBitCount;
  uint32_t dwRBitMask;
  uint32_t dwGBitMask;
  uint32_t dwBBitMask;
  uint32_t dwABitMask;
};

#define DDS_FOURCC 0x00000004      // DDPF_FOURCC
#define DDS_RGB 0x00000040         // DDPF_RGB
#define DDS_RGBA 0x00000041        // DDPF_RGB | DDPF_ALPHAPIXELS
#define DDS_LUMINANCE 0x00020000   // DDPF_LUMINANCE
#define DDS_LUMINANCEA 0x00020001  // DDPF_LUMINANCE | DDPF_ALPHAPIXELS
#define DDS_ALPHA 0x00000002       // DDPF_ALPHA
#define DDS_PAL8 0x00000020        // DDPF_PALETTEINDEXED8
#define DDS_PAL8A 0x00000021       // DDPF_PALETTEINDEXED8 | DDPF_ALPHAPIXELS
#define DDS_BUMPDUDV 0x00080000    // DDPF_BUMPDUDV

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)                                                             \
  ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | ((uint32_t)(uint8_t)(ch2) << 16) | \
   ((uint32_t)(uint8_t)(ch3) << 24))
#endif /* defined(MAKEFOURCC) */

#define DDS_HEADER_FLAGS_TEXTURE                                                                   \
  0x00001007  // DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT
#define DDS_HEADER_FLAGS_MIPMAP 0x00020000      // DDSD_MIPMAPCOUNT
#define DDS_HEADER_FLAGS_VOLUME 0x00800000      // DDSD_DEPTH
#define DDS_HEADER_FLAGS_PITCH 0x00000008       // DDSD_PITCH
#define DDS_HEADER_FLAGS_LINEARSIZE 0x00080000  // DDSD_LINEARSIZE

// Subset here matches D3D10_RESOURCE_DIMENSION and D3D11_RESOURCE_DIMENSION
enum DDS_RESOURCE_DIMENSION
{
  DDS_DIMENSION_TEXTURE1D = 2,
  DDS_DIMENSION_TEXTURE2D = 3,
  DDS_DIMENSION_TEXTURE3D = 4,
};

struct DDS_HEADER
{
  uint32_t dwSize;
  uint32_t dwFlags;
  uint32_t dwHeight;
  uint32_t dwWidth;
  uint32_t dwPitchOrLinearSize;
  uint32_t dwDepth;  // only if DDS_HEADER_FLAGS_VOLUME is set in dwFlags
  uint32_t dwMipMapCount;
  uint32_t dwReserved1[11];
  DDS_PIXELFORMAT ddspf;
  uint32_t dwCaps;
  uint32_t dwCaps2;
  uint32_t dwCaps3;
  uint32_t dwCaps4;
  uint32_t dwReserved2;
};

struct DDS_HEADER_DXT10
{
  uint32_t dxgiFormat;
  uint32_t resourceDimension;
  uint32_t miscFlag;  // see DDS_RESOURCE_MISC_FLAG
  uint32_t arraySize;
  uint32_t miscFlags2;  // see DDS_MISC_FLAGS2
};

#pragma pack(pop)

static_assert(sizeof(DDS_HEADER) == 124, "DDS Header size mismatch");
static_assert(sizeof(DDS_HEADER_DXT10) == 20, "DDS DX10 Extended Header size mismatch");

constexpr DDS_PIXELFORMAT DDSPF_A8R8G8B8 = {
    sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000};
constexpr DDS_PIXELFORMAT DDSPF_X8R8G8B8 = {
    sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000};
constexpr DDS_PIXELFORMAT DDSPF_A8B8G8R8 = {
    sizeof(DDS_PIXELFORMAT), DDS_RGBA, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000};
constexpr DDS_PIXELFORMAT DDSPF_X8B8G8R8 = {
    sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000};
constexpr DDS_PIXELFORMAT DDSPF_R8G8B8 = {
    sizeof(DDS_PIXELFORMAT), DDS_RGB, 0, 24, 0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000};

// End of Microsoft code from DDS.h.

static constexpr bool DDSPixelFormatMatches(const DDS_PIXELFORMAT& pf1, const DDS_PIXELFORMAT& pf2)
{
  return std::tie(pf1.dwSize, pf1.dwFlags, pf1.dwFourCC, pf1.dwRGBBitCount, pf1.dwRBitMask,
                  pf1.dwGBitMask, pf1.dwGBitMask, pf1.dwBBitMask, pf1.dwABitMask) ==
         std::tie(pf2.dwSize, pf2.dwFlags, pf2.dwFourCC, pf2.dwRGBBitCount, pf2.dwRBitMask,
                  pf2.dwGBitMask, pf2.dwGBitMask, pf2.dwBBitMask, pf2.dwABitMask);
}

struct DDSLoadInfo
{
  u32 block_size = 1;
  u32 bytes_per_block = 4;
  u32 width = 0;
  u32 height = 0;
  u32 mip_count = 0;
  AbstractTextureFormat format = AbstractTextureFormat::RGBA8;
  size_t first_mip_offset = 0;
  size_t first_mip_size = 0;
  u32 first_mip_row_length = 0;

  std::function<void(VideoCommon::CustomTextureData::Level*)> conversion_function;
};

static constexpr u32 GetBlockCount(u32 extent, u32 block_size)
{
  return std::max(Common::AlignUp(extent, block_size) / block_size, 1u);
}

static u32 CalculateMipCount(u32 width, u32 height)
{
  u32 mip_width = width;
  u32 mip_height = height;
  u32 mip_count = 1;
  while (mip_width > 1 || mip_height > 1)
  {
    mip_width = std::max(mip_width / 2, 1u);
    mip_height = std::max(mip_height / 2, 1u);
    mip_count++;
  }

  return mip_count;
}

static void ConvertTexture_X8B8G8R8(VideoCommon::CustomTextureData::Level* level)
{
  u8* data_ptr = level->data.data();
  for (u32 row = 0; row < level->height; row++)
  {
    for (u32 x = 0; x < level->row_length; x++)
    {
      // Set alpha channel to full intensity.
      data_ptr[3] = 0xFF;
      data_ptr += sizeof(u32);
    }
  }
}

static void ConvertTexture_A8R8G8B8(VideoCommon::CustomTextureData::Level* level)
{
  u8* data_ptr = level->data.data();
  for (u32 row = 0; row < level->height; row++)
  {
    for (u32 x = 0; x < level->row_length; x++)
    {
      // Byte swap ABGR -> RGBA
      u32 val;
      std::memcpy(&val, data_ptr, sizeof(val));
      val = ((val & 0xFF00FF00) | ((val >> 16) & 0xFF) | ((val << 16) & 0xFF0000));
      std::memcpy(data_ptr, &val, sizeof(u32));
      data_ptr += sizeof(u32);
    }
  }
}

static void ConvertTexture_X8R8G8B8(VideoCommon::CustomTextureData::Level* level)
{
  u8* data_ptr = level->data.data();
  for (u32 row = 0; row < level->height; row++)
  {
    for (u32 x = 0; x < level->row_length; x++)
    {
      // Byte swap XBGR -> RGBX, and set alpha to full intensity.
      u32 val;
      std::memcpy(&val, data_ptr, sizeof(val));
      val = ((val & 0x0000FF00) | ((val >> 16) & 0xFF) | ((val << 16) & 0xFF0000)) | 0xFF000000;
      std::memcpy(data_ptr, &val, sizeof(u32));
      data_ptr += sizeof(u32);
    }
  }
}

static void ConvertTexture_R8G8B8(VideoCommon::CustomTextureData::Level* level)
{
  std::vector<u8> new_data(level->row_length * level->height * sizeof(u32));

  const u8* rgb_data_ptr = level->data.data();
  u8* data_ptr = new_data.data();

  for (u32 row = 0; row < level->height; row++)
  {
    for (u32 x = 0; x < level->row_length; x++)
    {
      // This is BGR in memory.
      u32 val;
      std::memcpy(&val, rgb_data_ptr, sizeof(val));
      val = ((val & 0x0000FF00) | ((val >> 16) & 0xFF) | ((val << 16) & 0xFF0000)) | 0xFF000000;
      std::memcpy(data_ptr, &val, sizeof(u32));
      data_ptr += sizeof(u32);
      rgb_data_ptr += 3;
    }
  }

  level->data = std::move(new_data);
}

static bool ParseDDSHeader(File::IOFile& file, DDSLoadInfo* info)
{
  // Exit as early as possible for non-DDS textures, since all extensions are currently
  // passed through this function.
  u32 magic;
  if (!file.ReadBytes(&magic, sizeof(magic)) || magic != DDS_MAGIC)
    return false;

  DDS_HEADER header;
  size_t header_size = sizeof(header);
  if (!file.ReadBytes(&header, header_size) || header.dwSize < header_size)
    return false;

  // Required fields.
  if ((header.dwFlags & DDS_HEADER_FLAGS_TEXTURE) != DDS_HEADER_FLAGS_TEXTURE)
    return false;

  // Image should be 2D.
  if (header.dwFlags & DDS_HEADER_FLAGS_VOLUME)
    return false;

  // Presence of width/height fields is already tested by DDS_HEADER_FLAGS_TEXTURE.
  info->width = header.dwWidth;
  info->height = header.dwHeight;
  if (info->width == 0 || info->height == 0)
    return false;

  // Check for mip levels.
  if (header.dwFlags & DDS_HEADER_FLAGS_MIPMAP)
  {
    info->mip_count = header.dwMipMapCount;
    if (header.dwMipMapCount != 0)
      info->mip_count = header.dwMipMapCount;
    else
      info->mip_count = CalculateMipCount(info->width, info->height);
  }
  else
  {
    info->mip_count = 1;
  }

  // Handle fourcc formats vs uncompressed formats.
  bool has_fourcc = (header.ddspf.dwFlags & DDS_FOURCC) != 0;
  bool needs_s3tc = false;
  if (has_fourcc)
  {
    // Handle DX10 extension header.
    u32 dxt10_format = 0;
    if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', '1', '0'))
    {
      DDS_HEADER_DXT10 dxt10_header;
      if (!file.ReadBytes(&dxt10_header, sizeof(dxt10_header)))
        return false;

      // Can't handle array textures here. Doesn't make sense to use them, anyway.
      if (dxt10_header.resourceDimension != DDS_DIMENSION_TEXTURE2D || dxt10_header.arraySize != 1)
        return false;

      header_size += sizeof(dxt10_header);
      dxt10_format = dxt10_header.dxgiFormat;
    }

    // Currently, we only handle compressed textures here, and leave the rest to the SOIL loader.
    // In the future, this could be extended, but these isn't much benefit in doing so currently.
    if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '1') || dxt10_format == 71)
    {
      info->format = AbstractTextureFormat::DXT1;
      info->block_size = 4;
      info->bytes_per_block = 8;
      needs_s3tc = true;
    }
    else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '3') || dxt10_format == 74)
    {
      info->format = AbstractTextureFormat::DXT3;
      info->block_size = 4;
      info->bytes_per_block = 16;
      needs_s3tc = true;
    }
    else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '5') || dxt10_format == 77)
    {
      info->format = AbstractTextureFormat::DXT5;
      info->block_size = 4;
      info->bytes_per_block = 16;
      needs_s3tc = true;
    }
    else if (dxt10_format == 98)
    {
      info->format = AbstractTextureFormat::BPTC;
      info->block_size = 4;
      info->bytes_per_block = 16;
      if (!g_ActiveConfig.backend_info.bSupportsBPTCTextures)
        return false;
    }
    else
    {
      // Leave all remaining formats to SOIL.
      return false;
    }
  }
  else
  {
    if (DDSPixelFormatMatches(header.ddspf, DDSPF_A8R8G8B8))
    {
      info->conversion_function = ConvertTexture_A8R8G8B8;
    }
    else if (DDSPixelFormatMatches(header.ddspf, DDSPF_X8R8G8B8))
    {
      info->conversion_function = ConvertTexture_X8R8G8B8;
    }
    else if (DDSPixelFormatMatches(header.ddspf, DDSPF_X8B8G8R8))
    {
      info->conversion_function = ConvertTexture_X8B8G8R8;
    }
    else if (DDSPixelFormatMatches(header.ddspf, DDSPF_R8G8B8))
    {
      info->conversion_function = ConvertTexture_R8G8B8;
    }
    else if (DDSPixelFormatMatches(header.ddspf, DDSPF_A8B8G8R8))
    {
      // This format is already in RGBA order, so no conversion necessary.
    }
    else
    {
      return false;
    }

    // All these formats are RGBA, just with byte swapping.
    info->format = AbstractTextureFormat::RGBA8;
    info->block_size = 1;
    info->bytes_per_block = header.ddspf.dwRGBBitCount / 8;
  }

  // We also need to ensure the backend supports these formats natively before loading them,
  // otherwise, fallback to SOIL, which will decompress them to RGBA.
  if (needs_s3tc && !g_ActiveConfig.backend_info.bSupportsST3CTextures)
    return false;

  // Mip levels smaller than the block size are padded to multiples of the block size.
  u32 blocks_wide = GetBlockCount(info->width, info->block_size);
  u32 blocks_high = GetBlockCount(info->height, info->block_size);

  // Pitch can be specified in the header, otherwise we can derive it from the dimensions. For
  // compressed formats, both DDS_HEADER_FLAGS_LINEARSIZE and DDS_HEADER_FLAGS_PITCH should be
  // set. See https://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
  if (header.dwFlags & DDS_HEADER_FLAGS_PITCH && header.dwFlags & DDS_HEADER_FLAGS_LINEARSIZE)
  {
    // Convert pitch (in bytes) to texels/row length.
    if (header.dwPitchOrLinearSize < info->bytes_per_block)
    {
      // Likely a corrupted or invalid file.
      return false;
    }

    info->first_mip_row_length =
        std::max(header.dwPitchOrLinearSize / info->bytes_per_block, 1u) * info->block_size;
    info->first_mip_size = static_cast<size_t>(info->first_mip_row_length / info->block_size) *
                           info->block_size * blocks_high;
  }
  else
  {
    // Assume no padding between rows of blocks.
    info->first_mip_row_length = blocks_wide * info->block_size;
    info->first_mip_size = blocks_wide * static_cast<size_t>(info->bytes_per_block) * blocks_high;
  }

  // Check for truncated or corrupted files.
  info->first_mip_offset = sizeof(magic) + header_size;
  if (info->first_mip_offset >= file.GetSize())
    return false;

  return true;
}

static bool ReadMipLevel(VideoCommon::CustomTextureData::Level* level, File::IOFile& file,
                         const std::string& filename, u32 mip_level, const DDSLoadInfo& info,
                         u32 width, u32 height, u32 row_length, size_t size)
{
  // D3D11 cannot handle block compressed textures where the first mip level is
  // not a multiple of the block size.
  if (mip_level == 0 && info.block_size > 1 &&
      ((width % info.block_size) != 0 || (height % info.block_size) != 0))
  {
    ERROR_LOG_FMT(VIDEO,
                  "Invalid dimensions for DDS texture {}. For compressed textures of this format, "
                  "the width/height of the first mip level must be a multiple of {}.",
                  filename, info.block_size);
    return false;
  }

  // Copy to the final storage location.
  level->width = width;
  level->height = height;
  level->format = info.format;
  level->row_length = row_length;
  level->data.resize(size);
  if (!file.ReadBytes(level->data.data(), level->data.size()))
    return false;

  // Apply conversion function for uncompressed textures.
  if (info.conversion_function)
    info.conversion_function(level);

  return true;
}

}  // namespace

namespace VideoCommon
{
bool LoadDDSTexture(CustomTextureData* texture, const std::string& filename)
{
  File::IOFile file;
  file.Open(filename, "rb");
  if (!file.IsOpen())
    return false;

  DDSLoadInfo info;
  if (!ParseDDSHeader(file, &info))
    return false;

  // Read first mip level, as it may have a custom pitch.
  CustomTextureData::Level first_level;
  if (!file.Seek(info.first_mip_offset, File::SeekOrigin::Begin) ||
      !ReadMipLevel(&first_level, file, filename, 0, info, info.width, info.height,
                    info.first_mip_row_length, info.first_mip_size))
  {
    return false;
  }

  texture->m_levels.push_back(std::move(first_level));

  // Read in any remaining mip levels in the file.
  // If the .dds file does not contain a full mip chain, we'll fall back to the old path.
  u32 mip_width = info.width;
  u32 mip_height = info.height;
  for (u32 i = 1; i < info.mip_count; i++)
  {
    mip_width = std::max(mip_width / 2, 1u);
    mip_height = std::max(mip_height / 2, 1u);

    // Pitch can't be specified with each mip level, so we have to calculate it ourselves.
    u32 blocks_wide = GetBlockCount(mip_width, info.block_size);
    u32 blocks_high = GetBlockCount(mip_height, info.block_size);
    u32 mip_row_length = blocks_wide * info.block_size;
    size_t mip_size = blocks_wide * static_cast<size_t>(info.bytes_per_block) * blocks_high;
    CustomTextureData::Level level;
    if (!ReadMipLevel(&level, file, filename, i, info, mip_width, mip_height, mip_row_length,
                      mip_size))
      break;

    texture->m_levels.push_back(std::move(level));
  }

  return true;
}

bool LoadDDSTexture(CustomTextureData::Level* level, const std::string& filename, u32 mip_level)
{
  // Only loading a single mip level.
  File::IOFile file;
  file.Open(filename, "rb");
  if (!file.IsOpen())
    return false;

  DDSLoadInfo info;
  if (!ParseDDSHeader(file, &info))
    return false;

  return ReadMipLevel(level, file, filename, mip_level, info, info.width, info.height,
                      info.first_mip_row_length, info.first_mip_size);
}

bool LoadPNGTexture(CustomTextureData* texture, const std::string& filename)
{
  return LoadPNGTexture(&texture->m_levels[0], filename);
}

bool LoadPNGTexture(CustomTextureData::Level* level, const std::string& filename)
{
  if (!level) [[unlikely]]
    return false;

  File::IOFile file;
  file.Open(filename, "rb");
  std::vector<u8> buffer(file.GetSize());
  file.ReadBytes(buffer.data(), file.GetSize());

  if (!Common::LoadPNG(buffer, &level->data, &level->width, &level->height))
    return false;

  if (level->data.empty())
    return false;

  // Loaded PNG images are converted to RGBA.
  level->format = AbstractTextureFormat::RGBA8;
  level->row_length = level->width;
  return true;
}
}  // namespace VideoCommon
