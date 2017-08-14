// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cfloat>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>

#include "Common/BitSet.h"
#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MathUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

alignas(16) static MathUtil::Matrix4f g_fProjectionMatrix;

// track changes
static bool bTexMatricesChanged[2], bPosNormalMatrixChanged, bProjectionChanged, bViewportChanged;
static bool bTexMtxInfoChanged, bLightingConfigChanged;
static BitSet32 nMaterialsChanged;
static int nTransformMatricesChanged[2];      // min,max
static int nNormalMatricesChanged[2];         // min,max
static int nPostTransformMatricesChanged[2];  // min,max
static int nLightsChanged[2];                 // min,max

static MathUtil::Matrix4f s_viewportCorrection;
static MathUtil::Matrix3f s_viewRotationMatrix;
static MathUtil::Matrix3f s_viewInvRotationMatrix;
static MathUtil::Vector3f s_fViewTranslationVector;
static MathUtil::Vector2f s_fViewRotation;

VertexShaderConstants VertexShaderManager::constants;
bool VertexShaderManager::dirty;

struct ProjectionHack
{
  float sign;
  float value;
  ProjectionHack() {}
  ProjectionHack(float new_sign, float new_value) : sign(new_sign), value(new_value) {}
};

namespace
{
// Control Variables
static ProjectionHack g_proj_hack_near;
static ProjectionHack g_proj_hack_far;
}  // Namespace

static float PHackValue(std::string sValue)
{
  float f = 0;
  bool fp = false;
  const char* cStr = sValue.c_str();
  char* c = new char[strlen(cStr) + 1];
  std::istringstream sTof("");

  for (unsigned int i = 0; i <= strlen(cStr); ++i)
  {
    if (i == 20)
    {
      c[i] = '\0';
      break;
    }

    c[i] = (cStr[i] == ',') ? '.' : *(cStr + i);
    if (c[i] == '.')
      fp = true;
  }

  cStr = c;
  sTof.str(cStr);
  sTof >> f;

  if (!fp)
    f /= 0xF4240;

  delete[] c;
  return f;
}

void UpdateProjectionHack(const ProjectionHackConfig& config)
{
  float near_value = 0, far_value = 0;
  float near_sign = 1.0, far_sign = 1.0;

  if (config.m_enable)
  {
    const char* near_sign_str = "";
    const char* far_sign_str = "";

    NOTICE_LOG(VIDEO, "\t\t--- Orthographic Projection Hack ON ---");

    if (config.m_sznear)
    {
      near_sign *= -1.0f;
      near_sign_str = " * (-1)";
    }
    if (config.m_szfar)
    {
      far_sign *= -1.0f;
      far_sign_str = " * (-1)";
    }

    near_value = PHackValue(config.m_znear);
    NOTICE_LOG(VIDEO, "- zNear Correction = (%f + zNear)%s", near_value, near_sign_str);

    far_value = PHackValue(config.m_zfar);
    NOTICE_LOG(VIDEO, "- zFar Correction =  (%f + zFar)%s", far_value, far_sign_str);
  }

  // Set the projections hacks
  g_proj_hack_near = ProjectionHack(near_sign, near_value);
  g_proj_hack_far = ProjectionHack(far_sign, far_value);
}

