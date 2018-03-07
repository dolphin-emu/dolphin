// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/MetalFramebuffer.h"
#include "Common/Assert.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/Metal/CommandBufferManager.h"
#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/MetalTexture.h"
#include "VideoBackends/Metal/Util.h"

namespace Metal
{
MetalFramebuffer::MetalFramebuffer(mtlpp::RenderPassDescriptor& load_rpd,
                                   mtlpp::RenderPassDescriptor& discard_rpd,
                                   mtlpp::RenderPassDescriptor& clear_rpd,
                                   AbstractTextureFormat color_format,
                                   AbstractTextureFormat depth_format, u32 width, u32 height,
                                   u32 layers, u32 samples)
    : AbstractFramebuffer(color_format, depth_format, width, height, layers, samples),
      m_load_rpd(load_rpd), m_discard_rpd(discard_rpd), m_clear_rpd(clear_rpd)
{
}

MetalFramebuffer::~MetalFramebuffer()
{
}

const mtlpp::RenderPassDescriptor&
MetalFramebuffer::GetClearDescriptor(const Renderer::ClearColor& clear_color,
                                     float clear_depth) const
{
  if (m_color_format != AbstractTextureFormat::Undefined && clear_color != m_last_clear_color)
  {
    mtlpp::ClearColor mtl_clear_color(clear_color[0], clear_color[1], clear_color[2],
                                      clear_color[3]);
    m_clear_rpd.GetColorAttachments()[0].SetClearColor(mtl_clear_color);
    m_last_clear_color = clear_color;
  }

  if (m_depth_format != AbstractTextureFormat::Undefined && clear_depth != m_last_clear_depth)
  {
    m_clear_rpd.GetDepthAttachment().SetClearDepth(clear_depth);
    m_last_clear_depth = clear_depth;
  }

  return m_clear_rpd;
}

static void MakeDescriptor(mtlpp::RenderPassDescriptor& rpd, const mtlpp::Texture* color_tex,
                           const mtlpp::Texture* depth_tex, mtlpp::LoadAction load_action,
                           mtlpp::StoreAction store_action = mtlpp::StoreAction::Store)
{
  // Color
  if (color_tex)
  {
    rpd.SetRenderTargetArrayLength(1);
    auto color_attach = rpd.GetColorAttachments()[0];
    color_attach.SetTexture(*color_tex);
    color_attach.SetLevel(0);
    color_attach.SetSlice(0);
    color_attach.SetLoadAction(load_action);
    color_attach.SetStoreAction(store_action);
    if (load_action == mtlpp::LoadAction::Clear)
      color_attach.SetClearColor(mtlpp::ClearColor(0.0, 0.0, 0.0, 0.0));
  }

  // Depth
  if (depth_tex)
  {
    mtlpp::RenderPassDepthAttachmentDescriptor ddesc;
    ddesc.SetTexture(*depth_tex);
    ddesc.SetLevel(0);
    ddesc.SetSlice(0);
    ddesc.SetLoadAction(load_action);
    ddesc.SetStoreAction(store_action);
    if (load_action == mtlpp::LoadAction::Clear)
      ddesc.SetClearDepth(1.0f);
    rpd.SetDepthAttachment(ddesc);
  }
}

std::unique_ptr<MetalFramebuffer>
MetalFramebuffer::CreateForAbstractTexture(const MetalTexture* color_attachment,
                                           const MetalTexture* depth_attachment)
{
  const MetalTexture* either_attachment = color_attachment ? color_attachment : depth_attachment;
  _dbg_assert_(VIDEO, either_attachment && ValidateConfig(color_attachment, depth_attachment));

  const u32 width = either_attachment->GetWidth();
  const u32 height = either_attachment->GetHeight();
  const u32 layers = either_attachment->GetLayers();
  const u32 samples = either_attachment->GetSamples();
  const AbstractTextureFormat color_format =
      color_attachment ? color_attachment->GetFormat() : AbstractTextureFormat::Undefined;
  const AbstractTextureFormat depth_format =
      depth_attachment ? depth_attachment->GetFormat() : AbstractTextureFormat::Undefined;

  mtlpp::RenderPassDescriptor load_rpd, discard_rpd, clear_rpd;
  MakeDescriptor(load_rpd, color_attachment ? &color_attachment->GetTexture() : nullptr,
                 depth_attachment ? &depth_attachment->GetTexture() : nullptr,
                 mtlpp::LoadAction::Load);
  MakeDescriptor(discard_rpd, color_attachment ? &color_attachment->GetTexture() : nullptr,
                 depth_attachment ? &depth_attachment->GetTexture() : nullptr,
                 mtlpp::LoadAction::DontCare);
  MakeDescriptor(clear_rpd, color_attachment ? &color_attachment->GetTexture() : nullptr,
                 depth_attachment ? &depth_attachment->GetTexture() : nullptr,
                 mtlpp::LoadAction::Clear);

  return std::make_unique<MetalFramebuffer>(load_rpd, discard_rpd, clear_rpd, color_format,
                                            depth_format, width, height, layers, samples);
}

std::unique_ptr<MetalFramebuffer> MetalFramebuffer::CreateForTexture(const mtlpp::Texture& tex)
{
  const u32 width = tex.GetWidth();
  const u32 height = tex.GetHeight();
  const AbstractTextureFormat format = Util::GetAbstractTextureFormat(tex.GetPixelFormat());

  mtlpp::RenderPassDescriptor load_rpd, discard_rpd, clear_rpd;
  MakeDescriptor(load_rpd, &tex, nullptr, mtlpp::LoadAction::Load);
  MakeDescriptor(discard_rpd, &tex, nullptr, mtlpp::LoadAction::DontCare);
  MakeDescriptor(clear_rpd, &tex, nullptr, mtlpp::LoadAction::Clear);

  return std::make_unique<MetalFramebuffer>(load_rpd, discard_rpd, clear_rpd, format,
                                            AbstractTextureFormat::Undefined, width, height, 1, 1);
}

}  // namespace Metal
