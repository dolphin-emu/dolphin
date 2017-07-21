// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

class AbstractRawTexture
{
public:
  AbstractRawTexture(const u8* data, u32 stride, u32 width, u32 height);
  virtual ~AbstractRawTexture();

  const u8* GetData() const;
  u32 GetStride() const;
  u32 GetWidth() const;
  u32 GetHeight() const;

  bool Save(const std::string& filename);

protected:
  const u8* m_data;
  u32 m_stride;
  u32 m_width;
  u32 m_height;
};
