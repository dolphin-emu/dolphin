// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VideoBase.h"

#include "Common/ChunkFile.h"

#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/System.h"

#include "VideoBackendBase.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/BPStructs.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FrameDumper.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TMEM.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoState.h"
#include "VideoCommon/Widescreen.h"

#ifdef _WIN32
#include <windows.h>
#include "VideoBase.h"

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

bool VideoBase::Initialize()
{
  m_backend = VideoBackendBase::GetConfiguredBackend();
  if (!m_backend)
    return false;

  m_backend->PopulateBackendInfo();

  return InitializeShared(m_backend);
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBase::OutputXFB(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, u64 ticks)
{
  if (m_initialized && g_presenter && !g_ActiveConfig.bImmediateXFB)
  {
    auto& system = Core::System::GetInstance();
    system.GetFifo().SyncGPU(Fifo::SyncGPUReason::Swap);

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

u32 VideoBase::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 data)
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

u32 VideoBase::GetQueryResult(PerfQueryType type)
{
  if (!g_perf_query->ShouldEmulate())
  {
    return 0;
  }

  auto& system = Core::System::GetInstance();
  system.GetFifo().SyncGPU(Fifo::SyncGPUReason::PerfQuery);

  AsyncRequests::Event e;
  e.time = 0;
  e.type = AsyncRequests::Event::PERF_QUERY;

  if (!g_perf_query->IsFlushed())
    AsyncRequests::GetInstance()->PushEvent(e, true);

  return g_perf_query->GetQueryResult(type);
}

u16 VideoBase::GetBoundingBox(int index)
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
  else if (!g_gfx->BackendInfo().bSupportsBBox)
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

  auto& system = Core::System::GetInstance();
  system.GetFifo().SyncGPU(Fifo::SyncGPUReason::BBox);

  AsyncRequests::Event e;
  u16 result;
  e.time = 0;
  e.type = AsyncRequests::Event::BBOX_READ;
  e.bbox.index = index;
  e.bbox.data = &result;
  AsyncRequests::GetInstance()->PushEvent(e, true);

  return result;
}

void VideoBase::DoState(PointerWrap& p)
{
  auto& system = Core::System::GetInstance();
  if (!system.IsDualCoreMode())
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
  system.GetFifo().GpuMaySleep();
}

const BackendInfo& VideoBase::GetBackendInfo() const
{
  if (m_backend)
    return m_backend->backend_info;

  auto backend = VideoBackendBase::GetConfiguredBackend();
  backend->InitBackendInfo();

  return backend->backend_info;
}
bool VideoBase::InitializeShared(VideoBackendBase* backend)
{
  memset(reinterpret_cast<u8*>(&g_main_cp_state), 0, sizeof(g_main_cp_state));
  memset(reinterpret_cast<u8*>(&g_preprocess_cp_state), 0, sizeof(g_preprocess_cp_state));
  memset(texMem, 0, TMEM_SIZE);

  // This is the core context and needs to be created before anything else.
  g_gfx = backend->CreateGfx();

  if (!g_gfx)
  {
    PanicAlertFmtT("Failed to initialize {0} backend.", backend->GetDisplayName());
    return false;
  }

  g_vertex_manager = backend->CreateVertexManager();
  g_perf_query = backend->CreatePerfQuery();
  g_bounding_box = backend->CreateBoundingBox();

  // Null and Software Backends supply their own derived Renderer and Texture Cache
  g_texture_cache = backend->CreateTextureCache();
  g_renderer = backend->CreateRenderer();

  g_presenter = std::make_unique<VideoCommon::Presenter>();
  g_frame_dumper = std::make_unique<FrameDumper>();
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  g_shader_cache = std::make_unique<VideoCommon::ShaderCache>();
  g_graphics_mod_manager = std::make_unique<GraphicsModManager>();
  g_widescreen = std::make_unique<WidescreenManager>();

  if (!g_vertex_manager->Initialize() || !g_shader_cache->Initialize() ||
      !g_perf_query->Initialize() || !g_presenter->Initialize() ||
      !g_framebuffer_manager->Initialize() || !g_texture_cache->Initialize() ||
      !g_bounding_box->Initialize() || !g_graphics_mod_manager->Initialize())
  {
    PanicAlertFmtT("Failed to initialize renderer classes");
    Shutdown();
    return false;
  }

  auto& system = Core::System::GetInstance();
  auto& command_processor = system.GetCommandProcessor();
  command_processor.Init(system);
  system.GetFifo().Init(system);
  system.GetPixelEngine().Init(system);
  BPInit();
  VertexLoaderManager::Init();
  system.GetVertexShaderManager().Init();
  system.GetGeometryShaderManager().Init();
  system.GetPixelShaderManager().Init();
  TMEM::Init();

  g_Config.VerifyValidity(backend->backend_info);
  UpdateActiveConfig();

  g_shader_cache->InitializeShaderCache();

  return true;
}

void VideoBase::Shutdown()
{
  g_gfx->WaitForGPUIdle();

  g_frame_dumper.reset();
  g_presenter.reset();

  if (g_shader_cache)
    g_shader_cache->Shutdown();
  if (g_texture_cache)
    g_texture_cache->Shutdown();

  g_bounding_box.reset();
  g_perf_query.reset();
  g_texture_cache.reset();
  g_framebuffer_manager.reset();
  g_shader_cache.reset();
  g_vertex_manager.reset();
  g_renderer.reset();
  g_widescreen.reset();
  g_presenter.reset();
  g_gfx.reset();

  m_initialized = false;

  auto& system = Core::System::GetInstance();
  VertexLoaderManager::Clear();
  system.GetFifo().Shutdown();

  // FIXME-PR: How to get correct ordering on shutdown? Split into an Early and Late version?
  m_backend->Shutdown();

  m_backend = nullptr;
}
