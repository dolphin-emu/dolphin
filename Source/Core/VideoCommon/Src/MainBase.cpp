
#include "MainBase.h"
#include "VideoState.h"
#include "VideoConfig.h"
#include "RenderBase.h"
#include "FramebufferManagerBase.h"
#include "TextureCacheBase.h"
#include "VertexLoaderManager.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "Atomic.h"
#include "Fifo.h"
#include "BPStructs.h"
#include "OnScreenDisplay.h"
#include "VideoBackendBase.h"
#include "ConfigManager.h"

bool s_PluginInitialized = false;

volatile u32 s_swapRequested = false;
u32 s_efbAccessRequested = false;
volatile u32 s_FifoShuttingDown = false;

static volatile struct
{
	u32 xfbAddr;
	FieldType field;
	u32 fbWidth;
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

void VideoBackendHLE::EmuStateChange(EMUSTATE_CHANGE newState)
{
	Fifo_RunLoop((newState == EMUSTATE_CHANGE_PLAY) ? true : false);
}

// Enter and exit the video loop
void VideoBackendHLE::Video_EnterLoop()
{
	Fifo_EnterLoop();
}

void VideoBackendHLE::Video_ExitLoop()
{
	Fifo_ExitLoop();
	s_FifoShuttingDown = true;
}

void VideoBackendHLE::Video_SetRendering(bool bEnabled)
{
	Fifo_SetRendering(bEnabled);
}

// Run from the graphics thread (from Fifo.cpp)
void VideoFifo_CheckSwapRequest()
{
	if(g_ActiveConfig.bUseXFB)
	{
		if (Common::AtomicLoadAcquire(s_swapRequested))
		{
			EFBRectangle rc;
			g_renderer->Swap(s_beginFieldArgs.xfbAddr, s_beginFieldArgs.field, s_beginFieldArgs.fbWidth, s_beginFieldArgs.fbHeight,rc);
			Common::AtomicStoreRelease(s_swapRequested, false);
		}
	}
}

// Run from the graphics thread (from Fifo.cpp)
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	if (g_ActiveConfig.bUseXFB)
	{
		if(Common::AtomicLoadAcquire(s_swapRequested))
		{
			u32 aLower = xfbAddr;
			u32 aUpper = xfbAddr + 2 * fbWidth * fbHeight;
			u32 bLower = s_beginFieldArgs.xfbAddr;
			u32 bUpper = s_beginFieldArgs.xfbAddr + 2 * s_beginFieldArgs.fbWidth * s_beginFieldArgs.fbHeight;

			if (addrRangesOverlap(aLower, aUpper, bLower, bUpper))
				VideoFifo_CheckSwapRequest();
		}
	}
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackendHLE::Video_BeginField(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{
	if (s_PluginInitialized && g_ActiveConfig.bUseXFB)
	{
		if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread)
			VideoFifo_CheckSwapRequest();
		s_beginFieldArgs.xfbAddr = xfbAddr;
		s_beginFieldArgs.field = field;
		s_beginFieldArgs.fbWidth = fbWidth;
		s_beginFieldArgs.fbHeight = fbHeight;
	}
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackendHLE::Video_EndField()
{
	if (s_PluginInitialized)
	{
		Common::AtomicStoreRelease(s_swapRequested, true);
	}
}

void VideoBackendHLE::Video_AddMessage(const char* pstr, u32 milliseconds)
{
	OSD::AddMessage(pstr, milliseconds);
}

void VideoBackendHLE::Video_ClearMessages()
{
	OSD::ClearMessages();
}

// Screenshot
bool VideoBackendHLE::Video_Screenshot(const char *_szFilename)
{
	Renderer::SetScreenshot(_szFilename);
	return true;
}

void VideoFifo_CheckEFBAccess()
{
	if (Common::AtomicLoadAcquire(s_efbAccessRequested))
	{
		s_AccessEFBResult = g_renderer->AccessEFB(s_accessEFBArgs.type, s_accessEFBArgs.x, s_accessEFBArgs.y, s_accessEFBArgs.Data);

		Common::AtomicStoreRelease(s_efbAccessRequested, false);
	}
}

u32 VideoBackendHLE::Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
	if (s_PluginInitialized)
	{
		s_accessEFBArgs.type = type;
		s_accessEFBArgs.x = x;
		s_accessEFBArgs.y = y;
		s_accessEFBArgs.Data = InputData;

		Common::AtomicStoreRelease(s_efbAccessRequested, true);

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread)
		{
			while (Common::AtomicLoadAcquire(s_efbAccessRequested) && !s_FifoShuttingDown)
				//Common::SleepCurrentThread(1);
				Common::YieldCPU();
		}
		else
			VideoFifo_CheckEFBAccess();

		return s_AccessEFBResult;
	}

	return 0;
}

static volatile u32 s_doStateRequested = false;
 
static volatile struct
{
	unsigned char **ptr;
	int mode;
} s_doStateArgs;

// Depending on the threading mode (DC/SC) this can be called 
// from either the GPU thread or the CPU thread
void VideoFifo_CheckStateRequest()
{
	if (Common::AtomicLoadAcquire(s_doStateRequested))
	{
		// Clear all caches that touch RAM
		CommandProcessor::FifoCriticalEnter();
		TextureCache::Invalidate(false);
		VertexLoaderManager::MarkAllDirty();
		CommandProcessor::FifoCriticalLeave();

		PointerWrap p(s_doStateArgs.ptr, s_doStateArgs.mode);
		VideoCommon_DoState(p);

		// Refresh state.
		if (s_doStateArgs.mode == PointerWrap::MODE_READ)
		{
			BPReload();
			RecomputeCachedArraybases();
		}

		Common::AtomicStoreRelease(s_doStateRequested, false);
	}
}

// Run from the CPU thread
void VideoBackendHLE::DoState(PointerWrap& p)
{
	s_doStateArgs.ptr = p.ptr;
	s_doStateArgs.mode = p.mode;
	Common::AtomicStoreRelease(s_doStateRequested, true);
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread)
	{
		while (Common::AtomicLoadAcquire(s_doStateRequested) && !s_FifoShuttingDown)
			//Common::SleepCurrentThread(1);
			Common::YieldCPU();
	}
	else
		VideoFifo_CheckStateRequest();
}

void VideoBackendHLE::RunLoop(bool enable)
{
	VideoCommon_RunLoop(enable);
}

void VideoFifo_CheckAsyncRequest()
{
	VideoFifo_CheckSwapRequest();
	VideoFifo_CheckEFBAccess();
}

void VideoBackend::Video_GatherPipeBursted()
{
	CommandProcessor::GatherPipeBursted();
}

void VideoBackendHLE::Video_WaitForFrameFinish()
{
	CommandProcessor::WaitForFrameFinish();
}

bool VideoBackendHLE::Video_IsPossibleWaitingSetDrawDone()
{
	return CommandProcessor::isPossibleWaitingSetDrawDone;
}

void VideoBackendHLE::Video_AbortFrame()
{
	CommandProcessor::AbortFrame();
}
