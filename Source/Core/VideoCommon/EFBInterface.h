// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

enum class EFBReinterpretType;
class FramebufferManager;

class EFBInterfaceBase
{
public:
  virtual ~EFBInterfaceBase();

  virtual void ReinterpretPixelData(FramebufferManager* framebuffer_manager,
                                    EFBReinterpretType convtype) = 0;

  virtual void PokeColor(FramebufferManager* framebuffer_manager, u16 x, u16 y, u32 color) = 0;
  virtual void PokeDepth(FramebufferManager* framebuffer_manager, u16 x, u16 y, u32 depth) = 0;

  u32 PeekColor(FramebufferManager* framebuffer_manager, u16 x, u16 y);
  u32 PeekDepth(FramebufferManager* framebuffer_manager, u16 x, u16 y);

protected:
  bool ShouldSkipAccess(u16 x, u16 y) const;

  virtual u32 PeekColorInternal(FramebufferManager* framebuffer_manager, u16 x, u16 y) = 0;
  virtual u32 PeekDepthInternal(FramebufferManager* framebuffer_manager, u16 x, u16 y) = 0;
};

class HardwareEFBInterface final : public EFBInterfaceBase
{
  void ReinterpretPixelData(FramebufferManager* framebuffer_manager,
                            EFBReinterpretType convtype) override;

  void PokeColor(FramebufferManager* framebuffer_manager, u16 x, u16 y, u32 color) override;
  void PokeDepth(FramebufferManager* framebuffer_manager, u16 x, u16 y, u32 depth) override;

  u32 PeekColorInternal(FramebufferManager* framebuffer_manager, u16 x, u16 y) override;
  u32 PeekDepthInternal(FramebufferManager* framebuffer_manager, u16 x, u16 y) override;
};

extern std::unique_ptr<EFBInterfaceBase> g_efb_interface;
