// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "Common/Atomic.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/VideoInterface.h"

#include "VideoBackends/OGL/GLInterfaceBase.h"
#include "VideoBackends/OGL/GLExtensions/GLExtensions.h"
#include "VideoBackends/Software/BPMemLoader.h"
#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/DebugUtil.h"
#include "VideoBackends/Software/EfbInterface.h"
#include "VideoBackends/Software/HwRasterizer.h"
#include "VideoBackends/Software/OpcodeDecoder.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SWCommandProcessor.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/SWStatistics.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/SWVideoConfig.h"
#include "VideoBackends/Software/VideoBackend.h"
#include "VideoBackends/Software/XFMemLoader.h"

#include "VideoCommon/Fifo.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/XFMemory.h"

#define VSYNC_ENABLED 0

static volatile u32 s_swapRequested = false;

static volatile struct
{
	u32 xfbAddr;
	u32 fbWidth;
	u32 fbHeight;
} s_beginFieldArgs;

namespace SW
{

static volatile bool fifoStateRun = false;
static volatile bool emuRunningState = false;
static std::mutex m_csSWVidOccupied;

std::string VideoSoftware::GetName() const
{
	return "Software Renderer";
}

std::string VideoSoftware::GetDisplayName() const
{
	return "Software Renderer";
}

void VideoSoftware::ShowConfig(void *hParent)
{
	Host_ShowVideoConfig(hParent, GetDisplayName(), "gfx_software");
}

bool VideoSoftware::Initialize(void *window_handle)
{
	g_SWVideoConfig.Load((File::GetUserPath(D_CONFIG_IDX) + "gfx_software.ini").c_str());

	InitInterface();
	GLInterface->SetMode(GLInterfaceMode::MODE_DETECT);
	if (!GLInterface->Create(window_handle))
	{
		INFO_LOG(VIDEO, "GLInterface::Create failed.");
		return false;
	}

	InitBPMemory();
	InitXFMemory();
	SWCommandProcessor::Init();
	PixelEngine::Init();
	OpcodeDecoder::Init();
	Clipper::Init();
	Rasterizer::Init();
	HwRasterizer::Init();
	SWRenderer::Init();
	DebugUtil::Init();

	return true;
}

void VideoSoftware::DoState(PointerWrap& p)
{
	bool software = true;
	p.Do(software);
	if (p.GetMode() == PointerWrap::MODE_READ && software == false)
		// change mode to abort load of incompatible save state.
		p.SetMode(PointerWrap::MODE_VERIFY);

	// TODO: incomplete?
	SWCommandProcessor::DoState(p);
	PixelEngine::DoState(p);
	EfbInterface::DoState(p);
	OpcodeDecoder::DoState(p);
	Clipper::DoState(p);
	p.Do(xfmem);
	p.Do(bpmem);
	p.DoPOD(swstats);

	// CP Memory
	DoCPState(p);
}

void VideoSoftware::CheckInvalidState()
{
	// there is no state to invalidate
}

void VideoSoftware::PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	if (doLock)
	{
		EmuStateChange(EMUSTATE_CHANGE_PAUSE);
		if (!Core::IsGPUThread())
			m_csSWVidOccupied.lock();
	}
	else
	{
		if (unpauseOnUnlock)
			EmuStateChange(EMUSTATE_CHANGE_PLAY);
		if (!Core::IsGPUThread())
			m_csSWVidOccupied.unlock();
	}
}

void VideoSoftware::RunLoop(bool enable)
{
	emuRunningState = enable;
}

void VideoSoftware::EmuStateChange(EMUSTATE_CHANGE newState)
{
	emuRunningState = (newState == EMUSTATE_CHANGE_PLAY) ? true : false;
}

void VideoSoftware::Shutdown()
{
	// TODO: should be in Video_Cleanup
	HwRasterizer::Shutdown();
	SWRenderer::Shutdown();
	DebugUtil::Shutdown();

	// Do our OSD callbacks
	OSD::DoCallbacks(OSD::OSD_SHUTDOWN);

	GLInterface->Shutdown();
}

void VideoSoftware::Video_Cleanup()
{
	GLInterface->ClearCurrent();
}

