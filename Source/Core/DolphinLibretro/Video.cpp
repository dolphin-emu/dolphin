// needs to be first
#include "DolphinLibretro/Video.h"

#include <cassert>
#include <libretro.h>
#include <memory>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define HAVE_D3D11
#include <libretro_d3d.h>
#include "Common/DynamicLibrary.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/VideoBackend.h"
#endif
#ifndef __APPLE__
#include <libretro_vulkan.h>
#endif

#include "Common/GL/GLContext.h"
#include "Common/GL/GLUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Version.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "DolphinLibretro/Options.h"
#include "VideoBackends/Null/Render.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#ifndef __APPLE__
#include "VideoBackends/Vulkan/VideoBackend.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#endif

/* retroGL interface*/

class RGLContext : public GLContext
{
public:
  RGLContext()
  {
    WindowSystemInfo wsi(WindowSystemType::Libretro, nullptr, nullptr, nullptr);
    Initialize(wsi, g_Config.stereo_mode == StereoMode::QuadBuffer, true);
  }
  bool IsHeadless() const override { return false; }
  void Swap() override
  {
    Libretro::Video::video_cb(RETRO_HW_FRAME_BUFFER_VALID, m_backbuffer_width, m_backbuffer_height,
                              0);
  }
  void* GetFuncAddress(const std::string& name) override
  {
    return (void*)Libretro::Video::hw_render.get_proc_address(name.c_str());
  }
  virtual bool Initialize(const WindowSystemInfo& wsi, bool stereo, bool core) override
  {
    m_backbuffer_width = EFB_WIDTH * Libretro::Options::efbScale;
    m_backbuffer_height = EFB_HEIGHT * Libretro::Options::efbScale;
    switch (Libretro::Video::hw_render.context_type)
    {
    case RETRO_HW_CONTEXT_OPENGLES3:
      m_opengl_mode = Mode::OpenGLES;
      break;
    default:
    case RETRO_HW_CONTEXT_OPENGL_CORE:
    case RETRO_HW_CONTEXT_OPENGL:
      m_opengl_mode = Mode::OpenGL;
      break;
    }

    return true;
  }
};

namespace Libretro
{
extern retro_environment_t environ_cb;
namespace Video
{
retro_video_refresh_t video_cb;
struct retro_hw_render_callback hw_render;
#ifdef _WIN32
static Common::DynamicLibrary d3d11_library;
#endif

static void ContextReset(void)
{
  DEBUG_LOG(VIDEO, "Context reset!\n");

#ifndef __APPLE__
  if (hw_render.context_type == RETRO_HW_CONTEXT_VULKAN)
  {
    retro_hw_render_interface* vulkan;
    if (!Libretro::environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, (void**)&vulkan) ||
        !vulkan)
    {
      ERROR_LOG(VIDEO, "Failed to get HW rendering interface!\n");
      return;
    }
    if (vulkan->interface_version != RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION)
    {
      ERROR_LOG(VIDEO, "HW render interface mismatch, expected %u, got %u!\n",
                RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION, vulkan->interface_version);
      return;
    }
    Vk::SetHWRenderInterface(vulkan);
    Vk::SetSurfaceSize(EFB_WIDTH * Libretro::Options::efbScale,
                       EFB_HEIGHT * Libretro::Options::efbScale);
  }
#endif
#ifdef _WIN32
  if (hw_render.context_type == RETRO_HW_CONTEXT_DIRECT3D)
  {
    retro_hw_render_interface_d3d11* d3d;
    if (!Libretro::environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, (void**)&d3d) || !d3d)
    {
      ERROR_LOG(VIDEO, "Failed to get HW rendering interface!\n");
      return;
    }
    if (d3d->interface_version != RETRO_HW_RENDER_INTERFACE_D3D11_VERSION)
    {
      ERROR_LOG(VIDEO, "HW render interface mismatch, expected %u, got %u!\n",
                RETRO_HW_RENDER_INTERFACE_D3D11_VERSION, d3d->interface_version);
      return;
    }
    DX11::D3D::device = d3d->device;
    DX11::D3D::context = d3d->context;
    DX11::D3D::feature_level = d3d->featureLevel;
    D3DCommon::d3d_compile = d3d->D3DCompile;

    if (!d3d11_library.Open("d3d11.dll") || !D3DCommon::LoadLibraries())
    {
      d3d11_library.Close();
      ERROR_LOG(VIDEO, "Failed to load D3D11 Libraries");
      return;
    }

