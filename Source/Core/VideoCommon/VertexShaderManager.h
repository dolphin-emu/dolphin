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
#include "VideoCommon/NativeVertexFormat.h"

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

  static DOLPHIN_FORCE_INLINE void UpdateValue(bool* dirty, u32* old_value, u32 new_value)
  {
    if (*old_value == new_value)
      return;
    *old_value = new_value;
    *dirty = true;
  }

  static DOLPHIN_FORCE_INLINE void UpdateOffset(bool* dirty, bool include_components,
                                                u32* old_value, const AttributeFormat& attribute)
  {
    if (!attribute.enable)
      return;
    u32 new_value = attribute.offset / 4;  // GPU uses uint offsets
    if (include_components)
      new_value |= attribute.components << 16;
    UpdateValue(dirty, old_value, new_value);
  }

  template <size_t N>
  static DOLPHIN_FORCE_INLINE void UpdateOffsets(bool* dirty, bool include_components,
                                                 std::array<u32, N>* old_value,
                                                 const std::array<AttributeFormat, N>& attribute)
  {
    for (size_t i = 0; i < N; i++)
      UpdateOffset(dirty, include_components, &(*old_value)[i], attribute[i]);
  }

  DOLPHIN_FORCE_INLINE void SetVertexFormat(u32 components, const PortableVertexDeclaration& format)
  {
    UpdateValue(&dirty, &constants.components, components);
    UpdateValue(&dirty, &constants.vertex_stride, format.stride / 4);
    UpdateOffset(&dirty, true, &constants.vertex_offset_position, format.position);
    UpdateOffset(&dirty, false, &constants.vertex_offset_posmtx, format.posmtx);
    UpdateOffsets(&dirty, true, &constants.vertex_offset_texcoords, format.texcoords);
    UpdateOffsets(&dirty, false, &constants.vertex_offset_colors, format.colors);
    UpdateOffsets(&dirty, false, &constants.vertex_offset_normals, format.normals);
  }

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
