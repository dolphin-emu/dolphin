// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexShaderManager.h"

#include <array>
#include <cmath>
#include <cstring>
#include <iterator>

#include "Common/BitSet.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Logging/Log.h"
#include "Common/Matrix.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/FreeLookCamera.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionData.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

void VertexShaderManager::Init()
{
  // Initialize state tracking variables
  m_minmax_transform_matrices_changed.fill(-1);
  m_minmax_normal_matrices_changed.fill(-1);
  m_minmax_post_transform_matrices_changed.fill(-1);
  m_minmax_lights_changed.fill(-1);
  m_materials_changed = BitSet32(0);
  m_tex_matrices_changed.fill(false);
  m_pos_normal_matrix_changed = false;
  m_projection_changed = true;
  m_viewport_changed = false;
  m_tex_mtx_info_changed = false;
  m_lighting_config_changed = false;
  m_projection_graphics_mod_change = false;

  std::memset(static_cast<void*>(&xfmem), 0, sizeof(xfmem));
  constants = {};

  // TODO: should these go inside ResetView()?
  m_viewport_correction = Common::Matrix44::Identity();
  m_projection_matrix = Common::Matrix44::Identity().data;

  dirty = true;
}

void VertexShaderManager::Dirty()
{
  // This function is called after a savestate is loaded.
  // Any constants that can changed based on settings should be re-calculated
  m_projection_changed = true;

  dirty = true;
}

Common::Matrix44 VertexShaderManager::LoadProjectionMatrix()
{
  const auto& rawProjection = xfmem.projection.rawProjection;

  switch (xfmem.projection.type)
  {
  case ProjectionType::Perspective:
  {
    const Common::Vec2 fov_multiplier = g_freelook_camera.IsActive() ?
                                            g_freelook_camera.GetFieldOfViewMultiplier() :
                                            Common::Vec2{1, 1};
    m_projection_matrix[0] = rawProjection[0] * g_ActiveConfig.fAspectRatioHackW * fov_multiplier.x;
    m_projection_matrix[1] = 0.0f;
    m_projection_matrix[2] = rawProjection[1] * g_ActiveConfig.fAspectRatioHackW * fov_multiplier.x;
    m_projection_matrix[3] = 0.0f;

    m_projection_matrix[4] = 0.0f;
    m_projection_matrix[5] = rawProjection[2] * g_ActiveConfig.fAspectRatioHackH * fov_multiplier.y;
    m_projection_matrix[6] = rawProjection[3] * g_ActiveConfig.fAspectRatioHackH * fov_multiplier.y;
    m_projection_matrix[7] = 0.0f;

    m_projection_matrix[8] = 0.0f;
    m_projection_matrix[9] = 0.0f;
    m_projection_matrix[10] = rawProjection[4];
    m_projection_matrix[11] = rawProjection[5];

    m_projection_matrix[12] = 0.0f;
    m_projection_matrix[13] = 0.0f;

    m_projection_matrix[14] = -1.0f;
    m_projection_matrix[15] = 0.0f;

    g_stats.gproj = m_projection_matrix;
  }
  break;

  case ProjectionType::Orthographic:
  {
    m_projection_matrix[0] = rawProjection[0];
    m_projection_matrix[1] = 0.0f;
    m_projection_matrix[2] = 0.0f;
    m_projection_matrix[3] = rawProjection[1];

    m_projection_matrix[4] = 0.0f;
    m_projection_matrix[5] = rawProjection[2];
    m_projection_matrix[6] = 0.0f;
    m_projection_matrix[7] = rawProjection[3];

    m_projection_matrix[8] = 0.0f;
    m_projection_matrix[9] = 0.0f;
    m_projection_matrix[10] = rawProjection[4];
    m_projection_matrix[11] = rawProjection[5];

    m_projection_matrix[12] = 0.0f;
    m_projection_matrix[13] = 0.0f;

    m_projection_matrix[14] = 0.0f;
    m_projection_matrix[15] = 1.0f;

    g_stats.g2proj = m_projection_matrix;
    g_stats.proj = rawProjection;
  }
  break;

  default:
    ERROR_LOG_FMT(VIDEO, "Unknown projection type: {}", xfmem.projection.type);
  }

  PRIM_LOG("Projection: {} {} {} {} {} {}", rawProjection[0], rawProjection[1], rawProjection[2],
           rawProjection[3], rawProjection[4], rawProjection[5]);

  auto corrected_matrix = m_viewport_correction * Common::Matrix44::FromArray(m_projection_matrix);

  if (g_freelook_camera.IsActive() && xfmem.projection.type == ProjectionType::Perspective)
    corrected_matrix *= g_freelook_camera.GetView();

  g_freelook_camera.GetController()->SetClean();

  return corrected_matrix;
}

