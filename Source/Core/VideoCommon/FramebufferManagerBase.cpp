// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/FramebufferManagerBase.h"

#include <memory>

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/RenderBase.h"

std::unique_ptr<FramebufferManagerBase> g_framebuffer_manager;

unsigned int FramebufferManagerBase::m_EFBLayers = 1;

FramebufferManagerBase::~FramebufferManagerBase() = default;

AbstractTextureFormat FramebufferManagerBase::GetEFBDepthFormat()
{
  // 32-bit depth clears are broken in the Adreno Vulkan driver, and have no effect.
  // To work around this, we use a D24_S8 buffer instead, which results in a loss of accuracy.
  // We still resolve this to a R32F texture, as there is no 24-bit format.
  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_D32F_CLEAR))
    return AbstractTextureFormat::D24_S8;
  else
    return AbstractTextureFormat::D32F;
}
