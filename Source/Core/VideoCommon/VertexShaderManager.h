// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <vector>

#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/Matrix.h"
#include "VideoCommon/ConstantManager.h"

class PointerWrap;
struct PortableVertexDeclaration;

// The non-API dependent parts.
class alignas(16) VertexShaderManager
{
public:
  void Init();
  void Dirty();
  void DoState(PointerWrap& p);

  // constant management
  void SetConstants(const std::vector<std::string>& textures);

  void InvalidateXFRange(int start, int end);
  void SetTexMatrixChangedA(u32 value);
  void SetTexMatrixChangedB(u32 value);
  void SetViewportChanged();
  void SetProjectionChanged();
  void SetMaterialColorChanged(int index);

  void SetVertexFormat(u32 components, const PortableVertexDeclaration& format);
  void SetTexMatrixInfoChanged(int index);
  void SetLightingConfigChanged();

  // data: 3 floats representing the X, Y and Z vertex model coordinates and the posmatrix index.
  // out:  4 floats which will be initialized with the corresponding clip space coordinates
  // NOTE: g_fProjectionMatrix must be up to date when this is called
  //       (i.e. VertexShaderManager::SetConstants needs to be called before using this!)
  void TransformToClipSpace(const float* data, float* out, u32 mtxIdx);

  VertexShaderConstants constants{};
  bool dirty = false;

private:
  alignas(16) std::array<float, 16> g_fProjectionMatrix;

  // track changes
  std::array<bool, 2> bTexMatricesChanged{};
  bool bPosNormalMatrixChanged = false;
  bool bProjectionChanged = false;
  bool bViewportChanged = false;
  bool bTexMtxInfoChanged = false;
  bool bLightingConfigChanged = false;
  bool bProjectionGraphicsModChange = false;
  BitSet32 nMaterialsChanged;
  std::array<int, 2> nTransformMatricesChanged{};      // min,max
  std::array<int, 2> nNormalMatricesChanged{};         // min,max
  std::array<int, 2> nPostTransformMatricesChanged{};  // min,max
  std::array<int, 2> nLightsChanged{};                 // min,max

  Common::Matrix44 s_viewportCorrection{};
};
