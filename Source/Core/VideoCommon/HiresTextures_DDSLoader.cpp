// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/HiresTextures.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Common/Align.h"
#include "Common/FileUtil.h"
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

#define DDS_HEIGHT 0x00000002  // DDSD_HEIGHT
#define DDS_WIDTH 0x00000004   // DDSD_WIDTH

#define DDS_SURFACE_FLAGS_TEXTURE 0x00001000  // DDSCAPS_TEXTURE
#define DDS_SURFACE_FLAGS_MIPMAP 0x00400008   // DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
#define DDS_SURFACE_FLAGS_CUBEMAP 0x00000008  // DDSCAPS_COMPLEX

#define DDS_CUBEMAP_POSITIVEX 0x00000600  // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00  // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200  // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200  // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200  // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200  // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES                                                                       \
  (DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX | DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY | \
   DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ)

#define DDS_CUBEMAP 0x00000200  // DDSCAPS2_CUBEMAP

#define DDS_FLAGS_VOLUME 0x00200000  // DDSCAPS2_VOLUME

// Subset here matches D3D10_RESOURCE_DIMENSION and D3D11_RESOURCE_DIMENSION
enum DDS_RESOURCE_DIMENSION
{
  DDS_DIMENSION_TEXTURE1D = 2,
  DDS_DIMENSION_TEXTURE2D = 3,
  DDS_DIMENSION_TEXTURE3D = 4,
};

// Subset here matches D3D10_RESOURCE_MISC_FLAG and D3D11_RESOURCE_MISC_FLAG
enum DDS_RESOURCE_MISC_FLAG
{
  DDS_RESOURCE_MISC_TEXTURECUBE = 0x4L,
};

enum DDS_MISC_FLAGS2
{
  DDS_MISC_FLAGS2_ALPHA_MODE_MASK = 0x7L,
};

enum DDS_ALPHA_MODE
{
  DDS_ALPHA_MODE_UNKNOWN = 0,
  DDS_ALPHA_MODE_STRAIGHT = 1,
  DDS_ALPHA_MODE_PREMULTIPLIED = 2,
  DDS_ALPHA_MODE_OPAQUE = 3,
  DDS_ALPHA_MODE_CUSTOM = 4,
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

}  // namespace

struct DDSLoadInfo
{
  u32 block_size = 1;
  u32 bytes_per_block = 4;
  u32 width = 0;
  u32 height = 0;
  u32 mip_count = 0;
  HostTextureFormat format = HostTextureFormat::RGBA8;
  size_t first_mip_offset = 0;
  size_t first_mip_size = 0;
  u32 first_mip_row_length = 0;
};

static u32 GetBlockCount(u32 extent, u32 block_size)
{
  return std::max(Common::AlignUp(extent, block_size) / block_size, 1u);
}

static u32 CalculateMipCount(u32 width, u32 height)
{
  u32 mip_width = std::max(width / 2, 1u);
  u32 mip_height = std::max(height / 2, 1u);
  u32 mip_count = 1;
  while (mip_width > 1 || mip_height > 1)
  {
    mip_width = std::max(mip_width / 2, 1u);
    mip_height = std::max(mip_height / 2, 1u);
    mip_count++;
  }

  return mip_count;
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
    // Miplevels = 0 means full mip chain?
    // Some files may specify a number too large here, which doesn't play well with the backends.
    info->mip_count = header.dwMipMapCount;
    if (info->mip_count != 0)
      info->mip_count = std::min(info->mip_count, CalculateMipCount(info->width, info->height));
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
      info->format = HostTextureFormat::DXT1;
      info->block_size = 4;
      info->bytes_per_block = 8;
      needs_s3tc = true;
    }
    else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '3') || dxt10_format == 74)
    {
      info->format = HostTextureFormat::DXT3;
      info->block_size = 4;
      info->bytes_per_block = 16;
      needs_s3tc = true;
    }
    else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '5') || dxt10_format == 77)
    {
      info->format = HostTextureFormat::DXT5;
      info->block_size = 4;
      info->bytes_per_block = 16;
      needs_s3tc = true;
    }
    else
    {
      // Leave all remaining formats to SOIL.
      return false;
    }
  }
  else
  {
    // TODO: Support RGBA8 and friends.
    return false;
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

static bool ReadMipLevel(HiresTexture::Level& level, File::IOFile& file, u32 width, u32 height,
                         HostTextureFormat format, u32 row_length, size_t size)
{
  // Copy to the final storage location. The deallocator here is simple, nothing extra is
  // needed, compared to the SOIL-based loader.
  level.width = width;
  level.height = height;
  level.format = format;
  level.row_length = row_length;
  level.data_size = size;
  level.data =
      HiresTexture::ImageDataPointer(new u8[level.data_size], [](u8* data) { delete[] data; });
  if (!file.ReadBytes(level.data.get(), level.data_size))
  {
    level.data.reset();
    return false;
  }

  return true;
}

bool HiresTexture::LoadDDSTexture(HiresTexture* tex, const std::string& filename)
{
  File::IOFile file;
  file.Open(filename, "rb");
  if (!file.IsOpen())
    return false;

  DDSLoadInfo info;
  if (!ParseDDSHeader(file, &info))
    return false;

  // Read first mip level, as it may have a custom pitch.
  Level first_level;
  if (!file.Seek(info.first_mip_offset, SEEK_SET) ||
      !ReadMipLevel(first_level, file, info.width, info.height, info.format,
                    info.first_mip_row_length, info.first_mip_size))
  {
    return false;
  }

  tex->m_levels.push_back(std::move(first_level));

  // Read in any remaining mip levels in the file.
  // If the .dds file does not contain a full mip chain, we'll fall back to the old path.
  u32 mip_width = std::max(info.width / 2, 1u);
  u32 mip_height = std::max(info.height / 2, 1u);
  for (u32 i = 1; i < info.mip_count; i++)
  {
    // Pitch can't be specified with each mip level, so we have to calculate it ourselves.
    u32 blocks_wide = GetBlockCount(mip_width, info.block_size);
    u32 blocks_high = GetBlockCount(mip_height, info.block_size);
    u32 mip_row_length = blocks_wide * info.block_size;
    size_t mip_size = blocks_wide * static_cast<size_t>(info.bytes_per_block) * blocks_high;
    Level level;
    if (!ReadMipLevel(level, file, mip_width, mip_height, info.format, mip_row_length, mip_size))
      break;

    tex->m_levels.push_back(std::move(level));
    mip_width = std::max(mip_width / 2, 1u);
    mip_height = std::max(mip_height / 2, 1u);
  }

  return true;
}

bool HiresTexture::LoadDDSTexture(Level& level, const std::string& filename)
{
  // Only loading a single mip level.
  File::IOFile file;
  file.Open(filename, "rb");
  if (!file.IsOpen())
    return false;

  DDSLoadInfo info;
  if (!ParseDDSHeader(file, &info))
    return false;

  return ReadMipLevel(level, file, info.width, info.height, info.format, info.first_mip_row_length,
                      info.first_mip_size);
}
