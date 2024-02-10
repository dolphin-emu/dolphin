// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/BitSet.h"

class PointerWrap;

// This class manages how XF state changes over
// a period of time (typically a single draw call)
class XFStateManager
{
public:
  void Init();
  void DoState(PointerWrap& p);

  void InvalidateXFRange(int start, int end);

  void SetTexMatrixChangedA(u32 value);
  bool DidTexMatrixAChange() const { return m_tex_matrices_changed[0]; }
  void ResetTexMatrixAChange();

  void SetTexMatrixChangedB(u32 value);
  bool DidTexMatrixBChange() const { return m_tex_matrices_changed[1]; }
  void ResetTexMatrixBChange();

  bool DidPosNormalChange() const { return m_pos_normal_matrix_changed; }
  void ResetPosNormalChange();

  void SetProjectionChanged();
  bool DidProjectionChange() const { return m_projection_changed; }
  void ResetProjection();

  void SetViewportChanged();
  bool DidViewportChange() const { return m_viewport_changed; }
  void ResetViewportChange();

  void SetTexMatrixInfoChanged(int index);
  bool DidTexMatrixInfoChange() const { return m_tex_mtx_info_changed; }
  void ResetTexMatrixInfoChange();

  void SetLightingConfigChanged();
  bool DidLightingConfigChange() const { return m_lighting_config_changed; }
  void ResetLightingConfigChange();

  const std::array<int, 2>& GetLightsChanged() const { return m_minmax_lights_changed; }
  void ResetLightsChanged();

  void SetMaterialColorChanged(int index);
  const BitSet32& GetMaterialChanges() const { return m_materials_changed; }
  void ResetMaterialChanges();

  const std::array<int, 2>& GetPerVertexTransformMatrixChanges() const
  {
    return m_minmax_transform_matrices_changed;
  }
  void ResetPerVertexTransformMatrixChanges();

  const std::array<int, 2>& GetPerVertexNormalMatrixChanges() const
  {
    return m_minmax_normal_matrices_changed;
  }
  void ResetPerVertexNormalMatrixChanges();

  const std::array<int, 2>& GetPostTransformMatrixChanges() const
  {
    return m_minmax_post_transform_matrices_changed;
  }
  void ResetPostTransformMatrixChanges();

private:
  // track changes
  std::array<bool, 2> m_tex_matrices_changed{};
  bool m_pos_normal_matrix_changed = false;
  bool m_projection_changed = false;
  bool m_viewport_changed = false;
  bool m_tex_mtx_info_changed = false;
  bool m_lighting_config_changed = false;
  BitSet32 m_materials_changed;
  std::array<int, 2> m_minmax_transform_matrices_changed{};
  std::array<int, 2> m_minmax_normal_matrices_changed{};
  std::array<int, 2> m_minmax_post_transform_matrices_changed{};
  std::array<int, 2> m_minmax_lights_changed{};
};
