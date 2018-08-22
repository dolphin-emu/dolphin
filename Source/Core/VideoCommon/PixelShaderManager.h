// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/ConstantManager.h"

class PointerWrap;

// The non-API dependent parts.
class PixelShaderManager
{
public:
  static void Init();
  static void Dirty();
  static void DoState(PointerWrap& p);

  static void SetConstants();  // sets pixel shader constants

  // constant management
  // Some of these functions grab the constant values from global state,
  // so make sure to call them after memory is committed
  static void SetTevColor(int index, int component, s32 value);
  static void SetTevKonstColor(int index, int component, s32 value);
  static void SetTevOrder(int index, u32 order);
  static void SetTevKSel(int index, u32 ksel);
  static void SetTevCombiner(int index, int alpha, u32 combiner);
  static void SetAlpha();
  static void SetAlphaTestChanged();
  static void SetDestAlphaChanged();
  static void SetTexDims(int texmapid, u32 width, u32 height);
  static void SetZTextureBias();
  static void SetViewportChanged();
  static void SetEfbScaleChanged(float scalex, float scaley);
  static void SetZSlope(float dfdx, float dfdy, float f0);
  static void SetIndMatrixChanged(int matrixidx);
  static void SetTevIndirectChanged();
  static void SetZTextureTypeChanged();
  static void SetZTextureOpChanged();
  static void SetIndTexScaleChanged(bool high);
  static void SetTexCoordChanged(u8 texmapid);
  static void SetFogColorChanged();
  static void SetFogParamChanged();
  static void SetFogRangeAdjustChanged();
  static void SetGenModeChanged();
  static void SetZModeControl();
  static void SetBlendModeChanged();
  static void SetBoundingBoxActive(bool active);

  static PixelShaderConstants constants;
  static bool dirty;
};
