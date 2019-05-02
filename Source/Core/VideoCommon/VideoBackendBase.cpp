// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VideoBackendBase.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"

// TODO: ugly
#ifdef _WIN32
#include "VideoBackends/D3D/VideoBackend.h"
#include "VideoBackends/D3D12/VideoBackend.h"
#endif
#include "VideoBackends/Null/VideoBackend.h"
#include "VideoBackends/OGL/VideoBackend.h"
#include "VideoBackends/Software/VideoBackend.h"
#include "VideoBackends/Vulkan/VideoBackend.h"

#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoState.h"

std::vector<std::unique_ptr<VideoBackendBase>> g_available_video_backends;
VideoBackendBase* g_video_backend = nullptr;
static VideoBackendBase* s_default_backend = nullptr;

#ifdef _WIN32
#include <windows.h>

// Nvidia drivers >= v302 will check if the application exports a global
// variable named NvOptimusEnablement to know if it should run the app in high
// performance graphics mode or using the IGP.
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
}
#endif

void VideoBackendBase::Video_ExitLoop()
{
  Fifo::ExitGpuLoop();
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackendBase::Video_BeginField(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height,
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
      ERROR_LOG(VIDEO, "BBox shall be used but it is disabled. Please use a gameini to enable it "
                       "for this game.");
    }
    warn_once = false;
    return 0;
  }

  if (!g_ActiveConfig.backend_info.bSupportsBBox)
  {
    static bool warn_once = true;
    if (warn_once)
    {
      PanicAlertT("This game requires bounding box emulation to run properly but your graphics "
                  "card or its drivers do not support it. As a result you will experience bugs or "
                  "freezes while running this game.");
    }
    warn_once = false;
    return 0;
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

void VideoBackendBase::PopulateList()
{
  // OGL > D3D11 > Vulkan > SW > Null
  g_available_video_backends.push_back(std::make_unique<OGL::VideoBackend>());
#ifdef _WIN32
  g_available_video_backends.push_back(std::make_unique<DX11::VideoBackend>());
  g_available_video_backends.push_back(std::make_unique<DX12::VideoBackend>());
#endif
  g_available_video_backends.push_back(std::make_unique<Vulkan::VideoBackend>());
  g_available_video_backends.push_back(std::make_unique<SW::VideoSoftware>());
  g_available_video_backends.push_back(std::make_unique<Null::VideoBackend>());

  const auto iter =
      std::find_if(g_available_video_backends.begin(), g_available_video_backends.end(),
                   [](const auto& backend) { return backend != nullptr; });

  if (iter == g_available_video_backends.end())
    return;

  s_default_backend = iter->get();
  g_video_backend = iter->get();
}

void VideoBackendBase::ClearList()
{
  g_available_video_backends.clear();
}

void VideoBackendBase::ActivateBackend(const std::string& name)
{
  // If empty, set it to the default backend (expected behavior)
  if (name.empty())
    g_video_backend = s_default_backend;

  const auto iter =
      std::find_if(g_available_video_backends.begin(), g_available_video_backends.end(),
                   [&name](const auto& backend) { return name == backend->GetName(); });

  if (iter == g_available_video_backends.end())
    return;

  g_video_backend = iter->get();
}

void VideoBackendBase::PopulateBackendInfo()
{
  // If the core is running, the backend info will have been populated already.
  // If we did it here, the UI thread can race with the with the GPU thread.
  if (Core::IsRunning())
    return;

  // We refresh the config after initializing the backend info, as system-specific settings
  // such as anti-aliasing, or the selected adapter may be invalid, and should be checked.
  ActivateBackend(SConfig::GetInstance().m_strVideoBackend);
  g_video_backend->InitBackendInfo();
  g_Config.Refresh();
}

// Run from the CPU thread
void VideoBackendBase::DoState(PointerWrap& p)
{
  bool software = false;
  p.Do(software);

  if (p.GetMode() == PointerWrap::MODE_READ && software == true)
  {
    // change mode to abort load of incompatible save state.
    p.SetMode(PointerWrap::MODE_VERIFY);
  }

  VideoCommon_DoState(p);
  p.DoMarker("VideoCommon");

  // Refresh state.
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    m_invalid = true;

    // Clear all caches that touch RAM
    // (? these don't appear to touch any emulation state that gets saved. moved to on load only.)
    VertexLoaderManager::MarkAllDirty();
  }
}

void VideoBackendBase::CheckInvalidState()
{
  if (m_invalid)
  {
    m_invalid = false;

    BPReload();
    g_texture_cache->Invalidate();
  }
}

void VideoBackendBase::InitializeShared()
{
  memset(&g_main_cp_state, 0, sizeof(g_main_cp_state));
  memset(&g_preprocess_cp_state, 0, sizeof(g_preprocess_cp_state));
  memset(texMem, 0, TMEM_SIZE);

  // do not initialize again for the config window
  m_initialized = true;

  m_invalid = false;

  CommandProcessor::Init();
  Fifo::Init();
  OpcodeDecoder::Init();
  PixelEngine::Init();
  BPInit();
  VertexLoaderManager::Init();
  IndexGenerator::Init();
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