    if (FAILED(DX11::D3D::device.As(&DX11::D3D::device1)))
    {
      WARN_LOG(VIDEO, "Missing Direct3D 11.1 support. Logical operations will not be supported.");
      g_Config.backend_info.bSupportsLogicOp = false;
    }
    DX11::D3D::stateman = std::make_unique<DX11::D3D::StateManager>();

    DX11::VideoBackend* d3d11 = static_cast<DX11::VideoBackend*>(g_video_backend);
    d3d11->FillBackendInfo();
    d3d11->InitializeShared();

    WindowSystemInfo wsi(WindowSystemType::Libretro, nullptr, nullptr, nullptr);
    std::unique_ptr<DX11SwapChain> swap_chain = std::make_unique<DX11SwapChain>(
        wsi, EFB_WIDTH * Libretro::Options::efbScale, EFB_HEIGHT * Libretro::Options::efbScale);
    g_renderer =
        std::make_unique<DX11::Renderer>(std::move(swap_chain), Libretro::Options::efbScale);
  }
#endif
  if (Config::Get(Config::MAIN_GFX_BACKEND) == "OGL")
  {
    std::unique_ptr<GLContext> main_gl_context = std::make_unique<RGLContext>();

    OGL::VideoBackend* ogl = static_cast<OGL::VideoBackend*>(g_video_backend);
    ogl->InitializeGLExtensions(main_gl_context.get());
    ogl->FillBackendInfo();
    ogl->InitializeShared();
    g_renderer =
        std::make_unique<OGL::Renderer>(std::move(main_gl_context), Libretro::Options::efbScale);
  }

  WindowSystemInfo wsi(WindowSystemType::Libretro, nullptr, nullptr, nullptr);
  g_video_backend->Initialize(wsi);
}

static void ContextDestroy(void)
{
  DEBUG_LOG(VIDEO, "Context destroy!\n");

  if (Config::Get(Config::MAIN_GFX_BACKEND) == "OGL")
  {
    static_cast<OGL::Renderer*>(g_renderer.get())->SetSystemFrameBuffer(0);
  }

  g_video_backend->Shutdown();
  switch (hw_render.context_type)
  {
  case RETRO_HW_CONTEXT_DIRECT3D:
#ifdef _WIN32
    DX11::D3D::context->ClearState();
    DX11::D3D::context->Flush();

    DX11::D3D::context.Reset();
    DX11::D3D::device1.Reset();

    DX11::D3D::device = nullptr;
    DX11::D3D::device1 = nullptr;
    DX11::D3D::context = nullptr;
    D3DCommon::d3d_compile = nullptr;
    DX11::D3D::stateman.reset();
    D3DCommon::UnloadLibraries();
    d3d11_library.Close();
#endif
    break;
  case RETRO_HW_CONTEXT_VULKAN:
#ifndef __APPLE__
    Vk::SetHWRenderInterface(nullptr);
#endif
    break;
  case RETRO_HW_CONTEXT_OPENGL:
  case RETRO_HW_CONTEXT_OPENGL_CORE:
  case RETRO_HW_CONTEXT_OPENGLES3:
    break;
  default:
  case RETRO_HW_CONTEXT_NONE:
    break;
  }
}
#ifndef __APPLE__
namespace Vk
{
static const VkApplicationInfo* GetApplicationInfo(void)
{
  static VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app_info.pApplicationName = "Dolphin-Emu";
  app_info.applicationVersion = 5;  // TODO: extract from Common::scm_desc_str
  app_info.pEngineName = "Dolphin-Emu";
  app_info.engineVersion = 2;
  app_info.apiVersion = VK_API_VERSION_1_0;
  return &app_info;
}

static bool CreateDevice(retro_vulkan_context* context, VkInstance instance, VkPhysicalDevice gpu,
                         VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                         const char** required_device_extensions,
                         unsigned num_required_device_extensions,
                         const char** required_device_layers, unsigned num_required_device_layers,
                         const VkPhysicalDeviceFeatures* required_features)
{
  assert(g_video_backend->GetName() == "Vulkan");

  Vulkan::LoadVulkanLibrary();

  Init(instance, gpu, surface, get_instance_proc_addr, required_device_extensions,
       num_required_device_extensions, required_device_layers, num_required_device_layers,
       required_features);

  if (!Vulkan::LoadVulkanInstanceFunctions(instance))
  {
    ERROR_LOG(VIDEO, "Failed to load Vulkan instance functions.");
    Vulkan::UnloadVulkanLibrary();
    return false;
  }

  Vulkan::VulkanContext::GPUList gpu_list = Vulkan::VulkanContext::EnumerateGPUs(instance);
  if (gpu_list.empty())
  {
    ERROR_LOG(VIDEO, "No Vulkan physical devices available.");
    Vulkan::UnloadVulkanLibrary();
    return false;
  }

  Vulkan::VulkanContext::PopulateBackendInfo(&g_Config);
  Vulkan::VulkanContext::PopulateBackendInfoAdapters(&g_Config, gpu_list);

  if (gpu == VK_NULL_HANDLE)
    gpu = gpu_list[0];

  Vulkan::g_vulkan_context = Vulkan::VulkanContext::Create(instance, gpu, surface, false, false);
  if (!Vulkan::g_vulkan_context)
  {
    ERROR_LOG(VIDEO, "Failed to create Vulkan device");
    Vulkan::UnloadVulkanLibrary();
    return false;
  }

  context->gpu = Vulkan::g_vulkan_context->GetPhysicalDevice();
  context->device = Vulkan::g_vulkan_context->GetDevice();
  context->queue = Vulkan::g_vulkan_context->GetGraphicsQueue();
  context->queue_family_index = Vulkan::g_vulkan_context->GetGraphicsQueueFamilyIndex();
  context->presentation_queue = context->queue;
  context->presentation_queue_family_index = context->queue_family_index;

  return true;
}
}  // namespace Vk
#endif