// Viewport correction:
// In D3D, the viewport rectangle must fit within the render target.
// Say you want a viewport at (ix, iy) with size (iw, ih),
// but your viewport must be clamped at (ax, ay) with size (aw, ah).
// Just multiply the projection matrix with the following to get the same
// effect:
// [   (iw/aw)         0     0    ((iw - 2*(ax-ix)) / aw - 1)   ]
// [         0   (ih/ah)     0   ((-ih + 2*(ay-iy)) / ah + 1)   ]
// [         0         0     1                              0   ]
// [         0         0     0                              1   ]
static void ViewportCorrectionMatrix(MathUtil::Matrix4f& result)
{
  int scissorXOff = bpmem.scissorOffset.x * 2;
  int scissorYOff = bpmem.scissorOffset.y * 2;

  // TODO: ceil, floor or just cast to int?
  // TODO: Directly use the floats instead of rounding them?
  float intendedX = xfmem.viewport.xOrig - xfmem.viewport.wd - scissorXOff;
  float intendedY = xfmem.viewport.yOrig + xfmem.viewport.ht - scissorYOff;
  float intendedWd = 2.0f * xfmem.viewport.wd;
  float intendedHt = -2.0f * xfmem.viewport.ht;

  if (intendedWd < 0.f)
  {
    intendedX += intendedWd;
    intendedWd = -intendedWd;
  }
  if (intendedHt < 0.f)
  {
    intendedY += intendedHt;
    intendedHt = -intendedHt;
  }

  // fit to EFB size
  float X = (intendedX >= 0.f) ? intendedX : 0.f;
  float Y = (intendedY >= 0.f) ? intendedY : 0.f;
  float Wd = (X + intendedWd <= EFB_WIDTH) ? intendedWd : (EFB_WIDTH - X);
  float Ht = (Y + intendedHt <= EFB_HEIGHT) ? intendedHt : (EFB_HEIGHT - Y);

  result.setIdentity();
  if (Wd == 0 || Ht == 0)
    return;

  result(0, 0) = intendedWd / Wd;
  result(0, 3) = (intendedWd - 2.f * (X - intendedX)) / Wd - 1.f;
  result(1, 1) = intendedHt / Ht;
  result(1, 3) = (-intendedHt + 2.f * (Y - intendedY)) / Ht + 1.f;
}

void VertexShaderManager::Init()
{
  // Initialize state tracking variables
  nTransformMatricesChanged[0] = -1;
  nTransformMatricesChanged[1] = -1;
  nNormalMatricesChanged[0] = -1;
  nNormalMatricesChanged[1] = -1;
  nPostTransformMatricesChanged[0] = -1;
  nPostTransformMatricesChanged[1] = -1;
  nLightsChanged[0] = -1;
  nLightsChanged[1] = -1;
  nMaterialsChanged = BitSet32(0);
  bTexMatricesChanged[0] = false;
  bTexMatricesChanged[1] = false;
  bPosNormalMatrixChanged = false;
  bProjectionChanged = true;
  bViewportChanged = false;
  bTexMtxInfoChanged = false;
  bLightingConfigChanged = false;

  std::memset(&xfmem, 0, sizeof(xfmem));
  constants = {};
  ResetView();

  // TODO: should these go inside ResetView()?
  s_viewportCorrection.setIdentity();
  g_fProjectionMatrix.setIdentity();

  dirty = true;
}

void VertexShaderManager::Dirty()
{
  // This function is called after a savestate is loaded.
  // Any constants that can changed based on settings should be re-calculated
  bProjectionChanged = true;

  dirty = true;
}

