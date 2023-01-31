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
  void SetProjectionMatrix();
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
  // NOTE: m_projection_matrix must be up to date when this is called
  //       (i.e. VertexShaderManager::SetConstants needs to be called before using this!)
  void TransformToClipSpace(const float* data, float* out, u32 mtxIdx);

  static bool UseVertexDepthRange();

  VertexShaderConstants constants{};
  bool dirty = false;



private:
  alignas(16) std::array<float, 16> m_projection_matrix;

  // track changes
  std::array<bool, 2> m_tex_matrices_changed{};
  bool m_pos_normal_matrix_changed = false;
  bool m_projection_changed = false;
  bool m_viewport_changed = false;
  bool m_tex_mtx_info_changed = false;
  bool m_lighting_config_changed = false;
  bool m_projection_graphics_mod_change = false;
  BitSet32 m_materials_changed;
  std::array<int, 2> m_minmax_transform_matrices_changed{};
  std::array<int, 2> m_minmax_normal_matrices_changed{};
  std::array<int, 2> m_minmax_post_transform_matrices_changed{};
  std::array<int, 2> m_minmax_lights_changed{};

  Common::Matrix44 m_viewport_correction{};

  Common::Matrix44 LoadProjectionMatrix();
};
