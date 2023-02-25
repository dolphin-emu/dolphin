// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/FBInfo.h"

#include "Common/Hash.h"

u32 FBInfo::CalculateHash() const
{
  return Common::HashAdler32(reinterpret_cast<const u8*>(this), sizeof(FBInfo));
}

bool FBInfo::operator==(const FBInfo& other) const
{
  return m_height == other.m_height && m_width == other.m_width &&
         m_texture_format == other.m_texture_format;
}

bool FBInfo::operator!=(const FBInfo& other) const
{
  return !(*this == other);
}