// Syncs the shader constant buffers with xfmem
// TODO: A cleaner way to control the matrices without making a mess in the parameters field
void VertexShaderManager::SetConstants()
{
  if (nTransformMatricesChanged[0] >= 0)
  {
    int startn = nTransformMatricesChanged[0] / 4;
    int endn = (nTransformMatricesChanged[1] + 3) / 4;
    memcpy(constants.transformmatrices[startn].data(), &xfmem.posMatrices[startn * 4],
           (endn - startn) * sizeof(float4));
    dirty = true;
    nTransformMatricesChanged[0] = nTransformMatricesChanged[1] = -1;
  }

  if (nNormalMatricesChanged[0] >= 0)
  {
    int startn = nNormalMatricesChanged[0] / 3;
    int endn = (nNormalMatricesChanged[1] + 2) / 3;
    for (int i = startn; i < endn; i++)
    {
      memcpy(constants.normalmatrices[i].data(), &xfmem.normalMatrices[3 * i], 12);
    }
    dirty = true;
    nNormalMatricesChanged[0] = nNormalMatricesChanged[1] = -1;
  }

  if (nPostTransformMatricesChanged[0] >= 0)
  {
    int startn = nPostTransformMatricesChanged[0] / 4;
    int endn = (nPostTransformMatricesChanged[1] + 3) / 4;
    memcpy(constants.posttransformmatrices[startn].data(), &xfmem.postMatrices[startn * 4],
           (endn - startn) * sizeof(float4));
    dirty = true;
    nPostTransformMatricesChanged[0] = nPostTransformMatricesChanged[1] = -1;
  }

  if (nLightsChanged[0] >= 0)
  {
    // TODO: Outdated comment
    // lights don't have a 1 to 1 mapping, the color component needs to be converted to 4 floats
    int istart = nLightsChanged[0] / 0x10;
    int iend = (nLightsChanged[1] + 15) / 0x10;

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

      double norm = double(light.ddir[0]) * double(light.ddir[0]) +
                    double(light.ddir[1]) * double(light.ddir[1]) +
                    double(light.ddir[2]) * double(light.ddir[2]);
      norm = 1.0 / sqrt(norm);
      float norm_float = static_cast<float>(norm);
      dstlight.dir[0] = light.ddir[0] * norm_float;
      dstlight.dir[1] = light.ddir[1] * norm_float;
      dstlight.dir[2] = light.ddir[2] * norm_float;
    }
    dirty = true;

    nLightsChanged[0] = nLightsChanged[1] = -1;
  }

  for (int i : nMaterialsChanged)
  {
    u32 data = i >= 2 ? xfmem.matColor[i - 2] : xfmem.ambColor[i];
    constants.materials[i][0] = (data >> 24) & 0xFF;
    constants.materials[i][1] = (data >> 16) & 0xFF;
    constants.materials[i][2] = (data >> 8) & 0xFF;
    constants.materials[i][3] = data & 0xFF;
    dirty = true;
  }
  nMaterialsChanged = BitSet32(0);

  if (bPosNormalMatrixChanged)
  {
    bPosNormalMatrixChanged = false;

    const float* pos = &xfmem.posMatrices[g_main_cp_state.matrix_index_a.PosNormalMtxIdx * 4];
    const float* norm =
        &xfmem.normalMatrices[3 * (g_main_cp_state.matrix_index_a.PosNormalMtxIdx & 31)];

    memcpy(constants.posnormalmatrix.data(), pos, 3 * sizeof(float4));
    memcpy(constants.posnormalmatrix[3].data(), norm, 3 * sizeof(float));
    memcpy(constants.posnormalmatrix[4].data(), norm + 3, 3 * sizeof(float));
    memcpy(constants.posnormalmatrix[5].data(), norm + 6, 3 * sizeof(float));
    dirty = true;
  }

  if (bTexMatricesChanged[0])
  {
    bTexMatricesChanged[0] = false;
    const float* pos_matrix_ptrs[] = {
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex0MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex1MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex2MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_a.Tex3MtxIdx * 4]};

    for (size_t i = 0; i < ArraySize(pos_matrix_ptrs); ++i)
    {
      memcpy(constants.texmatrices[3 * i].data(), pos_matrix_ptrs[i], 3 * sizeof(float4));
    }
    dirty = true;
  }

  if (bTexMatricesChanged[1])
  {
    bTexMatricesChanged[1] = false;
    const float* pos_matrix_ptrs[] = {
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex4MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex5MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex6MtxIdx * 4],
        &xfmem.posMatrices[g_main_cp_state.matrix_index_b.Tex7MtxIdx * 4]};

    for (size_t i = 0; i < ArraySize(pos_matrix_ptrs); ++i)
    {
      memcpy(constants.texmatrices[3 * i + 12].data(), pos_matrix_ptrs[i], 3 * sizeof(float4));
    }
    dirty = true;
  }

  if (bViewportChanged)
  {
    bViewportChanged = false;

    // The console GPU places the pixel center at 7/12 unless antialiasing
    // is enabled, while D3D and OpenGL place it at 0.5. See the comment
    // in VertexShaderGen.cpp for details.
    // NOTE: If we ever emulate antialiasing, the sample locations set by
    // BP registers 0x01-0x04 need to be considered here.
    const float pixel_center_correction = 7.0f / 12.0f - 0.5f;
    const bool bUseVertexRounding = g_ActiveConfig.bVertexRounding && g_ActiveConfig.iEFBScale != 1;
    const float viewport_width = bUseVertexRounding ?
                                     (2.f * xfmem.viewport.wd) :
                                     g_renderer->EFBToScaledXf(2.f * xfmem.viewport.wd);
    const float viewport_height = bUseVertexRounding ?
                                      (2.f * xfmem.viewport.ht) :
                                      g_renderer->EFBToScaledXf(2.f * xfmem.viewport.ht);
    const float pixel_size_x = 2.f / viewport_width;
    const float pixel_size_y = 2.f / viewport_height;
    constants.pixelcentercorrection[0] = pixel_center_correction * pixel_size_x;
    constants.pixelcentercorrection[1] = pixel_center_correction * pixel_size_y;

    // By default we don't change the depth value at all in the vertex shader.
    constants.pixelcentercorrection[2] = 1.0f;
    constants.pixelcentercorrection[3] = 0.0f;

    constants.viewport[0] = (2.f * xfmem.viewport.wd);
    constants.viewport[1] = (2.f * xfmem.viewport.ht);

    if (g_renderer->UseVertexDepthRange())
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
    // This is so implementation-dependent that we can't have it here.
    g_renderer->SetViewport();

    // Update projection if the viewport isn't 1:1 useable
    if (!g_ActiveConfig.backend_info.bSupportsOversizedViewports)
    {
      ViewportCorrectionMatrix(s_viewportCorrection);
      bProjectionChanged = true;
    }
  }

  if (bProjectionChanged)
  {
    bProjectionChanged = false;

    const float* rawProjection = xfmem.projection.rawProjection;

    switch (xfmem.projection.type)
    {
    case GX_PERSPECTIVE:
      g_fProjectionMatrix(0, 0) = rawProjection[0] * g_ActiveConfig.fAspectRatioHackW;
      g_fProjectionMatrix(0, 1) = 0.0f;
      g_fProjectionMatrix(0, 2) = rawProjection[1] * g_ActiveConfig.fAspectRatioHackW;
      g_fProjectionMatrix(0, 3) = 0.0f;

      g_fProjectionMatrix(1, 0) = 0.0f;
      g_fProjectionMatrix(1, 1) = rawProjection[2] * g_ActiveConfig.fAspectRatioHackH;
      g_fProjectionMatrix(1, 2) = rawProjection[3] * g_ActiveConfig.fAspectRatioHackH;
      g_fProjectionMatrix(1, 3) = 0.0f;

      g_fProjectionMatrix.row(2) << 0.0f, 0.0f, rawProjection[4], rawProjection[5];
      g_fProjectionMatrix.row(3) << 0.0f, 0.0f, -1.0f, 0.0f;

      SETSTAT_FT(stats.gproj_0, g_fProjectionMatrix(0, 0));
      SETSTAT_FT(stats.gproj_1, g_fProjectionMatrix(0, 1));
      SETSTAT_FT(stats.gproj_2, g_fProjectionMatrix(0, 2));
      SETSTAT_FT(stats.gproj_3, g_fProjectionMatrix(0, 3));
      SETSTAT_FT(stats.gproj_4, g_fProjectionMatrix(1, 0));
      SETSTAT_FT(stats.gproj_5, g_fProjectionMatrix(1, 1));
      SETSTAT_FT(stats.gproj_6, g_fProjectionMatrix(1, 2));
      SETSTAT_FT(stats.gproj_7, g_fProjectionMatrix(1, 3));
      SETSTAT_FT(stats.gproj_8, g_fProjectionMatrix(2, 0));
      SETSTAT_FT(stats.gproj_9, g_fProjectionMatrix(2, 1));
      SETSTAT_FT(stats.gproj_10, g_fProjectionMatrix(2, 2));
      SETSTAT_FT(stats.gproj_11, g_fProjectionMatrix(2, 3));
      SETSTAT_FT(stats.gproj_12, g_fProjectionMatrix(3, 0));
      SETSTAT_FT(stats.gproj_13, g_fProjectionMatrix(3, 1));
      SETSTAT_FT(stats.gproj_14, g_fProjectionMatrix(3, 2));
      SETSTAT_FT(stats.gproj_15, g_fProjectionMatrix(3, 3));
      break;

    case GX_ORTHOGRAPHIC:
      g_fProjectionMatrix.row(0) << rawProjection[0], 0.0f, 0.0f, rawProjection[1];
      g_fProjectionMatrix.row(1) << 0.0f, rawProjection[2], 0.0f, rawProjection[3];

      g_fProjectionMatrix(2, 0) = 0.0f;
      g_fProjectionMatrix(2, 1) = 0.0f;
      g_fProjectionMatrix(2, 2) = (g_proj_hack_near.value + rawProjection[4]) *
                                  ((g_proj_hack_near.sign == 0) ? 1.0f : g_proj_hack_near.sign);
      g_fProjectionMatrix(2, 3) = (g_proj_hack_far.value + rawProjection[5]) *
                                  ((g_proj_hack_far.sign == 0) ? 1.0f : g_proj_hack_far.sign);

      g_fProjectionMatrix.row(3) << 0.0f, 0.0f, 0.0f, 1.0f;

      SETSTAT_FT(stats.g2proj_0, g_fProjectionMatrix(0, 0));
      SETSTAT_FT(stats.g2proj_1, g_fProjectionMatrix(0, 1));
      SETSTAT_FT(stats.g2proj_2, g_fProjectionMatrix(0, 2));
      SETSTAT_FT(stats.g2proj_3, g_fProjectionMatrix(0, 3));
      SETSTAT_FT(stats.g2proj_4, g_fProjectionMatrix(1, 0));
      SETSTAT_FT(stats.g2proj_5, g_fProjectionMatrix(1, 1));
      SETSTAT_FT(stats.g2proj_6, g_fProjectionMatrix(1, 2));
      SETSTAT_FT(stats.g2proj_7, g_fProjectionMatrix(1, 3));
      SETSTAT_FT(stats.g2proj_8, g_fProjectionMatrix(2, 0));
      SETSTAT_FT(stats.g2proj_9, g_fProjectionMatrix(2, 1));
      SETSTAT_FT(stats.g2proj_10, g_fProjectionMatrix(2, 2));
      SETSTAT_FT(stats.g2proj_11, g_fProjectionMatrix(2, 3));
      SETSTAT_FT(stats.g2proj_12, g_fProjectionMatrix(3, 0));
      SETSTAT_FT(stats.g2proj_13, g_fProjectionMatrix(3, 1));
      SETSTAT_FT(stats.g2proj_14, g_fProjectionMatrix(3, 2));
      SETSTAT_FT(stats.g2proj_15, g_fProjectionMatrix(3, 3));
      break;

    default:
      ERROR_LOG(VIDEO, "Unknown projection type: %d", xfmem.projection.type);
      break;
    }

    SETSTAT_FT(stats.proj_0, rawProjection[0]);
    SETSTAT_FT(stats.proj_1, rawProjection[1]);
    SETSTAT_FT(stats.proj_2, rawProjection[2]);
    SETSTAT_FT(stats.proj_3, rawProjection[3]);
    SETSTAT_FT(stats.proj_4, rawProjection[4]);
    SETSTAT_FT(stats.proj_5, rawProjection[5]);

    PRIM_LOG("Projection: %f %f %f %f %f %f", rawProjection[0], rawProjection[1], rawProjection[2],
             rawProjection[3], rawProjection[4], rawProjection[5]);

    constants.projection = s_viewportCorrection * g_fProjectionMatrix;
    if (g_ActiveConfig.bFreeLook && xfmem.projection.type == GX_PERSPECTIVE)
    {
      const Eigen::Projective3f rotation(s_viewRotationMatrix);
      const Eigen::Translation3f translation(s_fViewTranslationVector);

      constants.projection *= Eigen::Projective3f(rotation * translation).matrix();
    }

    dirty = true;
  }

  if (bTexMtxInfoChanged)
  {
    bTexMtxInfoChanged = false;
    constants.xfmem_dualTexInfo = xfmem.dualTexTrans.enabled;
    for (size_t i = 0; i < ArraySize(xfmem.texMtxInfo); i++)
      constants.xfmem_pack1[i][0] = xfmem.texMtxInfo[i].hex;
    for (size_t i = 0; i < ArraySize(xfmem.postMtxInfo); i++)
      constants.xfmem_pack1[i][1] = xfmem.postMtxInfo[i].hex;

    dirty = true;
  }

  if (bLightingConfigChanged)
  {
    bLightingConfigChanged = false;

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
    bPosNormalMatrixChanged = true;
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
    bTexMatricesChanged[0] = true;
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
    bTexMatricesChanged[1] = true;
  }

  if (start < XFMEM_POSMATRICES_END)
  {
    if (nTransformMatricesChanged[0] == -1)
    {
      nTransformMatricesChanged[0] = start;
      nTransformMatricesChanged[1] = end > XFMEM_POSMATRICES_END ? XFMEM_POSMATRICES_END : end;
    }
    else
    {
      if (nTransformMatricesChanged[0] > start)
        nTransformMatricesChanged[0] = start;

      if (nTransformMatricesChanged[1] < end)
        nTransformMatricesChanged[1] = end > XFMEM_POSMATRICES_END ? XFMEM_POSMATRICES_END : end;
    }
  }

  if (start < XFMEM_NORMALMATRICES_END && end > XFMEM_NORMALMATRICES)
  {
    int _start = start < XFMEM_NORMALMATRICES ? 0 : start - XFMEM_NORMALMATRICES;
    int _end = end < XFMEM_NORMALMATRICES_END ? end - XFMEM_NORMALMATRICES :
                                                XFMEM_NORMALMATRICES_END - XFMEM_NORMALMATRICES;

    if (nNormalMatricesChanged[0] == -1)
    {
      nNormalMatricesChanged[0] = _start;
      nNormalMatricesChanged[1] = _end;
    }
    else
    {
      if (nNormalMatricesChanged[0] > _start)
        nNormalMatricesChanged[0] = _start;

      if (nNormalMatricesChanged[1] < _end)
        nNormalMatricesChanged[1] = _end;
    }
  }

  if (start < XFMEM_POSTMATRICES_END && end > XFMEM_POSTMATRICES)
  {
    int _start = start < XFMEM_POSTMATRICES ? XFMEM_POSTMATRICES : start - XFMEM_POSTMATRICES;
    int _end = end < XFMEM_POSTMATRICES_END ? end - XFMEM_POSTMATRICES :
                                              XFMEM_POSTMATRICES_END - XFMEM_POSTMATRICES;

    if (nPostTransformMatricesChanged[0] == -1)
    {
      nPostTransformMatricesChanged[0] = _start;
      nPostTransformMatricesChanged[1] = _end;
    }
    else
    {
      if (nPostTransformMatricesChanged[0] > _start)
        nPostTransformMatricesChanged[0] = _start;

      if (nPostTransformMatricesChanged[1] < _end)
        nPostTransformMatricesChanged[1] = _end;
    }
  }

  if (start < XFMEM_LIGHTS_END && end > XFMEM_LIGHTS)
  {
    int _start = start < XFMEM_LIGHTS ? XFMEM_LIGHTS : start - XFMEM_LIGHTS;
    int _end = end < XFMEM_LIGHTS_END ? end - XFMEM_LIGHTS : XFMEM_LIGHTS_END - XFMEM_LIGHTS;

    if (nLightsChanged[0] == -1)
    {
      nLightsChanged[0] = _start;
      nLightsChanged[1] = _end;
    }
    else
    {
      if (nLightsChanged[0] > _start)
        nLightsChanged[0] = _start;

      if (nLightsChanged[1] < _end)
        nLightsChanged[1] = _end;
    }
  }
}

