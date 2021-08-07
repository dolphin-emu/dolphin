// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VideoBackendBase.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

// TODO: ugly
#ifdef _WIN32
#include "VideoBackends/D3D/VideoBackend.h"
#include "VideoBackends/D3D12/VideoBackend.h"
#endif
#include "VideoBackends/Null/VideoBackend.h"
#ifdef HAS_OPENGL
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoBackends/Software/VideoBackend.h"
#endif
#ifdef HAS_VULKAN
#include "VideoBackends/Vulkan/VideoBackend.h"
#endif

#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoState.h"

VideoBackendBase* g_video_backend = nullptr;

#ifdef _WIN32
#include <windows.h>

// Nvidia drivers >= v302 will check if the application exports a global
// variable named NvOptimusEnablement to know if it should run the app in high
// performance graphics mode or using the IGP.
// AMD drivers >= 13.35 do the same, but for the variable
// named AmdPowerXpressRequestHighPerformance instead.
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

std::string VideoBackendBase::BadShaderFilename(const char* shader_stage, int counter)
{
  return fmt::format("{}bad_{}_{}_{}.txt", File::GetUserPath(D_DUMP_IDX), shader_stage,
                     g_video_backend->GetName(), counter);
}

void VideoBackendBase::Video_ExitLoop()
{
  Fifo::ExitGpuLoop();
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackendBase::Video_OutputXFB(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height,
                                       u64 ticks)
{
  if (m_initialized && g_renderer && !g_ActiveConfig.bImmediateXFB)
  {
    Fifo::SyncGPU(Fifo::SyncGPUReason::Swap);

    AsyncRequests::Event e;
    e.time = ticks;
    e.type = AsyncRequests::Event::SWAP_EVENT;

    e.swap_event.xfbAddr = xfb_addr;
    e.swap_event.fbWidth = fb_width;
    e.swap_event.fbStride = fb_stride;
    e.swap_event.fbHeight = fb_height;
    AsyncRequests::GetInstance()->PushEvent(e, false);
  }
}

u32 VideoBackendBase::Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 data)
{
  if (!g_ActiveConfig.bEFBAccessEnable || x >= EFB_WIDTH || y >= EFB_HEIGHT)
  {
    return 0;
  }

  if (type == EFBAccessType::PokeColor || type == EFBAccessType::PokeZ)
  {
    AsyncRequests::Event e;
    e.type = type == EFBAccessType::PokeColor ? AsyncRequests::Event::EFB_POKE_COLOR :
                                                AsyncRequests::Event::EFB_POKE_Z;
    e.time = 0;
    e.efb_poke.data = data;
    e.efb_poke.x = x;
    e.efb_poke.y = y;
    AsyncRequests::GetInstance()->PushEvent(e, false);
    return 0;
  }
  else
  {
    AsyncRequests::Event e;
    u32 result;
    e.type = type == EFBAccessType::PeekColor ? AsyncRequests::Event::EFB_PEEK_COLOR :
                                                AsyncRequests::Event::EFB_PEEK_Z;
    e.time = 0;
    e.efb_peek.x = x;
    e.efb_peek.y = y;
    e.efb_peek.data = &result;
    AsyncRequests::GetInstance()->PushEvent(e, true);
    return result;
  }
}

u32 VideoBackendBase::Video_GetQueryResult(PerfQueryType type)
{
  if (!g_perf_query->ShouldEmulate())
  {
    return 0;
  }

  Fifo::SyncGPU(Fifo::SyncGPUReason::PerfQuery);

  AsyncRequests::Event e;
  e.time = 0;
  e.type = AsyncRequests::Event::PERF_QUERY;

  if (!g_perf_query->IsFlushed())
    AsyncRequests::GetInstance()->PushEvent(e, true);

  return g_perf_query->GetQueryResult(type);
}

u16 VideoBackendBase::Video_GetBoundingBox(int index)
{
  if (!g_ActiveConfig.bBBoxEnable)
  {
    static bool warn_once = true;
    if (warn_once)
    {
      ERROR_LOG_FMT(VIDEO,
                    "BBox shall be used but it is disabled. Please use a gameini to enable it "
                    "for this game.");
    }
    warn_once = false;
  }
  else if (!g_ActiveConfig.backend_info.bSupportsBBox)
  {
    static bool warn_once = true;
    if (warn_once)
    {
      PanicAlertFmtT(
          "This game requires bounding box emulation to run properly but your graphics "
          "card or its drivers do not support it. As a result you will experience bugs or "
          "freezes while running this game.");
    }
    warn_once = false;
  }

  Fifo::SyncGPU(Fifo::SyncGPUReason::BBox);

  AsyncRequests::Event e;
  u16 result;
  e.time = 0;
  e.type = AsyncRequests::Event::BBOX_READ;
  e.bbox.index = index;
  e.bbox.data = &result;
  AsyncRequests::GetInstance()->PushEvent(e, true);

  return result;
}

