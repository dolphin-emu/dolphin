// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "VideoCommon/AbstractRawTexture.h"

namespace OGL
{
class StagingTexture2D;

class OGLTextureRaw final : public AbstractRawTexture
{
public:
  OGLTextureRaw(u32 stride, u32 width, u32 height, std::vector<u8>&& glData);

private:
  std::vector<u8> m_glData;
};

}  // namespace OGL
