
#pragma once

#include <libretro.h>
#include "VideoBackends/Null/Render.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/SWTexture.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#ifndef __APPLE__
#include "VideoBackends/Vulkan/VulkanLoader.h"
#endif
#ifdef _WIN32
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/DXShader.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/SwapChain.h"
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

class SWRenderer : public SW::SWRenderer
{
public:
  SWRenderer()
      : SW::SWRenderer(SWOGLWindow::Create(
            WindowSystemInfo(WindowSystemType::Libretro, nullptr, nullptr, nullptr)))
  {
  }
  void RenderXFBToScreen(const MathUtil::Rectangle<int>& target_rc,
                         const AbstractTexture* source_texture,
                         const MathUtil::Rectangle<int>& source_rc) override
  {
    m_texture = static_cast<const SW::SWTexture*>(source_texture);
    m_rc = source_rc;
    SW::SWRenderer::RenderXFBToScreen(target_rc, source_texture, source_rc);
  }

  void PresentBackbuffer() override
  {
    video_cb(m_texture->GetData(), m_rc.GetWidth(), m_rc.GetHeight(), m_texture->GetWidth() * 4);
    UpdateActiveConfig();
  }

private:
  const SW::SWTexture* m_texture;
  MathUtil::Rectangle<int> m_rc;
};

class NullRenderer : public Null::Renderer
{
public:
  void PresentBackbuffer() override
  {
    video_cb(NULL, 512, 512, 512 * 4);
    UpdateActiveConfig();
  }
};
#ifdef _WIN32
class DX11SwapChain : public DX11::SwapChain
{
public:
  DX11SwapChain(const WindowSystemInfo& wsi, int width, int height)
      : DX11::SwapChain(wsi, nullptr, nullptr)
  {
    m_width = width;
    m_height = height;
    m_stereo = WantsStereo();
    CreateSwapChainBuffers();
  }

  bool Present() override
  {
    ID3D11ShaderResourceView* srv = m_texture->GetD3DSRV();

    ID3D11RenderTargetView* nullView = nullptr;
    DX11::D3D::context->OMSetRenderTargets(1, &nullView, nullptr);
    DX11::D3D::context->PSSetShaderResources(0, 1, &srv);
    Libretro::Video::video_cb(RETRO_HW_FRAME_BUFFER_VALID, m_width, m_height, m_width);
    DX11::D3D::stateman->Restore();
    return true;
  }

protected:
  bool CreateSwapChainBuffers() override
  {
    TextureConfig config(m_width, m_height, 1, 1, 1, AbstractTextureFormat::RGBA8,
                         AbstractTextureFlag_RenderTarget);

    m_texture = DX11::DXTexture::Create(config);
    m_framebuffer = DX11::DXFramebuffer::Create(m_texture.get(), nullptr);
    return true;
  }
};

#endif
}  // namespace Video
}  // namespace Libretro