static bool SetHWRender(retro_hw_context_type type)
{
  hw_render.context_type = type;
  hw_render.context_reset = ContextReset;
  hw_render.context_destroy = ContextDestroy;
  hw_render.bottom_left_origin = true;
  switch (type)
  {
  case RETRO_HW_CONTEXT_OPENGL_CORE:
    // minimum requirements to run is opengl 3.3 (RA will try to use highest version available anyway)
    hw_render.version_major = 3;
    hw_render.version_minor = 3;
    if (environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
    {
      Config::SetBase(Config::MAIN_GFX_BACKEND, "OGL");
      return true;
    }
    break;
  case RETRO_HW_CONTEXT_OPENGLES3:
  case RETRO_HW_CONTEXT_OPENGL:
    // when using RETRO_HW_CONTEXT_OPENGL you can't set version above 3.0 (RA will try to use highest version available anyway)
    // dolphin support OpenGL ES 3.0 too (no support for 2.0) so we are good
    hw_render.version_major = 3;
    hw_render.version_minor = 0;
    if (environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
    {
      // Shared context is required with "gl" video driver
      environ_cb(RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT, nullptr);
      Config::SetBase(Config::MAIN_GFX_BACKEND, "OGL");
      return true;
    }
    break;
#ifdef _WIN32
  case RETRO_HW_CONTEXT_DIRECT3D:
    hw_render.version_major = 11;
    hw_render.version_minor = 0;
    if (environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
    {
      Config::SetBase(Config::MAIN_GFX_BACKEND, "D3D");
      return true;
    }
    break;
#endif
#ifndef __APPLE__
  case RETRO_HW_CONTEXT_VULKAN:
    hw_render.version_major = VK_API_VERSION_1_0;
    hw_render.version_minor = 0;
    if (environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
    {
      static const struct retro_hw_render_context_negotiation_interface_vulkan iface = {
          RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN,
          RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION,
          Vk::GetApplicationInfo,
          Vk::CreateDevice,
          NULL,
      };
      environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE, (void*)&iface);

      Config::SetBase(Config::MAIN_GFX_BACKEND, "Vulkan");
      return true;
    }
    break;
#endif
  default:
    break;
  }
  return false;
}
void Init()
{
  if (Options::renderer == "Hardware")
  {
    retro_hw_context_type preferred = RETRO_HW_CONTEXT_NONE;
    if (environ_cb(RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER, &preferred) && SetHWRender(preferred))
      return;
    if (SetHWRender(RETRO_HW_CONTEXT_OPENGL_CORE))
      return;
    if (SetHWRender(RETRO_HW_CONTEXT_OPENGL))
      return;
    if (SetHWRender(RETRO_HW_CONTEXT_OPENGLES3))
      return;
#ifdef _WIN32
    if (SetHWRender(RETRO_HW_CONTEXT_DIRECT3D))
      return;
#endif
#ifndef __APPLE__
    if (SetHWRender(RETRO_HW_CONTEXT_VULKAN))
      return;
#endif
  }
  hw_render.context_type = RETRO_HW_CONTEXT_NONE;
  if (Options::renderer == "Software")
    Config::SetBase(Config::MAIN_GFX_BACKEND, "Software Renderer");
  else
    Config::SetBase(Config::MAIN_GFX_BACKEND, "Null");
}

}  // namespace Video
}  // namespace Libretro

void retro_set_video_refresh(retro_video_refresh_t cb)
{
  Libretro::Video::video_cb = cb;
}
