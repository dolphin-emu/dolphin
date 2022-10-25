// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/ConstantManager.h"

class PointerWrap;
struct PortableVertexDeclaration;

// The non-API dependent parts.
class VertexShaderManager
{
public:
  static void Init();
  static void Dirty();
  static void DoState(PointerWrap& p);

  // constant management
  static void SetConstants(const std::vector<std::string>& textures);

  static void InvalidateXFRange(int start, int end);
  static void SetTexMatrixChangedA(u32 value);
  static void SetTexMatrixChangedB(u32 value);
  static void SetViewportChanged();
  static void SetProjectionChanged();
  static void SetMaterialColorChanged(int index);

  static void SetVertexFormat(u32 components, const PortableVertexDeclaration& format);
  static void SetTexMatrixInfoChanged(int index);
  static void SetLightingConfigChanged();

  // data: 3 floats representing the X, Y and Z vertex model coordinates and the posmatrix index.
  // out:  4 floats which will be initialized with the corresponding clip space coordinates
  // NOTE: g_fProjectionMatrix must be up to date when this is called
  //       (i.e. VertexShaderManager::SetConstants needs to be called before using this!)
  static void TransformToClipSpace(const float* data, float* out, u32 mtxIdx);

  static VertexShaderConstants constants;
  static bool dirty;
};
