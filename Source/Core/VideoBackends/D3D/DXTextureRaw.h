// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoCommon/AbstractRawTexture.h"

struct ID3D11Texture2D;

namespace DX11
{
class DXTextureRaw final : public AbstractRawTexture
{
public:
  DXTextureRaw(const u8* data, u32 stride, u32 width, u32 height, ID3D11Texture2D* texture);
  ~DXTextureRaw();

private:
  ID3D11Texture2D* m_stagingTexture;
};

}  // namespace DX11
