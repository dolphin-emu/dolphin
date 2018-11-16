// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "Common/CommonTypes.h"
#include "VideoCommon/TextureConfig.h"


class FramebufferManagerBase
{
public:
  virtual ~FramebufferManagerBase() = default;

  static unsigned int GetEFBLayers() { return m_EFBLayers; }

  // 32-bit depth clears are broken in the Adreno Vulkan driver, and have no effect.
  // To work around this, we use a D24_S8 buffer instead, which results in a loss of accuracy.
  // We still resolve this to a R32F texture, as there is no 24-bit format.
  static AbstractTextureFormat GetEFBDepthFormat() { return AbstractTextureFormat::D24_S8; }

protected:
  static unsigned int m_EFBLayers;
};

extern std::unique_ptr<FramebufferManagerBase> g_framebuffer_manager;
