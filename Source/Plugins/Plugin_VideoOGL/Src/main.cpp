// Copyright (C) 2003 Dolphin Project.

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



// OpenGL Plugin Documentation
/* 

1.1 Display settings

Internal and fullscreen resolution: Since the only internal resolutions allowed
are also fullscreen resolution allowed by the system there is only need for one
resolution setting that applies to both the internal resolution and the
fullscreen resolution.  - Apparently no, someone else doesn't agree

Todo: Make the internal resolution option apply instantly, currently only the
native and 2x option applies instantly. To do this we need to be able to change
the reinitialize FramebufferManager:Init() while a game is running.

1.2 Screenshots


The screenshots should be taken from the internal representation of the picture
regardless of what the current window size is. Since AA and wireframe is
applied together with the picture resizing this rule is not currently applied
to AA or wireframe pictures, they are instead taken from whatever the window
size is.

Todo: Render AA and wireframe to a separate picture used for the screenshot in
addition to the one for display.

1.3 AA

Make AA apply instantly during gameplay if possible

*/

#include "Globals.h"
#include "LogManager.h"
#include "Thread.h"
#include "Atomic.h"

#include <cstdarg>

#ifdef _WIN32
#include "OS/Win32.h"
#endif

#if defined(HAVE_WX) && HAVE_WX
#include "GUI/ConfigDlg.h"
GFXConfigDialogOGL *m_ConfigFrame = NULL;
#include "Debugger/Debugger.h"
#endif // HAVE_WX

#include "VideoConfig.h"
#include "LookUpTables.h"
#include "ImageWrite.h"
#include "Render.h"
#include "GLUtil.h"
#include "Fifo.h"
#include "OpcodeDecoding.h"
#include "TextureCache.h"
#include "BPStructs.h"
#include "VertexLoader.h"
#include "VertexLoaderManager.h"
#include "VertexManager.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "VertexShaderManager.h"
#include "XFB.h"
#include "XFBConvert.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "TextureConverter.h"
#include "PostProcessing.h"
#include "OnScreenDisplay.h"
#include "Setup.h"
#include "DLCache.h"

#include "VideoState.h"

SVideoInitialize g_VideoInitialize;
PLUGIN_GLOBALS* globals = NULL;

// Logging
int GLScissorX, GLScissorY, GLScissorW, GLScissorH;

static bool s_PluginInitialized = false;

volatile u32 s_swapRequested = FALSE;
#if defined(HAVE_X11) && HAVE_X11
static volatile u32 s_doStateRequested = FALSE;
#endif
static u32 s_efbAccessRequested = FALSE;
static volatile u32 s_FifoShuttingDown = FALSE;

static volatile struct
{
	u32 xfbAddr;
	FieldType field;
	u32 fbWidth;
	u32 fbHeight;
} s_beginFieldArgs;

static volatile EFBAccessType s_AccessEFBType;

bool IsD3D()
{
	return false;
}

void GetDllInfo (PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_VIDEO;
#ifdef DEBUGFAST
	sprintf(_PluginInfo->Name, "Dolphin OpenGL (DebugFast)");
#elif defined _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin OpenGL (Debug)");
#else
	sprintf(_PluginInfo->Name, "Dolphin OpenGL");
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager*)globals->logManager);
}

// This is used for the functions right below here which use wxwidgets
#if defined(HAVE_WX) && HAVE_WX
#ifdef _WIN32
	WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
	extern HINSTANCE g_hInstance;
#endif
#endif

void *DllDebugger(void *_hParent, bool Show)
{
#if defined(HAVE_WX) && HAVE_WX
	return new GFXDebuggerOGL((wxWindow *)_hParent);
#else
	return NULL;
#endif
}

void DllConfig(void *_hParent)
{
	g_Config.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_opengl.ini").c_str());
	g_Config.GameIniLoad(globals->game_ini);
	g_Config.UpdateProjectionHack();
	UpdateActiveConfig();
#if defined(HAVE_WX) && HAVE_WX
	m_ConfigFrame = new GFXConfigDialogOGL((wxWindow *)_hParent);

	m_ConfigFrame->CreateGUIControls();
	m_ConfigFrame->ShowModal();
	m_ConfigFrame->Destroy();
#endif
}

void Initialize(void *init)
{
	frameCount = 0;
	SVideoInitialize *_pVideoInitialize = (SVideoInitialize*)init;
	// Create a shortcut to _pVideoInitialize that can also update it
	g_VideoInitialize = *(_pVideoInitialize);
	InitXFBConvTables();

	g_Config.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_opengl.ini").c_str());
	g_Config.GameIniLoad(globals->game_ini);

	g_Config.UpdateProjectionHack();
	UpdateActiveConfig();

	if (!OpenGL_Create(g_VideoInitialize, 640, 480))
	{
		g_VideoInitialize.pLog("Renderer::Create failed\n", TRUE);
		return;
	}

	_pVideoInitialize->pPeekMessages = g_VideoInitialize.pPeekMessages;
	_pVideoInitialize->pUpdateFPSDisplay = g_VideoInitialize.pUpdateFPSDisplay;

	// Now the window handle is written
	_pVideoInitialize->pWindowHandle = g_VideoInitialize.pWindowHandle;

	OSD::AddMessage("Dolphin OpenGL Video Plugin.", 5000);
	s_PluginInitialized = true;
}

static volatile struct
{
	unsigned char **ptr;
	int mode;
} s_doStateArgs;

