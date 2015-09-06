// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/OGL/GLUtil.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{

// Converts textures between formats using shaders
// TODO: support multiple texture formats
namespace TextureConverter
{

void Init();
void Shutdown();

void EncodeToRamYUYV(GLuint srcTexture, const TargetRectangle& sourceRc,
	u8* destAddr, u32 dstWidth, u32 dstStride, u32 dstHeight);

void DecodeToTexture(u32 xfbAddr, int srcWidth, int srcHeight, GLuint destTexture);

// returns size of the encoded data (in bytes)
void EncodeToRamFromTexture(u8 *dest_ptr, const TextureCache::TCacheEntryBase *texture_entry,
	GLuint source_texture, bool bFromZBuffer, bool bIsIntensityFmt, int bScaleByHalf, const EFBRectangle& source);

}

}  // namespace OGL
