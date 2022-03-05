// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/TextureDecoder.h"

struct FBInfo
{
  u32 m_height = 0;
  u32 m_width = 0;
  TextureFormat m_texture_format = TextureFormat::I4;
  u32 CalculateHash() const;
  bool operator==(const FBInfo& other) const;
  bool operator!=(const FBInfo& other) const;
};

struct FBInfoHasher
{
  std::size_t operator()(const FBInfo& fb_info) const noexcept
  {
    return static_cast<std::size_t>(fb_info.CalculateHash());
  }
};