// This is called after Video_Initialize() from the Core
void VideoSoftware::Video_Prepare()
{
	GLInterface->MakeCurrent();

	// Init extension support.
	if (!GLExtensions::Init())
	{
		ERROR_LOG(VIDEO, "GLExtensions::Init failed!Does your video card support OpenGL 2.0?");
		return;
	}

	// Handle VSync on/off
	GLInterface->SwapInterval(VSYNC_ENABLED);

	// Do our OSD callbacks
	OSD::DoCallbacks(OSD::OSD_INIT);

	HwRasterizer::Prepare();
	SWRenderer::Prepare();

	INFO_LOG(VIDEO, "Video backend initialized.");
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoSoftware::Video_BeginField(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight)
{
	// XXX: fbStride should be implemented properly here
	// If stride isn't implemented then there are problems with XFB
	// Animal Crossing is a good example for this.
	s_beginFieldArgs.xfbAddr = xfbAddr;
	s_beginFieldArgs.fbWidth = fbWidth;
	s_beginFieldArgs.fbHeight = fbHeight;
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoSoftware::Video_EndField()
{
	// Techincally the XFB is continually rendered out scanline by scanline between
	// BeginField and EndFeild, We could possibly get away with copying out the whole thing
	// at BeginField for less lag, but for the safest emulation we run it here.

	if (g_bSkipCurrentFrame || s_beginFieldArgs.xfbAddr == 0)
	{
		swstats.frameCount++;
		swstats.ResetFrame();
		Core::Callback_VideoCopiedToXFB(false);
		return;
	}
	if (!g_SWVideoConfig.bHwRasterizer)
	{
		if (!g_SWVideoConfig.bBypassXFB)
		{
			EfbInterface::yuv422_packed *xfb = (EfbInterface::yuv422_packed *) Memory::GetPointer(s_beginFieldArgs.xfbAddr);

			SWRenderer::UpdateColorTexture(xfb, s_beginFieldArgs.fbWidth, s_beginFieldArgs.fbHeight);
		}
	}

	// Ideally we would just move all the OpenGL context stuff to the CPU thread,
	// but this gets messy when the hardware rasterizer is enabled.
	// And neobrain loves his hardware rasterizer.

	// If BypassXFB has already done a swap (cf. EfbCopy::CopyToXfb), skip this.
	if (!g_SWVideoConfig.bBypassXFB)
	{
		// Dump frame if needed
		DebugUtil::OnFrameEnd(s_beginFieldArgs.fbWidth, s_beginFieldArgs.fbHeight);

		// If we are in dual core mode, notify the GPU thread about the new color texture.
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread)
			Common::AtomicStoreRelease(s_swapRequested, true);
		else
			SWRenderer::Swap(s_beginFieldArgs.fbWidth, s_beginFieldArgs.fbHeight);
	}
}

u32 VideoSoftware::Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
	u32 value = 0;

	switch (type)
	{
	case PEEK_Z:
		{
			value = EfbInterface::GetDepth(x, y);
			break;
		}

	case POKE_Z:
		break;

	case PEEK_COLOR:
		{
			u32 color = 0;
			EfbInterface::GetColor(x, y, (u8*)&color);

			// rgba to argb
			value = (color >> 8) | (color & 0xff) << 24;
			break;
		}

	case POKE_COLOR:
		break;
	}

	return value;
}

u32 VideoSoftware::Video_GetQueryResult(PerfQueryType type)
{
	return EfbInterface::perf_values[type];
}

bool VideoSoftware::Video_Screenshot(const std::string& filename)
{
	SWRenderer::SetScreenshot(filename.c_str());
	return true;
}

// Run from the graphics thread
static void VideoFifo_CheckSwapRequest()
{
	if (Common::AtomicLoadAcquire(s_swapRequested))
	{
		SWRenderer::Swap(s_beginFieldArgs.fbWidth, s_beginFieldArgs.fbHeight);
		Common::AtomicStoreRelease(s_swapRequested, false);
	}
}

// -------------------------------
// Enter and exit the video loop
// -------------------------------
void VideoSoftware::Video_EnterLoop()
{
	std::lock_guard<std::mutex> lk(m_csSWVidOccupied);
	fifoStateRun = true;

	while (fifoStateRun)
	{
		VideoFifo_CheckSwapRequest();
		g_video_backend->PeekMessages();

		if (!SWCommandProcessor::RunBuffer())
		{
			Common::YieldCPU();
		}

		while (!emuRunningState && fifoStateRun)
		{
			g_video_backend->PeekMessages();
			VideoFifo_CheckSwapRequest();
			m_csSWVidOccupied.unlock();
			Common::SleepCurrentThread(1);
			m_csSWVidOccupied.lock();
		}
	}
}

void VideoSoftware::Video_ExitLoop()
{
	fifoStateRun = false;
}

// TODO : could use the OSD class in video common, we would need to implement the Renderer class
//        however most of it is useless for the SW backend so we could as well move it to its own class
void VideoSoftware::Video_AddMessage(const std::string& msg, u32 milliseconds)
{
}
void VideoSoftware::Video_ClearMessages()
{
}

void VideoSoftware::Video_SetRendering(bool bEnabled)
{
	SWCommandProcessor::SetRendering(bEnabled);
}

void VideoSoftware::Video_GatherPipeBursted()
{
	SWCommandProcessor::GatherPipeBursted();
}

bool VideoSoftware::Video_IsPossibleWaitingSetDrawDone()
{
	return false;
}

void VideoSoftware::RegisterCPMMIO(MMIO::Mapping* mmio, u32 base)
{
	SWCommandProcessor::RegisterMMIO(mmio, base);
}

// Draw messages on top of the screen
unsigned int VideoSoftware::PeekMessages()
{
	return GLInterface->PeekMessages();
}

}
