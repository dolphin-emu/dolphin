// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/ConstantManager.h"

class StateLoadStore;

// The non-API dependent parts.
class PixelShaderManager
{
public:
  static void Init();
  static void Dirty();
  static void DoState(StateLoadStore& p);

  static void SetConstants();  // sets pixel shader constants

  // constant management
  // Some of these functions grab the constant values from global state,
  // so make sure to call them after memory is committed
  static void SetTevColorRA(int index);
  static void SetTevColorBG(int index);
  static void SetAlpha();
  static void SetDestAlpha();
  static void SetTexDims(int texmapid, u32 width, u32 height);
  static void SetZTextureBias();
  static void SetViewportChanged();
  static void SetEfbScaleChanged();
  static void SetZSlope(float dfdx, float dfdy, float f0);
  static void SetIndMatrixChanged(int matrixidx);
  static void SetZTextureTypeChanged();
  static void SetIndTexScaleChanged(bool high);
  static void SetTexCoordChanged(u8 texmapid);
  static void SetFogColorChanged();
  static void SetFogParamChanged();
  static void SetFogRangeAdjustChanged();

  static PixelShaderConstants constants;
  static bool dirty;

  static bool s_bFogRangeAdjustChanged;
  static bool s_bViewPortChanged;
};
