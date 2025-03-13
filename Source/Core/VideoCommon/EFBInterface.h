// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

enum class EFBReinterpretType;

class EFBInterfaceBase
{
public:
  virtual ~EFBInterfaceBase();

  virtual void ReinterpretPixelData(EFBReinterpretType convtype) = 0;

  virtual void PokeColor(u16 x, u16 y, u32 color) = 0;
  virtual void PokeDepth(u16 x, u16 y, u32 depth) = 0;

  u32 PeekColor(u16 x, u16 y);
  virtual u32 PeekDepth(u16 x, u16 y) = 0;

protected:
  virtual u32 PeekColorInternal(u16 x, u16 y) = 0;
};

class HardwareEFBInterface final : public EFBInterfaceBase
{
  void ReinterpretPixelData(EFBReinterpretType convtype) override;

  void PokeColor(u16 x, u16 y, u32 color) override;
  void PokeDepth(u16 x, u16 y, u32 depth) override;

  u32 PeekColorInternal(u16 x, u16 y) override;
  u32 PeekDepth(u16 x, u16 y) override;
};

extern std::unique_ptr<EFBInterfaceBase> g_efb_interface;
