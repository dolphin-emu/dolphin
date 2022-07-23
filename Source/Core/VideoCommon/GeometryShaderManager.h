// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/ConstantManager.h"

class PointerWrap;
enum class PrimitiveType : u32;

// The non-API dependent parts.
class GeometryShaderManager
{
public:
  static void Init();
  static void Dirty();
  static void DoState(PointerWrap& p);

  static void SetConstants(PrimitiveType prim);
  static void SetViewportChanged();
  static void SetProjectionChanged();
  static void SetLinePtWidthChanged();
  static void SetTexCoordChanged(u8 texmapid);

  static GeometryShaderConstants constants;
  static bool dirty;
};