void VertexShaderManager::SetTexMatrixChangedA(u32 Value)
{
  if (g_main_cp_state.matrix_index_a.Hex != Value)
  {
    g_vertex_manager->Flush();
    if (g_main_cp_state.matrix_index_a.PosNormalMtxIdx != (Value & 0x3f))
      bPosNormalMatrixChanged = true;
    bTexMatricesChanged[0] = true;
    g_main_cp_state.matrix_index_a.Hex = Value;
  }
}

void VertexShaderManager::SetTexMatrixChangedB(u32 Value)
{
  if (g_main_cp_state.matrix_index_b.Hex != Value)
  {
    g_vertex_manager->Flush();
    bTexMatricesChanged[1] = true;
    g_main_cp_state.matrix_index_b.Hex = Value;
  }
}

void VertexShaderManager::SetViewportChanged()
{
  bViewportChanged = true;
}

void VertexShaderManager::SetProjectionChanged()
{
  bProjectionChanged = true;
}

void VertexShaderManager::SetMaterialColorChanged(int index)
{
  nMaterialsChanged[index] = true;
}

void VertexShaderManager::TranslateView(float x, float y, float z)
{
  // Yes, (x, z, y) is the correct order.
  s_fViewTranslationVector += s_viewInvRotationMatrix * MathUtil::Vector3f(x, z, y);
  bProjectionChanged = true;
}

