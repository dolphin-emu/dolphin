// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>

#include "Common/CommonTypes.h"
#include "VideoCommon/ConstantManager.h"

class PointerWrap;

// The non-API dependent parts.
class PixelShaderManager final
{
public:
  void Init();
  void Dirty();
  void DoState(PointerWrap& p);

  void SetConstants();  // sets pixel shader constants

  // constant management
  // Some of these functions grab the constant values from global state,
  // so make sure to call them after memory is committed
  void SetTevColor(int index, int component, s32 value);
  void SetTevKonstColor(int index, int component, s32 value);
  void SetTevOrder(int index, u32 order);
  void SetTevKSel(int index, u32 ksel);
  void SetTevCombiner(int index, int alpha, u32 combiner);
  void SetAlpha();
  void SetAlphaTestChanged();
  void SetDestAlphaChanged();
  void SetTexDims(int texmapid, u32 width, u32 height);
  void SetSamplerState(int texmapid, u32 tm0, u32 tm1);
  void SetZTextureBias();
  void SetViewportChanged();
  void SetEfbScaleChanged(float scalex, float scaley);
  void SetZSlope(float dfdx, float dfdy, float f0);
  void SetIndMatrixChanged(int matrixidx);
  void SetTevIndirectChanged();
  void SetZTextureTypeChanged();
  void SetZTextureOpChanged();
  void SetIndTexScaleChanged(bool high);
  void SetTexCoordChanged(u8 texmapid);
  void SetFogColorChanged();
  void SetFogParamChanged();
  void SetFogRangeAdjustChanged();
  void SetGenModeChanged();
  void SetZModeControl();
  void SetBlendModeChanged();
  void SetBoundingBoxActive(bool active);

  PixelShaderConstants constants{};
  bool dirty = false;

  // Constants for custom shaders
  std::span<u8> custom_constants;
  bool custom_constants_dirty = false;

private:
  bool m_fog_range_adjusted_changed = false;
  bool m_viewport_changed = false;
  bool m_indirect_dirty = false;
  bool m_dest_alpha_dirty = false;
};
