// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PipelineUtils.h"

#include "Common/Assert.h"
#include "Common/Logging/Log.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
/// Edits the UID based on driver bugs and other special configurations
GXPipelineUid ApplyDriverBugs(const GXPipelineUid& in)
{
  GXPipelineUid out;
  // TODO: static_assert(std::is_trivially_copyable_v<GXPipelineUid>);
  // GXPipelineUid is not trivially copyable because RasterizationState and BlendingState aren't
  // either, but we can pretend it is for now. This will be improved after PR #10848 is finished.
  memcpy(static_cast<void*>(&out), static_cast<const void*>(&in), sizeof(out));  // copy padding
  pixel_shader_uid_data* ps = out.ps_uid.GetUidData();
  BlendingState& blend = out.blending_state;

  if (ps->ztest == EmulatedZ::ForcedEarly && !out.depth_state.update_enable)
  {
    // No need to force early depth test if you're not writing z
    ps->ztest = EmulatedZ::Early;
  }

  // If framebuffer fetch is available, we can emulate logic ops in the fragment shader
  // and don't need the below blend approximation
  if (blend.logic_op_enable && !g_backend_info.bSupportsLogicOp &&
      !g_backend_info.bSupportsFramebufferFetch)
  {
    if (!blend.LogicOpApproximationIsExact())
      WARN_LOG_FMT(VIDEO,
                   "Approximating logic op with blending, this will produce incorrect rendering.");
    if (blend.LogicOpApproximationWantsShaderHelp())
    {
      ps->emulate_logic_op_with_blend = true;
      ps->logic_op_mode = static_cast<u32>(blend.logic_mode.Value());
    }
    blend.ApproximateLogicOpWithBlending();
  }

  const bool benefits_from_ps_dual_source_off =
      (!g_backend_info.bSupportsDualSourceBlend && g_backend_info.bSupportsFramebufferFetch) ||
      DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DUAL_SOURCE_BLENDING);
  if (benefits_from_ps_dual_source_off && !blend.RequiresDualSrc())
  {
    // Only use dual-source blending when required on drivers that don't support it very well.
    ps->no_dual_src = true;
    blend.use_dual_src = false;
  }

  if (g_backend_info.bSupportsFramebufferFetch)
  {
    bool fbfetch_blend = false;
    if ((DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DISCARD_WITH_EARLY_Z) ||
         !g_backend_info.bSupportsEarlyZ) &&
        ps->ztest == EmulatedZ::ForcedEarly)
    {
      ps->ztest = EmulatedZ::EarlyWithFBFetch;
      fbfetch_blend |= static_cast<bool>(out.blending_state.blend_enable);
      ps->no_dual_src = true;
    }
    fbfetch_blend |= blend.logic_op_enable && !g_backend_info.bSupportsLogicOp;
    fbfetch_blend |= blend.use_dual_src && !g_backend_info.bSupportsDualSourceBlend;
    if (fbfetch_blend)
    {
      ps->no_dual_src = true;
      if (blend.logic_op_enable)
      {
        ps->logic_op_enable = true;
        ps->logic_op_mode = static_cast<u32>(blend.logic_mode.Value());
        blend.logic_op_enable = false;
      }
      if (blend.blend_enable)
      {
        ps->blend_enable = true;
        ps->blend_src_factor = blend.src_factor;
        ps->blend_src_factor_alpha = blend.src_factor_alpha;
        ps->blend_dst_factor = blend.dst_factor;
        ps->blend_dst_factor_alpha = blend.dst_factor_alpha;
        ps->blend_subtract = blend.subtract;
        ps->blend_subtract_alpha = blend.subtract_alpha;
        blend.blend_enable = false;
      }
    }
  }

  // force dual src off if we can't support it
  if (!g_backend_info.bSupportsDualSourceBlend)
  {
    ps->no_dual_src = true;
    blend.use_dual_src = false;
  }

  if (ps->ztest == EmulatedZ::ForcedEarly && !g_backend_info.bSupportsEarlyZ)
  {
    // These things should be false
    ASSERT(!ps->zfreeze);
    // ZCOMPLOC HACK:
    // The only way to emulate alpha test + early-z is to force early-z in the shader.
    // As this isn't available on all drivers and as we can't emulate this feature otherwise,
    // we are only able to choose which one we want to respect more.
    // Tests seem to have proven that writing depth even when the alpha test fails is more
    // important that a reliable alpha test, so we just force the alpha test to always succeed.
    // At least this seems to be less buggy.
    ps->ztest = EmulatedZ::EarlyWithZComplocHack;
  }

  if (g_ActiveConfig.UseVSForLinePointExpand() &&
      (out.rasterization_state.primitive == PrimitiveType::Points ||
       out.rasterization_state.primitive == PrimitiveType::Lines))
  {
    // All primitives are expanded to triangles in the vertex shader
    vertex_shader_uid_data* vs = out.vs_uid.GetUidData();
    const PortableVertexDeclaration& decl = out.vertex_format->GetVertexDeclaration();
    vs->position_has_3_elems = decl.position.components >= 3;
    vs->texcoord_elem_count = 0;
    for (int i = 0; i < 8; i++)
    {
      if (decl.texcoords[i].enable)
      {
        ASSERT(decl.texcoords[i].components <= 3);
        vs->texcoord_elem_count |= decl.texcoords[i].components << (i * 2);
      }
    }
    out.vertex_format = nullptr;
    if (out.rasterization_state.primitive == PrimitiveType::Points)
      vs->vs_expand = VSExpand::Point;
    else
      vs->vs_expand = VSExpand::Line;
    PrimitiveType prim = g_backend_info.bSupportsPrimitiveRestart ? PrimitiveType::TriangleStrip :
                                                                    PrimitiveType::Triangles;
    out.rasterization_state.primitive = prim;
    out.gs_uid.GetUidData()->primitive_type = static_cast<u32>(prim);
  }

  return out;
}

std::size_t PipelineToHash(const GXPipelineUid& in)
{
  XXH3_state_t pipeline_hash_state;
  XXH3_INITSTATE(&pipeline_hash_state);
  XXH3_64bits_reset_withSeed(&pipeline_hash_state, static_cast<XXH64_hash_t>(1));
  UpdateHashWithPipeline(in, &pipeline_hash_state);
  return XXH3_64bits_digest(&pipeline_hash_state);
}

void UpdateHashWithPipeline(const GXPipelineUid& in, XXH3_state_t* hash_state)
{
  XXH3_64bits_update(hash_state, &in.vertex_format->GetVertexDeclaration(),
                     sizeof(PortableVertexDeclaration));
  XXH3_64bits_update(hash_state, &in.blending_state, sizeof(BlendingState));
  XXH3_64bits_update(hash_state, &in.depth_state, sizeof(DepthState));
  XXH3_64bits_update(hash_state, &in.rasterization_state, sizeof(RasterizationState));
  XXH3_64bits_update(hash_state, &in.gs_uid, sizeof(GeometryShaderUid));
  XXH3_64bits_update(hash_state, &in.ps_uid, sizeof(PixelShaderUid));
  XXH3_64bits_update(hash_state, &in.vs_uid, sizeof(VertexShaderUid));
}
}  // namespace VideoCommon