void VertexShaderManager::RotateView(float x, float y)
{
  s_fViewRotation += MathUtil::Vector2f(x, y);

  const Eigen::AngleAxisf x_rot(s_fViewRotation.x(), MathUtil::Vector3f::UnitY());
  const Eigen::AngleAxisf y_rot(s_fViewRotation.y(), MathUtil::Vector3f::UnitX());

  s_viewRotationMatrix = y_rot * x_rot;
  s_viewInvRotationMatrix = x_rot.inverse() * y_rot.inverse();

  bProjectionChanged = true;
}

void VertexShaderManager::ResetView()
{
  s_fViewTranslationVector.setZero();
  s_viewRotationMatrix.setIdentity();
  s_viewInvRotationMatrix.setIdentity();
  s_fViewRotation.setZero();

  bProjectionChanged = true;
}

void VertexShaderManager::SetVertexFormat(u32 components)
{
  if (components != constants.components)
  {
    constants.components = components;
    dirty = true;
  }
}

void VertexShaderManager::SetTexMatrixInfoChanged(int index)
{
  // TODO: Should we track this with more precision, like which indices changed?
  // The whole vertex constants are probably going to be uploaded regardless.
  bTexMtxInfoChanged = true;
}

void VertexShaderManager::SetLightingConfigChanged()
{
  bLightingConfigChanged = true;
}

