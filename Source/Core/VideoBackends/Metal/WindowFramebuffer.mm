// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <QuartzCore/CAMetalLayer.h>
#include <QuartzCore/CVDisplayLink.h>
#include "Common/Assert.h"
#include "Common/Flag.h"
#include "Common/MsgHandler.h"
#include "VideoBackends/Metal/CommandBufferManager.h"
#include "VideoBackends/Metal/MetalContext.h"
#include "VideoBackends/Metal/MetalFramebuffer.h"
#include "VideoBackends/Metal/Util.h"

// How we achieve uncapped framerates in Metal:
// Instead of drawing directly to the drawable when we render the "frame" in Dolphin,
// we render to a off-screen texture (refered to as the "backbuffer"). This texture
// has the same format and dimensions as the drawable. We then set up a CVDisplayLink
// callback which is called every screen refresh, and blit from the off-screen texture
// to the drawable. This does have the potential of introducing one frame or less of
// additional input lag, as a present which just missed vsync won't show until the next
// vertical sync interval.

namespace Metal
{
class WindowFramebuffer final : public MetalFramebuffer
{
public:
  WindowFramebuffer(NSView* view, CAMetalLayer* layer, mtlpp::RenderPassDescriptor& load_rpd,
                    mtlpp::RenderPassDescriptor& discard_rpd,
                    mtlpp::RenderPassDescriptor& clear_rpd, AbstractTextureFormat format, u32 width,
                    u32 height, bool vsync)
      : MetalFramebuffer(load_rpd, discard_rpd, clear_rpd, format, AbstractTextureFormat::Undefined,
                         width, height, 1, 1)
  {
    m_view = view;
    m_layer = layer;
    [m_layer retain];
    m_vsync = vsync;
    if (!m_vsync)
    {
      // If something fails and we can't use an off-screen texture, fall back to immediate present.
      if (!CreateDisplayLink() || !CreateBackbuffer())
      {
        DestroyDisplayLink();
        m_vsync = true;
      }
    }
  }

  ~WindowFramebuffer()
  {
    if (!m_vsync)
      DestroyDisplayLink();

    [m_view setLayer:nil];
    [m_view setWantsLayer:NO];
    [m_layer release];
  }

  static CAMetalLayer* CreateLayer(NSView* view, mtlpp::PixelFormat pf)
  {
    CAMetalLayer* layer = [CAMetalLayer layer];
    [layer setDevice:((__bridge id<MTLDevice>)g_metal_context->GetDevice().GetPtr())];
    NSColorSpace* colorSpace = [NSColorSpace genericRGBColorSpace];
    [layer setColorspace:[colorSpace CGColorSpace]];
    [layer setPixelFormat:static_cast<MTLPixelFormat>(pf)];
    // Setting framebuffer only means we can't blit copy.
    [layer setFramebufferOnly:NO];
    [layer setFrame:[view frame]];
    [view setWantsLayer:YES];
    [view setLayer:layer];
    INFO_LOG(VIDEO, "Metal layer created");
    return layer;
  }

  void RecreateLayer()
  {
    [m_view setLayer:nil];
    [m_view setWantsLayer:NO];
    [m_layer release];

    m_layer = CreateLayer(m_view, Util::GetMetalPixelFormat(m_color_format));
    if (m_layer == nil)
    {
      PanicAlert("Failed to recreate metal layer.");
      return;
    }
    [m_layer retain];
  }

