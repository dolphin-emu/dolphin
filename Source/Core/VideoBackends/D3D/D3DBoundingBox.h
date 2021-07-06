// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "VideoBackends/D3D/D3DBase.h"

namespace DX11
{
class BBox
{
public:
  static ID3D11UnorderedAccessView* GetUAV();
  static void Init();
  static void Shutdown();

  static void Flush();
  static void Readback();

  static void Set(int index, int value);
  static int Get(int index);
};
};  // namespace DX11
