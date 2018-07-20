
#pragma once

#include <libretro.h>
#include "VideoBackends/Null/Render.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/SWTexture.h"
#include "VideoCommon/VideoConfig.h"
#ifndef __APPLE__
#include "VideoBackends/Vulkan/VulkanLoader.h"
#endif
#ifdef _WIN32
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/DXShader.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/TextureCache.h"
#endif

namespace Libretro
{
namespace Video
{
void Init(void);
extern retro_video_refresh_t video_cb;
extern struct retro_hw_render_callback hw_render;

#ifndef __APPLE__
namespace Vk
{
void Init(VkInstance instance, VkPhysicalDevice gpu, VkSurfaceKHR surface,
          PFN_vkGetInstanceProcAddr get_instance_proc_addr, const char** required_device_extensions,
          unsigned num_required_device_extensions, const char** required_device_layers,
          unsigned num_required_device_layers, const VkPhysicalDeviceFeatures* required_features);
void SetSurfaceSize(uint32_t width, uint32_t height);
void SetHWRenderInterface(retro_hw_render_interface* hw_render_interface);
void Shutdown();
void WaitForPresentation();
}  // namespace Vk
#endif

class SWRenderer : public ::SWRenderer
{
public:
  void SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks) override
  {
    SW::SWTexture* sw_image = static_cast<SW::SWTexture*>(texture);
    video_cb(sw_image->GetData(), rc.GetWidth(), rc.GetHeight(), sw_image->GetWidth() * 4);
    UpdateActiveConfig();
  }
};

class NullRenderer : public Null::Renderer
{
public:
  void SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks) override
  {
    video_cb(NULL, 512, 512, 512 * 4);
    UpdateActiveConfig();
  }
};
#ifdef _WIN32
class DX11Renderer : public DX11::Renderer
{
public:
  DX11Renderer() : DX11::Renderer(1,1) {}
  void SwapImpl(AbstractTexture* texture, const EFBRectangle& rc, u64 ticks) override
  {
    DX11::D3DTexture2D* xfb_texture = static_cast<DX11::DXTexture*>(texture)->GetRawTexIdentifier();

    ID3D11RenderTargetView* nullView = nullptr;
    DX11::D3D::context->OMSetRenderTargets(1, &nullView, nullptr);
    DX11::D3D::context->PSSetShaderResources(0, 1, &xfb_texture->GetSRV());
    Libretro::Video::video_cb(RETRO_HW_FRAME_BUFFER_VALID, rc.GetWidth(), rc.GetHeight(), 0);

    ResetAPIState();
    g_texture_cache->Cleanup(frameCount);

    // Enable configuration changes
    UpdateActiveConfig();
    g_texture_cache->OnConfigChanged(g_ActiveConfig);

    // Resize the back buffers NOW to avoid flickering
    if (CalculateTargetSize())
    {
      UpdateDrawRectangle();
      g_framebuffer_manager.reset();
      g_framebuffer_manager =
          std::make_unique<DX11::FramebufferManager>(m_target_width, m_target_height);
      static constexpr std::array<float, 4> clear_color{{0.f, 0.f, 0.f, 1.f}};
      DX11::D3D::context->ClearRenderTargetView(
          DX11::FramebufferManager::GetEFBColorTexture()->GetRTV(), clear_color.data());
      DX11::D3D::context->ClearDepthStencilView(
          DX11::FramebufferManager::GetEFBDepthTexture()->GetDSV(), D3D11_CLEAR_DEPTH, 0.f, 0);
    }

    CheckForHostConfigChanges();
    // begin next frame
    RestoreAPIState();
    DX11::D3D::stateman->Restore();
  }
};
#endif
}  // namespace Video
}  // namespace Libretro