void VertexShaderManager::SetProjectionMatrix()
{
  if (m_projection_changed || g_freelook_camera.GetController()->IsDirty())
  {
    m_projection_changed = false;
    auto corrected_matrix = LoadProjectionMatrix();
    memcpy(constants.projection.data(), corrected_matrix.data.data(), 4 * sizeof(float4));
  }
}

bool VertexShaderManager::UseVertexDepthRange()
{
  // We can't compute the depth range in the vertex shader if we don't support depth clamp.
  if (!g_ActiveConfig.backend_info.bSupportsDepthClamp)
    return false;

  // We need a full depth range if a ztexture is used.
  if (bpmem.ztex2.op != ZTexOp::Disabled && !bpmem.zcontrol.early_ztest)
    return true;

  // If an inverted depth range is unsupported, we also need to check if the range is inverted.
  if (!g_ActiveConfig.backend_info.bSupportsReversedDepthRange && xfmem.viewport.zRange < 0.0f)
    return true;

  // If an oversized depth range or a ztexture is used, we need to calculate the depth range
  // in the vertex shader.
  return fabs(xfmem.viewport.zRange) > 16777215.0f || fabs(xfmem.viewport.farZ) > 16777215.0f;
}

// Syncs the shader constant buffers with xfmem
// TODO: A cleaner way to control the matrices without making a mess in the parameters field
void VertexShaderManager::SetConstants(const std::vector<std::string>& textures)
{
  if (constants.missing_color_hex != g_ActiveConfig.iMissingColorValue)
  {
    const float a = (g_ActiveConfig.iMissingColorValue) & 0xFF;
    const float b = (g_ActiveConfig.iMissingColorValue >> 8) & 0xFF;
    const float g = (g_ActiveConfig.iMissingColorValue >> 16) & 0xFF;
    const float r = (g_ActiveConfig.iMissingColorValue >> 24) & 0xFF;
    constants.missing_color_hex = g_ActiveConfig.iMissingColorValue;
    constants.missing_color_value = {r / 255, g / 255, b / 255, a / 255};

    dirty = true;
  }

  if (m_minmax_transform_matrices_changed[0] >= 0)
  {
    int startn = m_minmax_transform_matrices_changed[0] / 4;
    int endn = (m_minmax_transform_matrices_changed[1] + 3) / 4;
    memcpy(constants.transformmatrices[startn].data(), &xfmem.posMatrices[startn * 4],
           (endn - startn) * sizeof(float4));
    dirty = true;
    m_minmax_transform_matrices_changed[0] = m_minmax_transform_matrices_changed[1] = -1;
  }

  if (m_minmax_normal_matrices_changed[0] >= 0)
  {
    int startn = m_minmax_normal_matrices_changed[0] / 3;
    int endn = (m_minmax_normal_matrices_changed[1] + 2) / 3;
    for (int i = startn; i < endn; i++)
    {
      memcpy(constants.normalmatrices[i].data(), &xfmem.normalMatrices[3 * i], 12);
    }
    dirty = true;
    m_minmax_normal_matrices_changed[0] = m_minmax_normal_matrices_changed[1] = -1;
  }

  if (m_minmax_post_transform_matrices_changed[0] >= 0)
  {
    int startn = m_minmax_post_transform_matrices_changed[0] / 4;
    int endn = (m_minmax_post_transform_matrices_changed[1] + 3) / 4;
    memcpy(constants.posttransformmatrices[startn].data(), &xfmem.postMatrices[startn * 4],
           (endn - startn) * sizeof(float4));
    dirty = true;
    m_minmax_post_transform_matrices_changed[0] = m_minmax_post_transform_matrices_changed[1] = -1;
  }

  if (m_minmax_lights_changed[0] >= 0)
  {
    // TODO: Outdated comment
    // lights don't have a 1 to 1 mapping, the color component needs to be converted to 4 floats
    int istart = m_minmax_lights_changed[0] / 0x10;
    int iend = (m_minmax_lights_changed[1] + 15) / 0x10;

    for (int i = istart; i < iend; ++i)
    {
      const Light& light = xfmem.lights[i];
      VertexShaderConstants::Light& dstlight = constants.lights[i];

      // xfmem.light.color is packed as abgr in u8[4], so we have to swap the order
      dstlight.color[0] = light.color[3];
      dstlight.color[1] = light.color[2];
      dstlight.color[2] = light.color[1];
      dstlight.color[3] = light.color[0];

      dstlight.cosatt[0] = light.cosatt[0];
      dstlight.cosatt[1] = light.cosatt[1];
      dstlight.cosatt[2] = light.cosatt[2];

      if (fabs(light.distatt[0]) < 0.00001f && fabs(light.distatt[1]) < 0.00001f &&
          fabs(light.distatt[2]) < 0.00001f)
      {
        // dist attenuation, make sure not equal to 0!!!
        dstlight.distatt[0] = .00001f;
      }
      else
      {
        dstlight.distatt[0] = light.distatt[0];
      }
      dstlight.distatt[1] = light.distatt[1];
      dstlight.distatt[2] = light.distatt[2];

      dstlight.pos[0] = light.dpos[0];
      dstlight.pos[1] = light.dpos[1];
      dstlight.pos[2] = light.dpos[2];

      // TODO: Hardware testing is needed to confirm that this normalization is correct
      auto sanitize = [](float f) {
        if (std::isnan(f))
          return 0.0f;
        else if (std::isinf(f))
          return f > 0.0f ? 1.0f : -1.0f;
        else
          return f;
      };
      double norm = double(light.ddir[0]) * double(light.ddir[0]) +
                    double(light.ddir[1]) * double(light.ddir[1]) +
                    double(light.ddir[2]) * double(light.ddir[2]);
      norm = 1.0 / sqrt(norm);
      dstlight.dir[0] = sanitize(static_cast<float>(light.ddir[0] * norm));
      dstlight.dir[1] = sanitize(static_cast<float>(light.ddir[1] * norm));
      dstlight.dir[2] = sanitize(static_cast<float>(light.ddir[2] * norm));
    }
    dirty = true;

    m_minmax_lights_changed[0] = m_minmax_lights_changed[1] = -1;
  }

  for (int i : m_materials_changed)
  {
    u32 data = i >= 2 ? xfmem.matColor[i - 2] : xfmem.ambColor[i];
    constants.materials[i][0] = (data >> 24) & 0xFF;
    constants.materials[i][1] = (data >> 16) & 0xFF;
    constants.materials[i][2] = (data >> 8) & 0xFF;
    constants.materials[i][3] = data & 0xFF;
    dirty = true;
  }
  m_materials_changed = BitSet32(0);

  if (m_pos_normal_matrix_changed)
  {
    m_pos_normal_matrix_changed = false;

    const float* pos = &xfmem.posMatrices[g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4];
    const float* norm =
        &xfmem.normalMatrices[3 * (g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31)];

    memcpy(constants.posnormalmatrix.data(), pos, 3 * sizeof(float4));
    memcpy(constants.posnormalmatrix[3].data(), norm, 3 * sizeof(float));
    memcpy(constants.posnormalmatrix[4].data(), norm + 3, 3 * sizeof(float));
    memcpy(constants.posnormalmatrix[5].data(), norm + 6, 3 * sizeof(float));
    dirty = true;
  }

  if (m_tex_matrices_changed[0])
  {
    m_tex_matrices_changed[0] = false;
    const std::array<const float*, 4> pos_matrix_ptrs{
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex0MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex1MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex2MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex3MtxIdx * 4],
    };

    for (size_t i = 0; i < pos_matrix_ptrs.size(); ++i)
    {
      memcpy(constants.texmatrices[3 * i].data(), pos_matrix_ptrs[i], 3 * sizeof(float4));
    }
    dirty = true;
  }

  if (m_tex_matrices_changed[1])
  {
    m_tex_matrices_changed[1] = false;
    const std::array<const float*, 4> pos_matrix_ptrs{
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex4MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex5MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex6MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex7MtxIdx * 4],
    };

    for (size_t i = 0; i < pos_matrix_ptrs.size(); ++i)
    {
      memcpy(constants.texmatrices[3 * i + 12].data(), pos_matrix_ptrs[i], 3 * sizeof(float4));
    }
    dirty = true;
  }

  if (m_viewport_changed)
  {
    m_viewport_changed = false;

    // The console GPU places the pixel center at 7/12 unless antialiasing
    // is enabled, while D3D and OpenGL place it at 0.5. See the comment
    // in VertexShaderGen.cpp for details.
    // NOTE: If we ever emulate antialiasing, the sample locations set by
    // BP registers 0x01-0x04 need to be considered here.
    const float pixel_center_correction = 7.0f / 12.0f - 0.5f;
    const bool bUseVertexRounding = g_ActiveConfig.UseVertexRounding();
    const float viewport_width = bUseVertexRounding ?
                                     (2.f * xfmem.viewport.wd) :
                                     g_framebuffer_manager->EFBToScaledXf(2.f * xfmem.viewport.wd);
    const float viewport_height = bUseVertexRounding ?
                                      (2.f * xfmem.viewport.ht) :
                                      g_framebuffer_manager->EFBToScaledXf(2.f * xfmem.viewport.ht);
    const float pixel_size_x = 2.f / viewport_width;
    const float pixel_size_y = 2.f / viewport_height;
    constants.pixelcentercorrection[0] = pixel_center_correction * pixel_size_x;
    constants.pixelcentercorrection[1] = pixel_center_correction * pixel_size_y;

    // By default we don't change the depth value at all in the vertex shader.
    constants.pixelcentercorrection[2] = 1.0f;
    constants.pixelcentercorrection[3] = 0.0f;

    constants.viewport[0] = (2.f * xfmem.viewport.wd);
    constants.viewport[1] = (2.f * xfmem.viewport.ht);

    if (UseVertexDepthRange())
    {
      // Oversized depth ranges are handled in the vertex shader. We need to reverse
      // the far value to use the reversed-Z trick.
      if (g_ActiveConfig.backend_info.bSupportsReversedDepthRange)
      {
        // Sometimes the console also tries to use the reversed-Z trick. We can only do
        // that with the expected accuracy if the backend can reverse the depth range.
        constants.pixelcentercorrection[2] = fabs(xfmem.viewport.zRange) / 16777215.0f;
        if (xfmem.viewport.zRange < 0.0f)
          constants.pixelcentercorrection[3] = xfmem.viewport.farZ / 16777215.0f;
        else
          constants.pixelcentercorrection[3] = 1.0f - xfmem.viewport.farZ / 16777215.0f;
      }
      else
      {
        // For backends that don't support reversing the depth range we can still render
        // cases where the console uses the reversed-Z trick. But we simply can't provide
        // the expected accuracy, which might result in z-fighting.
        constants.pixelcentercorrection[2] = xfmem.viewport.zRange / 16777215.0f;
        constants.pixelcentercorrection[3] = 1.0f - xfmem.viewport.farZ / 16777215.0f;
      }
    }

    dirty = true;
    BPFunctions::SetScissorAndViewport();
    g_stats.AddScissorRect();
  }

  std::vector<GraphicsModAction*> projection_actions;
  if (g_ActiveConfig.bGraphicMods)
  {
    for (const auto action : g_graphics_mod_manager->GetProjectionActions(xfmem.projection.type))
    {
      projection_actions.push_back(action);
    }

    for (const auto& texture : textures)
    {
      for (const auto action :
           g_graphics_mod_manager->GetProjectionTextureActions(xfmem.projection.type, texture))
      {
        projection_actions.push_back(action);
      }
    }
  }

  if (m_projection_changed || g_freelook_camera.GetController()->IsDirty() ||
      !projection_actions.empty() || m_projection_graphics_mod_change)
  {
    m_projection_changed = false;
    m_projection_graphics_mod_change = !projection_actions.empty();

    auto corrected_matrix = LoadProjectionMatrix();

    GraphicsModActionData::Projection projection{&corrected_matrix};
    for (auto action : projection_actions)
    {
      action->OnProjection(&projection);
    }

    memcpy(constants.projection.data(), corrected_matrix.data.data(), 4 * sizeof(float4));

    dirty = true;
  }

  if (m_tex_mtx_info_changed)
  {
    m_tex_mtx_info_changed = false;
    constants.xfmem_dualTexInfo = xfmem.dualTexTrans.enabled;
    for (size_t i = 0; i < std::size(xfmem.texMtxInfo); i++)
      constants.xfmem_pack1[i][0] = xfmem.texMtxInfo[i].hex;
    for (size_t i = 0; i < std::size(xfmem.postMtxInfo); i++)
      constants.xfmem_pack1[i][1] = xfmem.postMtxInfo[i].hex;

    dirty = true;
  }

  if (m_lighting_config_changed)
  {
    m_lighting_config_changed = false;

    for (size_t i = 0; i < 2; i++)
    {
      constants.xfmem_pack1[i][2] = xfmem.color[i].hex;
      constants.xfmem_pack1[i][3] = xfmem.alpha[i].hex;
    }
    constants.xfmem_numColorChans = xfmem.numChan.numColorChans;
    dirty = true;
  }
}

