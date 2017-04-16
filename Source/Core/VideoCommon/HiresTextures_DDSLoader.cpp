// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/HiresTextures.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Common/Align.h"
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

bool HiresTexture::LoadDDSTexture(Level& level, const std::vector<u8>& buffer)
{
  u32 magic;
  std::memcpy(&magic, buffer.data(), sizeof(magic));

  // Exit as early as possible for non-DDS textures, since all extensions are currently
  // passed through this function.
  if (magic != DDS_MAGIC)
    return false;

  DDS_HEADER header;
  std::memcpy(&header, &buffer[sizeof(magic)], sizeof(header));
  if (header.dwSize < sizeof(header))
    return false;

  // Required fields.
  if ((header.dwFlags & DDS_HEADER_FLAGS_TEXTURE) != DDS_HEADER_FLAGS_TEXTURE)
    return false;

  // Image should be 2D.
  if (header.dwFlags & DDS_HEADER_FLAGS_VOLUME)
    return false;

  // Presence of width/height fields is already tested by DDS_HEADER_FLAGS_TEXTURE.
  level.width = header.dwWidth;
  level.height = header.dwHeight;

  // Currently, we only handle compressed textures here, and leave the rest to the SOIL loader.
  // In the future, this could be extended, but these isn't much benefit in doing so currently.
  // TODO: DX10 extension header handling.
  u32 block_size = 1;
  u32 bytes_per_block = 4;
  bool needs_s3tc = false;
  if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '1'))
  {
    level.format = HostTextureFormat::DXT1;
    block_size = 4;
    bytes_per_block = 8;
    needs_s3tc = true;
  }
  else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '3'))
  {
    level.format = HostTextureFormat::DXT3;
    block_size = 4;
    bytes_per_block = 16;
    needs_s3tc = true;
  }
  else if (header.ddspf.dwFourCC == MAKEFOURCC('D', 'X', 'T', '5'))
  {
    level.format = HostTextureFormat::DXT5;
    block_size = 4;
    bytes_per_block = 16;
    needs_s3tc = true;
  }
  else
  {
    // Leave all remaining formats to SOIL.
    return false;
  }

  // We also need to ensure the backend supports these formats natively before loading them,
  // otherwise, fallback to SOIL, which will decompress them to RGBA.
  if (needs_s3tc && !g_ActiveConfig.backend_info.bSupportsST3CTextures)
    return false;

  // Mip levels smaller than the block size are padded to multiples of the block size.
  u32 blocks_wide = std::max(level.width / block_size, 1u);
  u32 blocks_high = std::max(level.height / block_size, 1u);

  // Pitch can be specified in the header, otherwise we can derive it from the dimensions. For
  // compressed formats, both DDS_HEADER_FLAGS_LINEARSIZE and DDS_HEADER_FLAGS_PITCH should be
  // set. See https://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
  if (header.dwFlags & DDS_HEADER_FLAGS_PITCH && header.dwFlags & DDS_HEADER_FLAGS_LINEARSIZE)
  {
    // Convert pitch (in bytes) to texels/row length.
    if (header.dwPitchOrLinearSize < bytes_per_block)
    {
      // Likely a corrupted or invalid file.
      return false;
    }

    level.row_length = std::max(header.dwPitchOrLinearSize / bytes_per_block, 1u) * block_size;
    level.data_size = static_cast<size_t>(level.row_length / block_size) * block_size * blocks_high;
  }
  else
  {
    // Assume no padding between rows of blocks.
    level.row_length = blocks_wide * block_size;
    level.data_size = blocks_wide * static_cast<size_t>(bytes_per_block) * blocks_high;
  }

  // Check for truncated or corrupted files.
  size_t data_offset = sizeof(magic) + sizeof(DDS_HEADER);
  if ((data_offset + level.data_size) > buffer.size())
    return false;

  // Copy to the final storage location. The deallocator here is simple, nothing extra is
  // needed, compared to the SOIL-based loader.
  level.data = ImageDataPointer(new u8[level.data_size], [](u8* data) { delete[] data; });
  std::memcpy(level.data.get(), &buffer[data_offset], level.data_size);
  return true;
}
