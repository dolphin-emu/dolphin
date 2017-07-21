// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/OGLTextureRaw.h"

namespace OGL
{
OGLTextureRaw::OGLTextureRaw(u32 stride, u32 width, u32 height, std::vector<u8>&& glData)
    : AbstractRawTexture(nullptr, stride, width, height), m_glData(std::move(glData))
{
  m_data = m_glData.data();
}

}  // namespace OGL