  bool UpdateSurface() override
  {
#if 0
    // Check for dimensions change.
    // TODO: This is currently broken.
    auto bounds = [m_view bounds];
    u32 width = static_cast<u32>(bounds.size.width);
    u32 height = static_cast<u32>(bounds.size.height);
    if (width != m_width || height != m_height)
    {
      INFO_LOG(VIDEO, "Window framebuffer resized: %ux%u", width, height);
      m_width = width;
      m_height = height;

      // If we're using delayed presentation, we need to recreate the backbuffer.
      if (!m_vsync && !CreateBackbuffer())
      {
        DestroyDisplayLink();
        m_vsync = true;
      }
    }
#endif

    // Check if vsync has changed. If so, start or stop delayed presentation.
    bool new_vsync = g_ActiveConfig.IsVSync();
    if (new_vsync != m_vsync)
    {
      // VSync enabled changed. So we need to either start or stop the display link.
      m_vsync = new_vsync;
      if (!m_vsync)
      {
        if (!CreateDisplayLink() || !CreateBackbuffer())
          m_vsync = false;
      }
      else
      {
        DestroyDisplayLink();
      }
    }

    // If delayed presentation is enabled, the RPD is already set up.
    if (!m_vsync)
    {
      m_load_rpd.GetColorAttachments()[0].SetTexture(m_backbuffer);
      m_discard_rpd.GetColorAttachments()[0].SetTexture(m_backbuffer);
      m_clear_rpd.GetColorAttachments()[0].SetTexture(m_backbuffer);
      return true;
    }

    @autoreleasepool
    {
      m_current_drawable = [m_layer nextDrawable];
      if (m_current_drawable == nil)
      {
        PanicAlert("Failed to get a metal drawable.");
        return false;
      }

      [m_current_drawable retain];
      m_current_texture = m_current_drawable.texture;
      [m_current_texture retain];

      // Update render pass descriptors with new textures.
      ((__bridge MTLRenderPassDescriptor*)m_load_rpd.GetPtr()).colorAttachments[0].texture =
          m_current_texture;
      ((__bridge MTLRenderPassDescriptor*)m_discard_rpd.GetPtr()).colorAttachments[0].texture =
          m_current_texture;
      ((__bridge MTLRenderPassDescriptor*)m_clear_rpd.GetPtr()).colorAttachments[0].texture =
          m_current_texture;
    }

    return true;
  }

  void Present() override
  {
    if (m_vsync)
    {
      _dbg_assert_(VIDEO, m_current_drawable != nil);
      id<MTLCommandBuffer> cmdbuf =
          (__bridge id<MTLCommandBuffer>)g_command_buffer_mgr->GetCurrentBuffer().GetPtr();
      [cmdbuf presentDrawable:m_current_drawable];
      [m_current_texture release];
      m_current_texture = nil;
      [m_current_drawable release];
      m_current_drawable = nil;
    }
    else
    {
      // Set the present flag for later, so we
      m_should_present.Set();
    }
  }

private:
  bool CreateDisplayLink()
  {
    if (CVDisplayLinkCreateWithActiveCGDisplays(&m_display_link_ref) != kCVReturnSuccess ||
        CVDisplayLinkSetOutputCallback(m_display_link_ref, DisplayLinkCallback, this) != kCVReturnSuccess ||
        CVDisplayLinkStart(m_display_link_ref) != kCVReturnSuccess)
    {
      PanicAlert("Failed to create display link.");
      return false;
    }

    return true;
  }

  void DestroyDisplayLink()
  {
    if (!m_display_link_ref)
      return;

    CVDisplayLinkStop(m_display_link_ref);
    CVDisplayLinkRelease(m_display_link_ref);
    m_display_link_ref = nullptr;
  }

  static CVReturn DisplayLinkCallback(CVDisplayLinkRef display_link, const CVTimeStamp* in_now,
                                  const CVTimeStamp* in_output_time, CVOptionFlags in_flags,
                                  CVOptionFlags* flags_out, void* context)
  {
    WindowFramebuffer* self = static_cast<WindowFramebuffer*>(context);
    if (!self->m_should_present.TestAndClear())
    {
      // Source has not changed since the last frame.
      return kCVReturnSuccess;
    }

    std::lock_guard<std::mutex> guard(self->m_present_mutex);
    @autoreleasepool
    {
      // Grab the next drawable, and create a command buffer which we can push the copy command to.
      id<CAMetalDrawable> drawable = [self->m_layer nextDrawable];
      if (drawable == nil)
      {
        WARN_LOG(VIDEO, "Failed to get a drawable for presentation.");
        return kCVReturnSuccess;
      }

      id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)g_command_buffer_mgr->GetCommandQueue().GetPtr();
      id<MTLCommandBuffer> buf = [queue commandBufferWithUnretainedReferences];
      if (buf == nil)
      {
        WARN_LOG(VIDEO, "Failed to get command buffer for presentation.");
        return kCVReturnSuccess;
      }

      id<MTLBlitCommandEncoder> encoder = [buf blitCommandEncoder];
      if (encoder == nil)
      {
        WARN_LOG(VIDEO, "Failed to get blit encoder for presentation.");
        return kCVReturnSuccess;
      }

      // Copy from staging texture to drawable. As the dimensions and formats match,
      // this should be a simple memcpy()-like operation for the GPU.
      [encoder copyFromTexture:(__bridge id<MTLTexture>)self->m_backbuffer.GetPtr()
               sourceSlice:0 sourceLevel: 0 sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(self->m_width, self->m_height, 1) toTexture:drawable.texture
               destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
      [encoder endEncoding];

      // Present the drawable to the window/screen, and commit (enqueue) the command buffer.
      [buf presentDrawable:drawable];
      [buf commit];
    }

    return kCVReturnSuccess;
  }