void VertexShaderManager::TransformToClipSpace(const float* data, float* out, u32 MtxIdx)
{
  const MathUtil::Matrix3x4f world_matrix(&xfmem.posMatrices[(MtxIdx & 0x3f) * 4]);

  // We use the projection matrix calculated by VertexShaderManager, because it
  // includes any free look transformations.
  // Make sure VertexShaderManager::SetConstants() has been called first.

  const MathUtil::Vector3f vertex(data);
  const MathUtil::Vector4f result =
      g_fProjectionMatrix * (Eigen::AffineCompact3f(world_matrix) * vertex).homogeneous();
  std::copy(result.data(), &result.data()[4], out);
}

void VertexShaderManager::DoState(PointerWrap& p)
{
  p.Do(g_fProjectionMatrix);
  p.Do(s_viewportCorrection);
  p.Do(s_viewRotationMatrix);
  p.Do(s_viewInvRotationMatrix);
  p.Do(s_fViewTranslationVector);
  p.Do(s_fViewRotation);

  p.Do(nTransformMatricesChanged);
  p.Do(nNormalMatricesChanged);
  p.Do(nPostTransformMatricesChanged);
  p.Do(nLightsChanged);

  p.Do(nMaterialsChanged);
  p.Do(bTexMatricesChanged);
  p.Do(bPosNormalMatrixChanged);
  p.Do(bProjectionChanged);
  p.Do(bViewportChanged);
  p.Do(bTexMtxInfoChanged);
  p.Do(bLightingConfigChanged);

  p.Do(constants);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    Dirty();
  }
}
