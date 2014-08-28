#include "Common/Event.h"
#include "Core/ConfigManager.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/MainBase.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoState.h"

bool s_BackendInitialized = false;

Common::Flag s_swapRequested;
static Common::Flag s_FifoShuttingDown;
static Common::Flag s_efbAccessRequested;
static Common::Event s_efbAccessReadyEvent;

static Common::Flag s_perfQueryRequested;
static Common::Event s_perfQueryReadyEvent;

static volatile struct
{
	u32 xfbAddr;
	u32 fbWidth;
	u32 fbStride;
	u32 fbHeight;
} s_beginFieldArgs;

static struct
{
	EFBAccessType type;
	u32 x;
	u32 y;
	u32 Data;
} s_accessEFBArgs;

static u32 s_AccessEFBResult = 0;

void VideoBackendHardware::EmuStateChange(EMUSTATE_CHANGE newState)
{
	EmulatorState((newState == EMUSTATE_CHANGE_PLAY) ? true : false);
}

// Enter and exit the video loop
void VideoBackendHardware::Video_EnterLoop()
{
	RunGpuLoop();
}

void VideoBackendHardware::Video_ExitLoop()
{
	ExitGpuLoop();
	s_FifoShuttingDown.Set();
	s_efbAccessReadyEvent.Set();
	s_perfQueryReadyEvent.Set();
}

void VideoBackendHardware::Video_SetRendering(bool bEnabled)
{
	Fifo_SetRendering(bEnabled);
}

// Run from the graphics thread (from Fifo.cpp)
static void VideoFifo_CheckSwapRequest()
{
	if (g_ActiveConfig.bUseXFB)
	{
		if (s_swapRequested.IsSet())
		{
			EFBRectangle rc;
			Renderer::Swap(s_beginFieldArgs.xfbAddr, s_beginFieldArgs.fbWidth, s_beginFieldArgs.fbStride, s_beginFieldArgs.fbHeight, rc);
			s_swapRequested.Clear();
		}
	}
}

// Run from the graphics thread (from Fifo.cpp)
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	if (g_ActiveConfig.bUseXFB)
	{
		if (s_swapRequested.IsSet())
		{
			u32 aLower = xfbAddr;
			u32 aUpper = xfbAddr + 2 * fbWidth * fbHeight;
			u32 bLower = s_beginFieldArgs.xfbAddr;
			u32 bUpper = s_beginFieldArgs.xfbAddr + 2 * s_beginFieldArgs.fbStride * s_beginFieldArgs.fbHeight;

			if (AddressRangesOverlap(aLower, aUpper, bLower, bUpper))
				VideoFifo_CheckSwapRequest();
		}
	}
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackendHardware::Video_BeginField(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight)
{
	if (s_BackendInitialized && g_ActiveConfig.bUseXFB)
	{
		if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread)
			VideoFifo_CheckSwapRequest();
		s_beginFieldArgs.xfbAddr = xfbAddr;
		s_beginFieldArgs.fbWidth = fbWidth;
		s_beginFieldArgs.fbStride = fbStride;
		s_beginFieldArgs.fbHeight = fbHeight;
	}
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackendHardware::Video_EndField()
{
	if (s_BackendInitialized)
	{
		SyncGPU(SYNC_GPU_SWAP);
		s_swapRequested.Set();
	}
}

void VideoBackendHardware::Video_AddMessage(const std::string& msg, u32 milliseconds)
{
	OSD::AddMessage(msg, milliseconds);
}

void VideoBackendHardware::Video_ClearMessages()
{
	OSD::ClearMessages();
}

// Screenshot
bool VideoBackendHardware::Video_Screenshot(const std::string& filename)
{
	Renderer::SetScreenshot(filename.c_str());
	return true;
}

void VideoFifo_CheckEFBAccess()
{
	if (s_efbAccessRequested.IsSet())
	{
		s_AccessEFBResult = g_renderer->AccessEFB(s_accessEFBArgs.type, s_accessEFBArgs.x, s_accessEFBArgs.y, s_accessEFBArgs.Data);
		s_efbAccessRequested.Clear();
		s_efbAccessReadyEvent.Set();
	}
}

