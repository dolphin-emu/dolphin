// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

inline bool AddressRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
  return !((aLower >= bUpper) || (bLower >= aUpper));
}

class FramebufferManagerBase
{
public:
  virtual ~FramebufferManagerBase();

  static unsigned int GetEFBLayers() { return m_EFBLayers; }

protected:
  static unsigned int m_EFBLayers;
};

extern std::unique_ptr<FramebufferManagerBase> g_framebuffer_manager;
