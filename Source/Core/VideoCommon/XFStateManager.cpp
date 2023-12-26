// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/XFStateManager.h"

#include "Common/ChunkFile.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/XFMemory.h"

void XFStateManager::Init()
{
  // Initialize state tracking variables
  ResetTexMatrixAChange();
  ResetTexMatrixBChange();
  ResetPosNormalChange();
  ResetProjection();
  ResetViewportChange();
  ResetTexMatrixInfoChange();
  ResetLightingConfigChange();
  ResetLightsChanged();
  ResetMaterialChanges();
  ResetPerVertexTransformMatrixChanges();
  ResetPerVertexNormalMatrixChanges();
  ResetPostTransformMatrixChanges();

  std::memset(static_cast<void*>(&xfmem), 0, sizeof(xfmem));
}

void XFStateManager::DoState(PointerWrap& p)
{
  p.DoArray(m_minmax_transform_matrices_changed);
  p.DoArray(m_minmax_normal_matrices_changed);
  p.DoArray(m_minmax_post_transform_matrices_changed);
  p.DoArray(m_minmax_lights_changed);

  p.Do(m_materials_changed);
  p.DoArray(m_tex_matrices_changed);
  p.Do(m_pos_normal_matrix_changed);
  p.Do(m_projection_changed);
  p.Do(m_viewport_changed);
  p.Do(m_tex_mtx_info_changed);
  p.Do(m_lighting_config_changed);

  if (p.IsReadMode())
  {
    // This is called after a savestate is loaded.
    // Any constants that can changed based on settings should be re-calculated
    m_projection_changed = true;
  }
}