u32 VideoBackendHardware::Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
	if (s_BackendInitialized && g_ActiveConfig.bEFBAccessEnable)
	{
		SyncGPU(SYNC_GPU_EFB_POKE);

		s_accessEFBArgs.type = type;
		s_accessEFBArgs.x = x;
		s_accessEFBArgs.y = y;
		s_accessEFBArgs.Data = InputData;

		s_efbAccessRequested.Set();

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread)
		{
			s_efbAccessReadyEvent.Reset();
			if (s_FifoShuttingDown.IsSet())
				return 0;
			s_efbAccessRequested.Set();
			s_efbAccessReadyEvent.Wait();
		}
		else
			VideoFifo_CheckEFBAccess();

		return s_AccessEFBResult;
	}

	return 0;
}

static void VideoFifo_CheckPerfQueryRequest()
{
	if (s_perfQueryRequested.IsSet())
	{
		g_perf_query->FlushResults();
		s_perfQueryRequested.Clear();
		s_perfQueryReadyEvent.Set();
	}
}

u32 VideoBackendHardware::Video_GetQueryResult(PerfQueryType type)
{
	if (!g_perf_query->ShouldEmulate())
	{
		return 0;
	}

	SyncGPU(SYNC_GPU_PERFQUERY);

	// TODO: Is this check sane?
	if (!g_perf_query->IsFlushed())
	{
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread)
		{
			s_perfQueryReadyEvent.Reset();
			if (s_FifoShuttingDown.IsSet())
				return 0;
			s_perfQueryRequested.Set();
			s_perfQueryReadyEvent.Wait();
		}
		else
			g_perf_query->FlushResults();
	}

	return g_perf_query->GetQueryResult(type);
}

void VideoBackendHardware::InitializeShared()
{
	VideoCommon_Init();

	s_swapRequested.Clear();
	s_efbAccessRequested.Clear();
	s_perfQueryRequested.Clear();
	s_FifoShuttingDown.Clear();
	memset((void*)&s_beginFieldArgs, 0, sizeof(s_beginFieldArgs));
	memset(&s_accessEFBArgs, 0, sizeof(s_accessEFBArgs));
	s_AccessEFBResult = 0;
	m_invalid = false;
}

// Run from the CPU thread
void VideoBackendHardware::DoState(PointerWrap& p)
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

	p.Do(s_swapRequested);
	p.Do(s_efbAccessRequested);
	p.Do(s_beginFieldArgs);
	p.Do(s_accessEFBArgs);
	p.Do(s_AccessEFBResult);
	p.DoMarker("VideoBackendHardware");

	// Refresh state.
	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		m_invalid = true;
		RecomputeCachedArraybases();

		// Clear all caches that touch RAM
		// (? these don't appear to touch any emulation state that gets saved. moved to on load only.)
		VertexLoaderManager::MarkAllDirty();
	}
}

void VideoBackendHardware::CheckInvalidState()
{
	if (m_invalid)
	{
		m_invalid = false;

		BPReload();
		TextureCache::Invalidate();
	}
}

void VideoBackendHardware::PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	Fifo_PauseAndLock(doLock, unpauseOnUnlock);
}


void VideoBackendHardware::RunLoop(bool enable)
{
	VideoCommon_RunLoop(enable);
}

void VideoFifo_CheckAsyncRequest()
{
	VideoFifo_CheckSwapRequest();
	VideoFifo_CheckEFBAccess();
	VideoFifo_CheckPerfQueryRequest();
}

void VideoBackendHardware::Video_GatherPipeBursted()
{
	CommandProcessor::GatherPipeBursted();
}

bool VideoBackendHardware::Video_IsPossibleWaitingSetDrawDone()
{
	return CommandProcessor::isPossibleWaitingSetDrawDone;
}

void VideoBackendHardware::RegisterCPMMIO(MMIO::Mapping* mmio, u32 base)
{
	CommandProcessor::RegisterMMIO(mmio, base);
}