// Run from the GPU thread on X11, CPU thread on the rest
static void check_DoState() {
#if defined(HAVE_X11) && HAVE_X11
	if (Common::AtomicLoadAcquire(s_doStateRequested))
	{
#endif
		// Clear all caches that touch RAM
		TextureCache::Invalidate(false);
		VertexLoaderManager::MarkAllDirty();

		PointerWrap p(s_doStateArgs.ptr, s_doStateArgs.mode);
		VideoCommon_DoState(p);

		// Refresh state.
		if (s_doStateArgs.mode == PointerWrap::MODE_READ)
		{
			BPReload();
			RecomputeCachedArraybases();
		}

#if defined(HAVE_X11) && HAVE_X11
		Common::AtomicStoreRelease(s_doStateRequested, FALSE);
	}
#endif
}

// Run from the CPU thread
void DoState(unsigned char **ptr, int mode)
{
	s_doStateArgs.ptr = ptr;
	s_doStateArgs.mode = mode;
#if defined(HAVE_X11) && HAVE_X11
	Common::AtomicStoreRelease(s_doStateRequested, TRUE);
	if (g_VideoInitialize.bOnThread)
	{
		while (Common::AtomicLoadAcquire(s_doStateRequested) && !s_FifoShuttingDown)
			//Common::SleepCurrentThread(1);
			Common::YieldCPU();
	}
	else
#endif
		check_DoState();
}

void EmuStateChange(PLUGIN_EMUSTATE newState)
{
	Fifo_RunLoop((newState == PLUGIN_EMUSTATE_PLAY) ? true : false);
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void Video_Prepare(void)
{
	OpenGL_MakeCurrent();
	if (!Renderer::Init()) {
		g_VideoInitialize.pLog("Renderer::Create failed\n", TRUE);
		PanicAlert("Can't create opengl renderer. You might be missing some required opengl extensions, check the logs for more info");
		exit(1);
	}

	s_efbAccessRequested = FALSE;
	s_FifoShuttingDown = FALSE;
	s_swapRequested = FALSE;

	CommandProcessor::Init();
	PixelEngine::Init();

	TextureCache::Init();

	BPInit();
	VertexManager::Init();
	Fifo_Init(); // must be done before OpcodeDecoder_Init()
	OpcodeDecoder_Init();
	VertexShaderCache::Init();
	VertexShaderManager::Init();
	PixelShaderCache::Init();
	PixelShaderManager::Init();
	PostProcessing::Init();
	GL_REPORT_ERRORD();
	VertexLoaderManager::Init();
	TextureConverter::Init();
	DLCache::Init();

	// Notify the core that the video plugin is ready
	g_VideoInitialize.pCoreMessage(WM_USER_CREATE);

	s_PluginInitialized = true;
	INFO_LOG(VIDEO, "Video plugin initialized.");
}

void Shutdown()
{
	s_PluginInitialized = false;

	s_efbAccessRequested = FALSE;
	s_FifoShuttingDown = FALSE;
	s_swapRequested = FALSE;
	DLCache::Shutdown();
	Fifo_Shutdown();
	PostProcessing::Shutdown();

	// The following calls are NOT Thread Safe
	// And need to be called from the video thread
	TextureConverter::Shutdown();
	VertexLoaderManager::Shutdown();
	VertexShaderCache::Shutdown();
	VertexShaderManager::Shutdown();
	PixelShaderManager::Shutdown();
	PixelShaderCache::Shutdown();
	VertexManager::Shutdown();
	TextureCache::Shutdown();
	OpcodeDecoder_Shutdown();
	Renderer::Shutdown();
	OpenGL_Shutdown();
}

// Enter and exit the video loop
void Video_EnterLoop()
{
	Fifo_EnterLoop(g_VideoInitialize);
}

void Video_ExitLoop()
{
	Fifo_ExitLoop();
	s_FifoShuttingDown = TRUE;
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
			Renderer::Swap(s_beginFieldArgs.xfbAddr, s_beginFieldArgs.field, s_beginFieldArgs.fbWidth, s_beginFieldArgs.fbHeight,rc);
			Common::AtomicStoreRelease(s_swapRequested, FALSE);
		}
	}
}

static inline bool addrRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
	return !((aLower >= bUpper) || (bLower >= aUpper));
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
		if (g_VideoInitialize.bOnThread)
		{
			while (Common::AtomicLoadAcquire(s_swapRequested) && !s_FifoShuttingDown)
				//Common::SleepCurrentThread(1);
				Common::YieldCPU();
		}
		else
			VideoFifo_CheckSwapRequest();
		s_beginFieldArgs.xfbAddr = xfbAddr;
		s_beginFieldArgs.field = field;
		s_beginFieldArgs.fbWidth = fbWidth;
		s_beginFieldArgs.fbHeight = fbHeight;

		Common::AtomicStoreRelease(s_swapRequested, TRUE);
	}
}

// Run from the CPU thread (from VideoInterface.cpp)
void Video_EndField()
{
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

static struct
{
	EFBAccessType type;
	u32 x;
	u32 y;
	u32 Data;
} s_accessEFBArgs;

static u32 s_AccessEFBResult = 0;

void VideoFifo_CheckEFBAccess()
{
	if (Common::AtomicLoadAcquire(s_efbAccessRequested))
	{
		s_AccessEFBResult = Renderer::AccessEFB(s_accessEFBArgs.type, s_accessEFBArgs.x, s_accessEFBArgs.y);

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

void VideoFifo_CheckAsyncRequest() {
	VideoFifo_CheckSwapRequest();
	VideoFifo_CheckEFBAccess();
#if defined(HAVE_X11) && HAVE_X11
	check_DoState();
#endif
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