void VertexShaderManager::InvalidateXFRange(int start, int end)
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

void VertexShaderManager::SetTexMatrixChangedA(u32 Value)
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

void VertexShaderManager::SetTexMatrixChangedB(u32 Value)
{
  if (g_main_cp_state.matrix_index_b.Hex != Value)
  {
    g_vertex_manager->Flush();
    m_tex_matrices_changed[1] = true;
    g_main_cp_state.matrix_index_b.Hex = Value;
  }
}

void VertexShaderManager::SetViewportChanged()
{
  m_viewport_changed = true;
}

void VertexShaderManager::SetProjectionChanged()
{
  m_projection_changed = true;
}

void VertexShaderManager::SetMaterialColorChanged(int index)
{
  m_materials_changed[index] = true;
}

void VertexShaderManager::SetTexMatrixInfoChanged(int index)
{
  // TODO: Should we track this with more precision, like which indices changed?
  // The whole vertex constants are probably going to be uploaded regardless.
  m_tex_mtx_info_changed = true;
}

void VertexShaderManager::SetLightingConfigChanged()
{
  m_lighting_config_changed = true;
}

void VertexShaderManager::TransformToClipSpace(const float* data, float* out, u32 MtxIdx)
{
  const float* world_matrix = &xfmem.posMatrices[(MtxIdx & 0x3f) * 4];

  // We use the projection matrix calculated by VertexShaderManager, because it
  // includes any free look transformations.
  // Make sure VertexShaderManager::SetConstants() has been called first.
  const float* proj_matrix = &m_projection_matrix[0];

  const float t[3] = {data[0] * world_matrix[0] + data[1] * world_matrix[1] +
                          data[2] * world_matrix[2] + world_matrix[3],
                      data[0] * world_matrix[4] + data[1] * world_matrix[5] +
                          data[2] * world_matrix[6] + world_matrix[7],
                      data[0] * world_matrix[8] + data[1] * world_matrix[9] +
                          data[2] * world_matrix[10] + world_matrix[11]};

  out[0] = t[0] * proj_matrix[0] + t[1] * proj_matrix[1] + t[2] * proj_matrix[2] + proj_matrix[3];
  out[1] = t[0] * proj_matrix[4] + t[1] * proj_matrix[5] + t[2] * proj_matrix[6] + proj_matrix[7];
  out[2] = t[0] * proj_matrix[8] + t[1] * proj_matrix[9] + t[2] * proj_matrix[10] + proj_matrix[11];
  out[3] =
      t[0] * proj_matrix[12] + t[1] * proj_matrix[13] + t[2] * proj_matrix[14] + proj_matrix[15];
}

void VertexShaderManager::DoState(PointerWrap& p)
{
  p.DoArray(m_projection_matrix);
  p.Do(m_viewport_correction);
  g_freelook_camera.DoState(p);

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

  p.Do(constants);

  if (p.IsReadMode())
  {
    Dirty();
  }
}
