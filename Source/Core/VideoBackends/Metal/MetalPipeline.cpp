// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/MetalPipeline.h"
#include "VideoBackends/Metal/MetalShader.h"
#include "VideoBackends/Metal/MetalVertexFormat.h"
#include "VideoBackends/Metal/Util.h"

namespace Metal
{
MetalPipeline::MetalPipeline(const mtlpp::RenderPipelineState& rps,
                             const mtlpp::DepthStencilState& dss,
                             mtlpp::DepthClipMode depth_clip_mode, mtlpp::CullMode cull_mode,
                             mtlpp::PrimitiveType primitive_type)
    : m_render_pipeline_state(rps), m_depth_stencil_state(dss), m_depth_clip_mode(depth_clip_mode),
      m_cull_mode(cull_mode), m_primitive_type(primitive_type)
{
}

MetalPipeline::~MetalPipeline()
{
  // TODO: Deferred destruction
}

const mtlpp::RenderPipelineState& MetalPipeline::GetRenderPipelineState() const
{
  return m_render_pipeline_state;
}

const mtlpp::DepthStencilState& MetalPipeline::GetDepthStencilState() const
{
  return m_depth_stencil_state;
}

mtlpp::DepthClipMode MetalPipeline::GetDepthClipMode() const
{
  return m_depth_clip_mode;
}

mtlpp::CullMode MetalPipeline::GetCullMode() const
{
  return m_cull_mode;
}

mtlpp::PrimitiveType MetalPipeline::GetPrimitiveType() const
{
  return m_primitive_type;
}

static mtlpp::DepthClipMode MapDepthClipMode()
{
  return mtlpp::DepthClipMode::Clamp;
}

static mtlpp::CullMode MapCullMode(const GenMode::CullMode mode)
{
  switch (mode)
  {
  case GenMode::CULL_NONE:
    return mtlpp::CullMode::None;
  case GenMode::CULL_BACK:
    return mtlpp::CullMode::Back;
  case GenMode::CULL_FRONT:
    return mtlpp::CullMode::Front;
  case GenMode::CULL_ALL:
  default:
    // Not implemented.
    return mtlpp::CullMode::Front;
  }
}

static mtlpp::PrimitiveTopologyClass GetPrimitiveTopologyClass(PrimitiveType type)
{
  switch (type)
  {
  case PrimitiveType::Points:
    return mtlpp::PrimitiveTopologyClass::Point;
  case PrimitiveType::Lines:
    return mtlpp::PrimitiveTopologyClass::Line;
  case PrimitiveType::Triangles:
  case PrimitiveType::TriangleStrip:
  default:
    return mtlpp::PrimitiveTopologyClass::Triangle;
  }
}

static mtlpp::PrimitiveType MapPrimitiveType(PrimitiveType type)
{
  switch (type)
  {
  case PrimitiveType::Points:
    return mtlpp::PrimitiveType::Point;
  case PrimitiveType::Lines:
    return mtlpp::PrimitiveType::Line;
  case PrimitiveType::Triangles:
    return mtlpp::PrimitiveType::Triangle;
  case PrimitiveType::TriangleStrip:
    return mtlpp::PrimitiveType::TriangleStrip;
  default:
    return mtlpp::PrimitiveType::Triangle;
  }
}

static mtlpp::BlendFactor GetBlendFactor(BlendMode::BlendFactor factor, bool usedualsrc)
{
  const bool dual_src = usedualsrc && g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
  switch (factor)
  {
  case BlendMode::ZERO:
    return mtlpp::BlendFactor::Zero;
  case BlendMode::ONE:
    return mtlpp::BlendFactor::One;
  case BlendMode::SRCCLR:
    return mtlpp::BlendFactor::SourceColor;
  case BlendMode::INVSRCCLR:
    return mtlpp::BlendFactor::OneMinusSourceColor;
  case BlendMode::SRCALPHA:
    return dual_src ? mtlpp::BlendFactor::Source1Alpha : mtlpp::BlendFactor::SourceAlpha;
  case BlendMode::INVSRCALPHA:
    return dual_src ? mtlpp::BlendFactor::OneMinusSource1Alpha :
                      mtlpp::BlendFactor::OneMinusSourceAlpha;
  case BlendMode::DSTALPHA:
    return mtlpp::BlendFactor::DestinationAlpha;
  case BlendMode::INVDSTALPHA:
    return mtlpp::BlendFactor::OneMinusDestinationAlpha;
  default:
    return mtlpp::BlendFactor::Zero;
  }
}

static void SetColorAttachment(mtlpp::RenderPipelineColorAttachmentDescriptor desc,
                               const AbstractPipelineConfig& config)
{
  mtlpp::ColorWriteMask write_mask = mtlpp::ColorWriteMask::None;
  if (config.blending_state.colorupdate)
  {
    write_mask = static_cast<mtlpp::ColorWriteMask>(static_cast<u32>(mtlpp::ColorWriteMask::Red) |
                                                    static_cast<u32>(mtlpp::ColorWriteMask::Green) |
                                                    static_cast<u32>(mtlpp::ColorWriteMask::Blue));
  }
  if (config.blending_state.alphaupdate)
  {
    write_mask = static_cast<mtlpp::ColorWriteMask>(static_cast<u32>(write_mask) |
                                                    static_cast<u32>(mtlpp::ColorWriteMask::Alpha));
  }

  desc.SetPixelFormat(Util::GetMetalPixelFormat(config.framebuffer_state.color_texture_format));
  desc.SetWriteMask(write_mask);
  desc.SetBlendingEnabled(config.blending_state.blendenable);
  desc.SetSourceRgbBlendFactor(
      GetBlendFactor(config.blending_state.srcfactor, config.blending_state.usedualsrc));
  desc.SetSourceAlphaBlendFactor(Util::GetAlphaBlendFactor(
      GetBlendFactor(config.blending_state.srcfactoralpha, config.blending_state.usedualsrc)));
  desc.SetDestinationRgbBlendFactor(
      GetBlendFactor(config.blending_state.dstfactor, config.blending_state.usedualsrc));
  desc.SetDestinationAlphaBlendFactor(Util::GetAlphaBlendFactor(
      GetBlendFactor(config.blending_state.dstfactoralpha, config.blending_state.usedualsrc)));
  desc.SetRgbBlendOperation(config.blending_state.subtract ?
                                mtlpp::BlendOperation::ReverseSubtract :
                                mtlpp::BlendOperation::Add);
  desc.SetAlphaBlendOperation(config.blending_state.subtractAlpha ?
                                  mtlpp::BlendOperation::ReverseSubtract :
                                  mtlpp::BlendOperation::Add);
}

std::unique_ptr<MetalPipeline> MetalPipeline::Create(const AbstractPipelineConfig& config)
{
  _dbg_assert_(VIDEO, config.vertex_shader && config.pixel_shader);

  mtlpp::RenderPipelineDescriptor desc;

  desc.SetInputPrimitiveTopology(GetPrimitiveTopologyClass(config.rasterization_state.primitive));
  if (config.vertex_format)
  {
    auto vdesc = static_cast<const MetalVertexFormat*>(config.vertex_format)->GetDescriptor();
    desc.SetVertexDescriptor(vdesc);
  }

  desc.SetRasterizationEnabled(true);
  desc.SetSampleCount(config.framebuffer_state.samples);
  SetColorAttachment(desc.GetColorAttachments()[0], config);
  if (config.framebuffer_state.depth_texture_format != AbstractTextureFormat::Undefined)
  {
    desc.SetDepthAttachmentPixelFormat(
        Util::GetMetalPixelFormat(config.framebuffer_state.depth_texture_format));
  }

  if (config.vertex_shader)
    desc.SetVertexFunction(static_cast<const MetalShader*>(config.vertex_shader)->GetFunction());
  if (config.pixel_shader)
    desc.SetFragmentFunction(static_cast<const MetalShader*>(config.pixel_shader)->GetFunction());

  ns::Error error;
  mtlpp::RenderPipelineState rps =
      g_metal_context->GetDevice().NewRenderPipelineState(desc, &error);
  if (!rps)
  {
    ERROR_LOG(VIDEO, "Failed to create Metal pipeline: Error code %u: %s", error.GetCode(),
              error.GetLocalizedDescription().GetCStr());
    return nullptr;
  }

  mtlpp::DepthStencilState& dss = g_metal_context->GetDepthState(config.depth_state);
  mtlpp::DepthClipMode dcm = MapDepthClipMode();
  mtlpp::CullMode cm = MapCullMode(config.rasterization_state.cullmode);
  mtlpp::PrimitiveType pt = MapPrimitiveType(config.rasterization_state.primitive);
  return std::make_unique<MetalPipeline>(rps, dss, dcm, cm, pt);
}
}  // namespace Metal
