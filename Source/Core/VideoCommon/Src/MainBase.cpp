
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


void EmuStateChange(PLUGIN_EMUSTATE newState)
{
	Fifo_RunLoop((newState == PLUGIN_EMUSTATE_PLAY) ? true : false);
}

// Enter and exit the video loop
void Video_EnterLoop()
{
	Fifo_EnterLoop(g_VideoInitialize);
}

void Video_ExitLoop()
{
	Fifo_ExitLoop();
	s_FifoShuttingDown = true;
}

void Video_SetRendering(bool bEnabled)
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
			Common::AtomicStoreRelease(s_swapRequested, FALSE);
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
void Video_BeginField(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{
	if (s_PluginInitialized && g_ActiveConfig.bUseXFB)
	{
		if (!g_VideoInitialize.bOnThread)
			VideoFifo_CheckSwapRequest();
		s_beginFieldArgs.xfbAddr = xfbAddr;
		s_beginFieldArgs.field = field;
		s_beginFieldArgs.fbWidth = fbWidth;
		s_beginFieldArgs.fbHeight = fbHeight;
	}
}

// Run from the CPU thread (from VideoInterface.cpp)
void Video_EndField()
{
	if (s_PluginInitialized)
	{
		Common::AtomicStoreRelease(s_swapRequested, TRUE);
	}
}

void Video_AddMessage(const char* pstr, u32 milliseconds)
{
	OSD::AddMessage(pstr, milliseconds);
}

// Screenshot
void Video_Screenshot(const char *_szFilename)
{
	Renderer::SetScreenshot(_szFilename);
}

void VideoFifo_CheckEFBAccess()
{
	if (Common::AtomicLoadAcquire(s_efbAccessRequested))
	{
		s_AccessEFBResult = g_renderer->AccessEFB(s_accessEFBArgs.type, s_accessEFBArgs.x, s_accessEFBArgs.y, s_accessEFBArgs.Data);

		Common::AtomicStoreRelease(s_efbAccessRequested, FALSE);
	}
}

u32 Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
	if (s_PluginInitialized)
	{
		s_accessEFBArgs.type = type;
		s_accessEFBArgs.x = x;
		s_accessEFBArgs.y = y;
		s_accessEFBArgs.Data = InputData;

		Common::AtomicStoreRelease(s_efbAccessRequested, TRUE);

		if (g_VideoInitialize.bOnThread)
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

static volatile u32 s_doStateRequested = FALSE;
 
static volatile struct
{
	unsigned char **ptr;
	int mode;
} s_doStateArgs;

// Run from the GPU thread on X11, CPU thread on the rest
static void check_DoState() {
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

		Common::AtomicStoreRelease(s_doStateRequested, FALSE);
	}
}

// Run from the CPU thread
void DoState(unsigned char **ptr, int mode)
{
	s_doStateArgs.ptr = ptr;
	s_doStateArgs.mode = mode;
	Common::AtomicStoreRelease(s_doStateRequested, TRUE);
	if (g_VideoInitialize.bOnThread)
	{
		while (Common::AtomicLoadAcquire(s_doStateRequested) && !s_FifoShuttingDown)
			//Common::SleepCurrentThread(1);
			Common::YieldCPU();
	}
	else
	check_DoState();
}

void VideoFifo_CheckAsyncRequest()
{
	VideoFifo_CheckSwapRequest();
	VideoFifo_CheckEFBAccess();
	check_DoState();
}

void Video_CommandProcessorRead16(u16& _rReturnValue, const u32 _Address)
{
	CommandProcessor::Read16(_rReturnValue, _Address);
}

void Video_CommandProcessorWrite16(const u16 _Data, const u32 _Address)
{
	CommandProcessor::Write16(_Data, _Address);
}

void Video_PixelEngineRead16(u16& _rReturnValue, const u32 _Address)
{
	PixelEngine::Read16(_rReturnValue, _Address);
}

void Video_PixelEngineWrite16(const u16 _Data, const u32 _Address)
{
	PixelEngine::Write16(_Data, _Address);
}

void Video_PixelEngineWrite32(const u32 _Data, const u32 _Address)
{
	PixelEngine::Write32(_Data, _Address);
}

void Video_GatherPipeBursted(void)
{
	CommandProcessor::GatherPipeBursted();
}

void Video_WaitForFrameFinish(void)
{
	CommandProcessor::WaitForFrameFinish();
}

bool Video_IsFifoBusy(void)
{
	return CommandProcessor::isFifoBusy;
}

void Video_AbortFrame(void)
{
	CommandProcessor::AbortFrame();
}
