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
#include "VideoBackends/D3D/D3DState.h"
#endif
#ifndef __APPLE__
#include <libretro_vulkan.h>
#endif

#include "Common/GL/GLInterfaceBase.h"
#include "Common/GL/GLUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Version.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "DolphinLibretro/Options.h"
#include "VideoBackends/Null/Render.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#ifndef __APPLE__
#include "VideoBackends/Vulkan/VideoBackend.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#endif

namespace Libretro
{
extern retro_environment_t environ_cb;
namespace Video
{
retro_video_refresh_t video_cb;
struct retro_hw_render_callback hw_render;

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
    DX11::D3D::featlevel = d3d->featureLevel;
    DX11::D3D::stateman = new DX11::D3D::StateManager;
    DX11::PD3DCompile = d3d->D3DCompile;
    if (FAILED(d3d->device->QueryInterface<ID3D11Device1>(&DX11::D3D::device1)))
    {
      WARN_LOG(VIDEO, "Missing Direct3D 11.1 support. Logical operations will not be supported.");
      g_Config.backend_info.bSupportsLogicOp = false;
    }
    UINT bc1_support, bc2_support, bc3_support, bc7_support;
    d3d->device->CheckFormatSupport(DXGI_FORMAT_BC1_UNORM, &bc1_support);
    d3d->device->CheckFormatSupport(DXGI_FORMAT_BC2_UNORM, &bc2_support);
    d3d->device->CheckFormatSupport(DXGI_FORMAT_BC7_UNORM, &bc7_support);
    d3d->device->CheckFormatSupport(DXGI_FORMAT_BC3_UNORM, &bc3_support);
    g_Config.backend_info.bSupportsST3CTextures =
        (bc1_support & bc2_support & bc3_support) & D3D11_FORMAT_SUPPORT_TEXTURE2D;
    g_Config.backend_info.bSupportsBPTCTextures = bc7_support & D3D11_FORMAT_SUPPORT_TEXTURE2D;
  }
#endif

  g_video_backend->Initialize(nullptr);
  g_video_backend->CheckInvalidState();
}

static void ContextDestroy(void)
{
  DEBUG_LOG(VIDEO, "Context destroy!\n");

  g_video_backend->Shutdown();
  switch (hw_render.context_type)
  {
  case RETRO_HW_CONTEXT_DIRECT3D:
#ifdef _WIN32
    DX11::D3D::device = nullptr;
    DX11::D3D::device1 = nullptr;
    DX11::D3D::context = nullptr;
    DX11::PD3DCompile = nullptr;
    delete DX11::D3D::stateman;
#endif
    break;
  case RETRO_HW_CONTEXT_VULKAN:
#ifndef __APPLE__
    Vk::SetHWRenderInterface(nullptr);
#endif
    break;
  case RETRO_HW_CONTEXT_OPENGL:
  case RETRO_HW_CONTEXT_OPENGLES2:
  case RETRO_HW_CONTEXT_OPENGL_CORE:
  case RETRO_HW_CONTEXT_OPENGLES3:
  case RETRO_HW_CONTEXT_OPENGLES_VERSION:
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

void Init()
{
  if (Options::renderer == "Hardware")
  {
    std::vector<retro_hw_context_type> openglTypes = {
        RETRO_HW_CONTEXT_OPENGL_CORE, RETRO_HW_CONTEXT_OPENGL, RETRO_HW_CONTEXT_OPENGLES3,
        RETRO_HW_CONTEXT_OPENGLES2};
    hw_render.version_major = 3;
    hw_render.version_minor = 1;
    hw_render.context_reset = ContextReset;
    hw_render.context_destroy = ContextDestroy;
    hw_render.bottom_left_origin = true;
    for (retro_hw_context_type type : openglTypes)
    {
      hw_render.context_type = type;
      if (environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      {
        environ_cb(RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT, nullptr);
        SConfig::GetInstance().m_strVideoBackend = "OGL";
        return;
      }
    }
#ifdef _WIN32
    hw_render.context_type = RETRO_HW_CONTEXT_DIRECT3D;
    hw_render.version_major = 11;
    hw_render.version_minor = 0;

    if (environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
    {
      SConfig::GetInstance().m_strVideoBackend = "D3D";
      return;
    }
#endif
#ifndef __APPLE__
    hw_render.context_type = RETRO_HW_CONTEXT_VULKAN;
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

      SConfig::GetInstance().m_strVideoBackend = "Vulkan";
      return;
    }
#endif
  }
  hw_render.context_type = RETRO_HW_CONTEXT_NONE;
  if (Options::renderer == "Software")
    SConfig::GetInstance().m_strVideoBackend = "Software Renderer";
  else
    SConfig::GetInstance().m_strVideoBackend = "Null";
}

}  // namespace Video
}  // namespace Libretro

/* retroGL interface*/

class cInterfaceRGL : public cInterfaceBase
{
public:
  void Swap() override
  {
    Libretro::Video::video_cb(RETRO_HW_FRAME_BUFFER_VALID, s_backbuffer_width, s_backbuffer_height,
                              0);
  }
  void SetMode(GLInterfaceMode mode) override { s_opengl_mode = mode; }
  void* GetFuncAddress(const std::string& name) override
  {
    return (void*)Libretro::Video::hw_render.get_proc_address(name.c_str());
  }
  virtual bool Create(void* window_handle, bool stereo, bool core) override
  {
    s_backbuffer_width = EFB_WIDTH * Libretro::Options::efbScale;
    s_backbuffer_height = EFB_HEIGHT * Libretro::Options::efbScale;
    switch (Libretro::Video::hw_render.context_type)
    {
    case RETRO_HW_CONTEXT_OPENGLES2:
      s_opengl_mode = GLInterfaceMode::MODE_OPENGLES2;
      break;
    case RETRO_HW_CONTEXT_OPENGLES3:
      s_opengl_mode = GLInterfaceMode::MODE_OPENGLES3;
      break;
    default:
    case RETRO_HW_CONTEXT_OPENGL_CORE:
    case RETRO_HW_CONTEXT_OPENGL:
      s_opengl_mode = GLInterfaceMode::MODE_OPENGL;
      break;
    }

    return true;
  }
};

std::unique_ptr<cInterfaceBase> HostGL_CreateGLInterface()
{
  return std::make_unique<cInterfaceRGL>();
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
  Libretro::Video::video_cb = cb;
}