  bool CreateBackbuffer()
  {
    std::lock_guard<std::mutex> guard(m_present_mutex);
    if (!m_backbuffer || m_backbuffer.GetWidth() != m_width || m_backbuffer.GetHeight() != m_height)
    {
      mtlpp::PixelFormat format = Util::GetMetalPixelFormat(m_color_format);
      mtlpp::TextureDescriptor descriptor =
          mtlpp::TextureDescriptor::Texture2DDescriptor(format, m_width, m_height, false);
      descriptor.SetStorageMode(mtlpp::StorageMode::Private);
      descriptor.SetUsage(mtlpp::TextureUsage::RenderTarget);
      m_backbuffer = {};
      m_backbuffer = g_metal_context->GetDevice().NewTexture(descriptor);
      if (!m_backbuffer)
      {
        PanicAlert("Failed to create staging texture for delayed presentation.");
        return false;
      }

      INFO_LOG(VIDEO, "Created a %ux%u staging texture for delayed presentation.", m_width,
               m_height);
    }

    // Update descriptors to point to the backbuffer rather than the last drawable.
    m_load_rpd.GetColorAttachments()[0].SetTexture(m_backbuffer);
    m_discard_rpd.GetColorAttachments()[0].SetTexture(m_backbuffer);
    m_clear_rpd.GetColorAttachments()[0].SetTexture(m_backbuffer);

    // Don't present until we have actually sent the texture a frame. Otherwise we risk
    // presenting garbage, or whatever the texture last contained.
    m_should_present.Clear();
    return true;
  }

  void DestroyBackbuffer()
  {
    std::lock_guard<std::mutex> guard(m_present_mutex);
    m_backbuffer = {};
  }

  NSView* m_view;
  CAMetalLayer* m_layer;
  id<CAMetalDrawable> m_current_drawable = nil;
  id<MTLTexture> m_current_texture = nil;

  mtlpp::Texture m_backbuffer;
  mtlpp::RenderPassDescriptor m_present_rpd;
  CVDisplayLinkRef m_display_link_ref = nullptr;
  std::mutex m_present_mutex;

  Common::Flag m_should_present{true};
  bool m_vsync;
};

static void FillUnboundDescriptor(mtlpp::RenderPassDescriptor& rpd, mtlpp::LoadAction load_action)
{
  rpd.SetRenderTargetArrayLength(1);
  auto color_attach = rpd.GetColorAttachments()[0];
  color_attach.SetLevel(0);
  color_attach.SetSlice(0);
  color_attach.SetLoadAction(load_action);
  color_attach.SetStoreAction(mtlpp::StoreAction::Store);
}

std::unique_ptr<MetalFramebuffer> MetalFramebuffer::CreateForWindow(void* window_handle,
                                                                    AbstractTextureFormat format)
{
  mtlpp::PixelFormat pf = Util::GetMetalPixelFormat(format);
  NSView* view = reinterpret_cast<NSView*>(window_handle);
  CAMetalLayer* layer = WindowFramebuffer::CreateLayer(view, pf);
  if (!layer)
  {
    PanicAlert("Failed to create Metal layer.");
    return nullptr;
  }

  mtlpp::RenderPassDescriptor load_rpd, discard_rpd, clear_rpd;
  FillUnboundDescriptor(load_rpd, mtlpp::LoadAction::Load);
  FillUnboundDescriptor(discard_rpd, mtlpp::LoadAction::DontCare);
  FillUnboundDescriptor(clear_rpd, mtlpp::LoadAction::Clear);

  // Get dimensions.
  auto bounds = [view bounds];
  u32 width = static_cast<u32>(bounds.size.width);
  u32 height = static_cast<u32>(bounds.size.height);
  return std::make_unique<WindowFramebuffer>(view, layer, load_rpd, discard_rpd, clear_rpd, format,
                                             width, height, g_ActiveConfig.IsVSync());
}

}  // namespace Metal
