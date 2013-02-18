// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


#include "Common.h"

#if defined(HAVE_WX) && HAVE_WX
#include "VideoConfigDialog.h"
#endif // HAVE_WX


#include "SWCommandProcessor.h"
#include "OpcodeDecoder.h"
#include "SWVideoConfig.h"
#include "SWPixelEngine.h"
#include "BPMemLoader.h"
#include "XFMemLoader.h"
#include "Clipper.h"
#include "Rasterizer.h"
#include "SWRenderer.h"
#include "HwRasterizer.h"
#include "LogManager.h"
#include "EfbInterface.h"
#include "DebugUtil.h"
#include "FileUtil.h"
#include "VideoBackend.h"
#include "Core.h"

#define VSYNC_ENABLED 0

namespace SW
{

static volatile bool fifoStateRun = false;
static volatile bool emuRunningState = false;
static std::mutex m_csSWVidOccupied;


std::string VideoSoftware::GetName()
{
	return _trans("Software Renderer");
}

void *DllDebugger(void *_hParent, bool Show)
{
	return NULL;
}

void VideoSoftware::ShowConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	VideoConfigDialog diag((wxWindow*)_hParent, "Software", "gfx_software");
	diag.ShowModal();
#endif
}

bool VideoSoftware::Initialize(void *&window_handle)
{
	g_SWVideoConfig.Load((File::GetUserPath(D_CONFIG_IDX) + "gfx_software.ini").c_str());
	InitInterface();

	if (!GLInterface->Create(window_handle))
	{
		INFO_LOG(VIDEO, "%s", "SWRenderer::Create failed\n");
		return false;
	}

	InitBPMemory();
	InitXFMemory();
	SWCommandProcessor::Init();
	SWPixelEngine::Init();
	OpcodeDecoder::Init();
	Clipper::Init();
	Rasterizer::Init();
	HwRasterizer::Init();
	SWRenderer::Init();
	DebugUtil::Init();

	return true;
}

void VideoSoftware::DoState(PointerWrap&)
{
	// NYI
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
	HwRasterizer::Shutdown();
	SWRenderer::Shutdown();
	GLInterface->Shutdown();
}

// This is called after Video_Initialize() from the Core
void VideoSoftware::Video_Prepare()
{
	GLInterface->MakeCurrent();
	// Init extension support.
#ifndef USE_GLES
#ifdef __APPLE__
	glewExperimental = 1;
#endif
	if (glewInit() != GLEW_OK) {
		ERROR_LOG(VIDEO, "glewInit() failed!Does your video card support OpenGL 2.x?");
		return;
	}
#endif
	// Handle VSync on/off
	GLInterface->SwapInterval(VSYNC_ENABLED);

	HwRasterizer::Prepare();
	SWRenderer::Prepare();

	INFO_LOG(VIDEO, "Video backend initialized.");
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoSoftware::Video_BeginField(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{	
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoSoftware::Video_EndField()
{
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

bool VideoSoftware::Video_Screenshot(const char *_szFilename)
{
	return false;
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
		g_video_backend->PeekMessages();

		if (!SWCommandProcessor::RunBuffer())
		{
			Common::YieldCPU();
		}

		while (!emuRunningState && fifoStateRun)
		{
			g_video_backend->PeekMessages();
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
void VideoSoftware::Video_AddMessage(const char* pstr, u32 milliseconds)
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

bool VideoSoftware::Video_IsPossibleWaitingSetDrawDone(void)
{
	return false;
}

bool VideoSoftware::Video_IsHiWatermarkActive(void)
{
	return false;
}


void VideoSoftware::Video_AbortFrame(void)
{
}

readFn16 VideoSoftware::Video_CPRead16()
{
	return SWCommandProcessor::Read16;
}
writeFn16 VideoSoftware::Video_CPWrite16()
{
	return SWCommandProcessor::Write16;
}

readFn16  VideoSoftware::Video_PERead16()
{
	return SWPixelEngine::Read16;
}
writeFn16 VideoSoftware::Video_PEWrite16()
{
	return SWPixelEngine::Write16;
}
writeFn32 VideoSoftware::Video_PEWrite32()
{
	return SWPixelEngine::Write32;
}


// Draw messages on top of the screen
unsigned int VideoSoftware::PeekMessages()
{
	return GLInterface->PeekMessages();
}

// Show the current FPS
void VideoSoftware::UpdateFPSDisplay(const char *text)
{
	char temp[100];
	snprintf(temp, sizeof temp, "%s | Software | %s", scm_rev_str, text);
	GLInterface->UpdateFPSDisplay(temp);
}

}