void XFStateManager::InvalidateXFRange(int start, int end)
{
  if (((u32)start >= (u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4 + 12) ||
      ((u32)start >=
           XFMEM_NORMALMATRICES + ((u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31) * 3 &&
       (u32)start < XFMEM_NORMALMATRICES +
                        ((u32)g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31) * 3 + 9))
  {
    m_pos_normal_matrix_changed = true;
  }

  if (((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex0MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex0MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex1MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex1MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex2MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex2MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_a.Tex3MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_a.Tex3MtxIdx * 4 + 12))
  {
    m_tex_matrices_changed[0] = true;
  }

  if (((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex4MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex4MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex5MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex5MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex6MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex6MtxIdx * 4 + 12) ||
      ((u32)start >= (u32)g_main_cp_state.matrix_index_b.Tex7MtxIdx * 4 &&
       (u32)start < (u32)g_main_cp_state.matrix_index_b.Tex7MtxIdx * 4 + 12))
  {
    m_tex_matrices_changed[1] = true;
  }

  if (start < XFMEM_POSMATRICES_END)
  {
    if (m_minmax_transform_matrices_changed[0] == -1)
    {
      m_minmax_transform_matrices_changed[0] = start;
      m_minmax_transform_matrices_changed[1] =
          end > XFMEM_POSMATRICES_END ? XFMEM_POSMATRICES_END : end;
    }
    else
    {
      if (m_minmax_transform_matrices_changed[0] > start)
        m_minmax_transform_matrices_changed[0] = start;

      if (m_minmax_transform_matrices_changed[1] < end)
        m_minmax_transform_matrices_changed[1] =
            end > XFMEM_POSMATRICES_END ? XFMEM_POSMATRICES_END : end;
    }
  }

  if (start < XFMEM_NORMALMATRICES_END && end > XFMEM_NORMALMATRICES)
  {
    int _start = start < XFMEM_NORMALMATRICES ? 0 : start - XFMEM_NORMALMATRICES;
    int _end = end < XFMEM_NORMALMATRICES_END ? end - XFMEM_NORMALMATRICES :
                                                XFMEM_NORMALMATRICES_END - XFMEM_NORMALMATRICES;

    if (m_minmax_normal_matrices_changed[0] == -1)
    {
      m_minmax_normal_matrices_changed[0] = _start;
      m_minmax_normal_matrices_changed[1] = _end;
    }
    else
    {
      if (m_minmax_normal_matrices_changed[0] > _start)
        m_minmax_normal_matrices_changed[0] = _start;

      if (m_minmax_normal_matrices_changed[1] < _end)
        m_minmax_normal_matrices_changed[1] = _end;
    }
  }

  if (start < XFMEM_POSTMATRICES_END && end > XFMEM_POSTMATRICES)
  {
    int _start = start < XFMEM_POSTMATRICES ? XFMEM_POSTMATRICES : start - XFMEM_POSTMATRICES;
    int _end = end < XFMEM_POSTMATRICES_END ? end - XFMEM_POSTMATRICES :
                                              XFMEM_POSTMATRICES_END - XFMEM_POSTMATRICES;

    if (m_minmax_post_transform_matrices_changed[0] == -1)
    {
      m_minmax_post_transform_matrices_changed[0] = _start;
      m_minmax_post_transform_matrices_changed[1] = _end;
    }
    else
    {
      if (m_minmax_post_transform_matrices_changed[0] > _start)
        m_minmax_post_transform_matrices_changed[0] = _start;

      if (m_minmax_post_transform_matrices_changed[1] < _end)
        m_minmax_post_transform_matrices_changed[1] = _end;
    }
  }

  if (start < XFMEM_LIGHTS_END && end > XFMEM_LIGHTS)
  {
    int _start = start < XFMEM_LIGHTS ? XFMEM_LIGHTS : start - XFMEM_LIGHTS;
    int _end = end < XFMEM_LIGHTS_END ? end - XFMEM_LIGHTS : XFMEM_LIGHTS_END - XFMEM_LIGHTS;

    if (m_minmax_lights_changed[0] == -1)
    {
      m_minmax_lights_changed[0] = _start;
      m_minmax_lights_changed[1] = _end;
    }
    else
    {
      if (m_minmax_lights_changed[0] > _start)
        m_minmax_lights_changed[0] = _start;

      if (m_minmax_lights_changed[1] < _end)
        m_minmax_lights_changed[1] = _end;
    }
  }
}

void XFStateManager::SetTexMatrixChangedA(u32 Value)
{
  if (g_main_cp_state.matrix_index_a.Hex != Value)
  {
    g_vertex_manager->Flush();
    if (g_main_cp_state.matrix_index_a.PosNormalMtxIdx != (Value & 0x3f))
      m_pos_normal_matrix_changed = true;
    m_tex_matrices_changed[0] = true;
    g_main_cp_state.matrix_index_a.Hex = Value;
  }
}

void XFStateManager::ResetTexMatrixAChange()
{
  m_tex_matrices_changed[0] = false;
}

void XFStateManager::SetTexMatrixChangedB(u32 Value)
{
  if (g_main_cp_state.matrix_index_b.Hex != Value)
  {
    g_vertex_manager->Flush();
    m_tex_matrices_changed[1] = true;
    g_main_cp_state.matrix_index_b.Hex = Value;
  }
}

void XFStateManager::ResetTexMatrixBChange()
{
  m_tex_matrices_changed[1] = false;
}

void XFStateManager::ResetPosNormalChange()
{
  m_pos_normal_matrix_changed = false;
}

void XFStateManager::SetProjectionChanged()
{
  m_projection_changed = true;
}

void XFStateManager::ResetProjection()
{
  m_projection_changed = false;
}

void XFStateManager::SetViewportChanged()
{
  m_viewport_changed = true;
}

void XFStateManager::ResetViewportChange()
{
  m_viewport_changed = false;
}

void XFStateManager::SetTexMatrixInfoChanged(int index)
{
  // TODO: Should we track this with more precision, like which indices changed?
  // The whole vertex constants are probably going to be uploaded regardless.
  m_tex_mtx_info_changed = true;
}

void XFStateManager::ResetTexMatrixInfoChange()
{
  m_tex_mtx_info_changed = false;
}

void XFStateManager::SetLightingConfigChanged()
{
  m_lighting_config_changed = true;
}

void XFStateManager::ResetLightingConfigChange()
{
  m_lighting_config_changed = false;
}

void XFStateManager::ResetLightsChanged()
{
  m_minmax_lights_changed.fill(-1);
}

void XFStateManager::SetMaterialColorChanged(int index)
{
  m_materials_changed[index] = true;
}

void XFStateManager::ResetMaterialChanges()
{
  m_materials_changed = BitSet32(0);
}

void XFStateManager::ResetPerVertexTransformMatrixChanges()
{
  m_minmax_transform_matrices_changed.fill(-1);
}

void XFStateManager::ResetPerVertexNormalMatrixChanges()
{
  m_minmax_normal_matrices_changed.fill(-1);
}

void XFStateManager::ResetPostTransformMatrixChanges()
{
  m_minmax_post_transform_matrices_changed.fill(-1);
}
