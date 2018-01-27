// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/Logging/Log.h"
#include "Core/Host.h"
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
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoState.h"

static Common::Flag s_FifoShuttingDown;

static volatile struct
{
  u32 xfbAddr;
  u32 fbWidth;
  u32 fbStride;
  u32 fbHeight;
} s_beginFieldArgs;

void VideoBackendBase::Video_ExitLoop()
{
  Fifo::ExitGpuLoop();
  s_FifoShuttingDown.Set();
}

void VideoBackendBase::Video_AsyncTimewarpDraw()
{
  g_renderer->AsyncTimewarpDraw();
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackendBase::Video_BeginField(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight,
                                        u64 ticks)
{
  if (m_initialized && g_ActiveConfig.bUseXFB && g_renderer)
  {
    Fifo::SyncGPU(Fifo::SyncGPUReason::Swap);

    AsyncRequests::Event e;
    e.time = ticks;
    e.type = AsyncRequests::Event::SWAP_EVENT;

    e.swap_event.xfbAddr = xfbAddr;
    e.swap_event.fbWidth = fbWidth;
    e.swap_event.fbStride = fbStride;
    e.swap_event.fbHeight = fbHeight;
    AsyncRequests::GetInstance()->PushEvent(e, false);
  }
}

u32 VideoBackendBase::Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
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
    e.efb_poke.data = InputData;
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
      ERROR_LOG(VIDEO, "BBox shall be used but it is disabled. Please use a gameini to enable it "
                       "for this game.");
    warn_once = false;
    return 0;
  }

  if (!g_ActiveConfig.backend_info.bSupportsBBox)
  {
    static bool warn_once = true;
    if (warn_once)
      PanicAlertT("This game requires bounding box emulation to run properly but your graphics "
                  "card or its drivers do not support it. As a result you will experience bugs or "
                  "freezes while running this game.");
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

void VideoBackendBase::ShowConfig(void* parent_handle)
{
  if (!m_initialized)
    InitBackendInfo();

  Host_ShowVideoConfig(parent_handle, GetDisplayName());
}

void VideoBackendBase::InitializeShared()
{
  memset(&g_main_cp_state, 0, sizeof(g_main_cp_state));
  memset(&g_preprocess_cp_state, 0, sizeof(g_preprocess_cp_state));
  memset(texMem, 0, TMEM_SIZE);

  // Do our OSD callbacks
  OSD::DoCallbacks(OSD::CallbackType::Initialization);

  // do not initialize again for the config window
  m_initialized = true;

  s_FifoShuttingDown.Clear();
  memset((void*)&s_beginFieldArgs, 0, sizeof(s_beginFieldArgs));
  m_invalid = false;
  frameCount = 0;

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

  g_Config.Refresh();
  g_Config.UpdateProjectionHack();
  UpdateActiveConfig();
}

void VideoBackendBase::ShutdownShared()
{
  OpcodeDecoder::Shutdown();

  // Do our OSD callbacks
  OSD::DoCallbacks(OSD::CallbackType::Shutdown);

  m_initialized = false;

  Fifo::Shutdown();
}

void VideoBackendBase::CleanupShared()
{
  VertexLoaderManager::Clear();
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

  p.Do(s_beginFieldArgs);
  p.DoMarker("VideoBackendBase");

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