static VideoBackendBase* GetDefaultVideoBackend()
{
  const auto& backends = VideoBackendBase::GetAvailableBackends();
  if (backends.empty())
    return nullptr;
  return backends.front().get();
}

std::string VideoBackendBase::GetDefaultBackendName()
{
  auto* default_backend = GetDefaultVideoBackend();
  return default_backend ? default_backend->GetName() : "";
}

const std::vector<std::unique_ptr<VideoBackendBase>>& VideoBackendBase::GetAvailableBackends()
{
  static auto s_available_backends = [] {
    std::vector<std::unique_ptr<VideoBackendBase>> backends;

    // OGL > D3D11 > D3D12 > Vulkan > SW > Null
    //
    // On macOS Mojave and newer, we prefer Vulkan over OGL due to outdated drivers.
    // However, on macOS High Sierra and older, we still prefer OGL due to its older Metal version
    // missing several features required by the Vulkan backend.
#ifdef HAS_OPENGL
    backends.push_back(std::make_unique<OGL::VideoBackend>());
#endif
#ifdef _WIN32
    backends.push_back(std::make_unique<DX11::VideoBackend>());
    backends.push_back(std::make_unique<DX12::VideoBackend>());
#endif
#ifdef HAS_VULKAN
#ifdef __APPLE__
    // If we can run the Vulkan backend, emplace it at the beginning of the vector so
    // it takes precedence over OpenGL.
    if (__builtin_available(macOS 10.14, *))
    {
      backends.emplace(backends.begin(), std::make_unique<Vulkan::VideoBackend>());
    }
    else
#endif
    {
      backends.push_back(std::make_unique<Vulkan::VideoBackend>());
    }
#endif
#ifdef HAS_OPENGL
    backends.push_back(std::make_unique<SW::VideoSoftware>());
#endif
    backends.push_back(std::make_unique<Null::VideoBackend>());

    if (!backends.empty())
      g_video_backend = backends.front().get();

    return backends;
  }();
  return s_available_backends;
}

void VideoBackendBase::ActivateBackend(const std::string& name)
{
  // If empty, set it to the default backend (expected behavior)
  if (name.empty())
    g_video_backend = GetDefaultVideoBackend();

  const auto& backends = GetAvailableBackends();
  const auto iter = std::find_if(backends.begin(), backends.end(), [&name](const auto& backend) {
    return name == backend->GetName();
  });

  if (iter == backends.end())
    return;

  g_video_backend = iter->get();
}

void VideoBackendBase::PopulateBackendInfo()
{
  // We refresh the config after initializing the backend info, as system-specific settings
  // such as anti-aliasing, or the selected adapter may be invalid, and should be checked.
  ActivateBackend(Config::Get(Config::MAIN_GFX_BACKEND));
  g_video_backend->InitBackendInfo();
  g_Config.Refresh();
}

void VideoBackendBase::PopulateBackendInfoFromUI()
{
  // If the core is running, the backend info will have been populated already.
  // If we did it here, the UI thread can race with the with the GPU thread.
  if (!Core::IsRunning())
    PopulateBackendInfo();
}

void VideoBackendBase::DoState(PointerWrap& p)
{
  if (!SConfig::GetInstance().bCPUThread)
  {
    VideoCommon_DoState(p);
    return;
  }

  AsyncRequests::Event ev = {};
  ev.do_save_state.p = &p;
  ev.type = AsyncRequests::Event::DO_SAVE_STATE;
  AsyncRequests::GetInstance()->PushEvent(ev, true);

  // Let the GPU thread sleep after loading the state, so we're not spinning if paused after loading
  // a state. The next GP burst will wake it up again.
  Fifo::GpuMaySleep();
}

void VideoBackendBase::InitializeShared()
{
  memset(reinterpret_cast<u8*>(&g_main_cp_state), 0, sizeof(g_main_cp_state));
  memset(reinterpret_cast<u8*>(&g_preprocess_cp_state), 0, sizeof(g_preprocess_cp_state));
  memset(texMem, 0, TMEM_SIZE);

  // do not initialize again for the config window
  m_initialized = true;

  CommandProcessor::Init();
  Fifo::Init();
  OpcodeDecoder::Init();
  PixelEngine::Init();
  BPInit();
  VertexLoaderManager::Init();
  VertexShaderManager::Init();
  GeometryShaderManager::Init();
  PixelShaderManager::Init();

  g_Config.VerifyValidity();
  UpdateActiveConfig();
}

void VideoBackendBase::ShutdownShared()
{
  m_initialized = false;

  VertexLoaderManager::Clear();
  Fifo::Shutdown();
}
