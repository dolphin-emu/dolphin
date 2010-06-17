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

#include <tchar.h>
#include <windows.h>
#include <d3dx11.h>

#include <wx/wx.h>
#include <wx/notebook.h>

#include "Common.h"
#include "Atomic.h"
#include "Thread.h"
#include "LogManager.h"

#include "svnrev.h"
#include "resource.h"
#include "main.h"
#include "VideoConfig.h"
#include "Fifo.h"
#include "OpcodeDecoding.h"
#include "TextureCache.h"
#include "BPStructs.h"
#include "VertexManager.h"
#include "VertexLoaderManager.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderCache.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "OnScreenDisplay.h"
#include "DlgSettings.h"
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "W32Util/Misc.h"
#include "EmuWindow.h"
#include "VideoState.h"
#include "XFBConvert.h"
#include "render.h"


#if defined(DEBUGFAST)
#define DLL_PLUGIN_NAME "Dolphin Direct3D 11 (DebugFast)"
#elif defined(_DEBUG)
#define DLL_PLUGIN_NAME "Dolphin Direct3D 11 (Debug)"
#else
#define DLL_PLUGIN_NAME "Dolphin Direct3D 11"
#endif


HINSTANCE g_hInstance = NULL;
SVideoInitialize g_VideoInitialize;
PLUGIN_GLOBALS* globals = NULL;
static bool s_PluginInitialized = false;

volatile u32 s_swapRequested = FALSE;
static u32 s_efbAccessRequested = FALSE;
static volatile u32 s_FifoShuttingDown = FALSE;
static bool ForceSwap = true;

static volatile struct
{
	u32 xfbAddr;
	FieldType field;
	u32 fbWidth;
	u32 fbHeight;
} s_beginFieldArgs;

static volatile EFBAccessType s_AccessEFBType;

bool HandleDisplayList(u32 address, u32 size)
{
	return false;
}

bool IsD3D()
{
	return true;
}

bool IsD3D9()
{
	return false;
}

bool IsD3D11()
{
	return true;
}

// This is used for the functions right below here which use wxwidgets
#if defined(HAVE_WX) && HAVE_WX
#ifdef _WIN32
	WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

wxWindow* GetParentedWxWindow(HWND Parent)
{
#ifdef _WIN32
	wxSetInstance((HINSTANCE)g_hInstance);
#endif
	wxWindow* win = new wxWindow();
#ifdef _WIN32
	win->SetHWND((WXHWND)Parent);
	win->AdoptAttributesFromHWND();
#endif
	return win;
}
#endif

void DllDebugger(HWND _hParent, bool Show)
{
}

#if defined(HAVE_WX) && HAVE_WX
	class wxDLLApp : public wxApp
	{
		bool OnInit()
		{
			return true;
		}
	};
	IMPLEMENT_APP_NO_MAIN(wxDLLApp) 
	WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
#if defined(HAVE_WX) && HAVE_WX
			wxSetInstance((HINSTANCE)hinstDLL);
			wxInitialize();
#endif
		}
		break;
	case DLL_PROCESS_DETACH:
#if defined(HAVE_WX) && HAVE_WX
		wxUninitialize();
#endif
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}

unsigned int Callback_PeekMessages()
{
	MSG msg;
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return FALSE;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return TRUE;
}


void UpdateFPSDisplay(const char* text)
{
	char temp[512];
	sprintf_s(temp, 512, "SVN R%i: DX11: %s", SVN_REV, text);
    SetWindowTextA(EmuWindow::GetWnd(), temp);
}

void GetDllInfo (PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_VIDEO;
	sprintf_s(_PluginInfo->Name, 100, DLL_PLUGIN_NAME);
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager*)globals->logManager);
}

void DllAbout(HWND _hParent)
{
	MessageBoxA(NULL, "DllAbout not implemented, how did you come here? Anyway, report this to the devs.", "Error!", MB_OK);
}

void DllConfig(HWND _hParent)
{
	DlgSettings_Show(g_hInstance, _hParent);
}

void Initialize(void* init)
{
	frameCount = 0;
	SVideoInitialize* _pVideoInitialize = (SVideoInitialize*)init;
	g_VideoInitialize = *_pVideoInitialize;
	InitXFBConvTables();

	g_Config.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx11.ini").c_str());
	g_Config.GameIniLoad(globals->game_ini);
	UpdateActiveConfig();

	g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create((HWND)g_VideoInitialize.pWindowHandle, g_hInstance, _T("Loading - Please wait."));
	if (g_VideoInitialize.pWindowHandle == NULL)
	{
		ERROR_LOG(VIDEO, "An error has occurred while trying to create the window.");
		return;
	}

	g_VideoInitialize.pPeekMessages = &Callback_PeekMessages;
	g_VideoInitialize.pUpdateFPSDisplay = &UpdateFPSDisplay;

    _pVideoInitialize->pPeekMessages = g_VideoInitialize.pPeekMessages;
    _pVideoInitialize->pUpdateFPSDisplay = g_VideoInitialize.pUpdateFPSDisplay;
    _pVideoInitialize->pWindowHandle = g_VideoInitialize.pWindowHandle;

	OSD::AddMessage("Dolphin Direct3D 11 Video Plugin.", 5000);
	s_PluginInitialized = true;
}

void Video_Prepare()
{
	s_efbAccessRequested = FALSE;
	s_FifoShuttingDown = FALSE;
	s_swapRequested = FALSE;
	ForceSwap = true;
	Renderer::Init();
	TextureCache::Init();
	BPInit();
	VertexManager::Init();
	Fifo_Init();
	VertexLoaderManager::Init();
	OpcodeDecoder_Init();
	VertexShaderCache::Init();
	VertexShaderManager::Init();
	PixelShaderCache::Init();
	PixelShaderManager::Init();
    CommandProcessor::Init();
    PixelEngine::Init();
	D3D::InitUtils();

	// tell the host that the window is ready
	g_VideoInitialize.pCoreMessage(WM_USER_CREATE);
}

void Shutdown()
{
	s_efbAccessRequested = FALSE;
	s_FifoShuttingDown = FALSE;
	s_swapRequested = FALSE;
	D3D::ShutdownUtils();
	CommandProcessor::Shutdown();
	PixelShaderManager::Shutdown();
	PixelShaderCache::Shutdown();
	VertexShaderManager::Shutdown();
	VertexShaderCache::Shutdown();
	OpcodeDecoder_Shutdown();
	VertexLoaderManager::Shutdown();
	Fifo_Shutdown();
	VertexManager::Shutdown();
	TextureCache::Shutdown();
	Renderer::Shutdown();
	EmuWindow::Close();
	s_PluginInitialized = false;
}

void DoState(unsigned char** ptr, int mode)
{
	// clear texture cache because it might have written to RAM
	CommandProcessor::FifoCriticalEnter();
	TextureCache::Invalidate(false);
	CommandProcessor::FifoCriticalLeave();
	// no need to clear shader caches
	PointerWrap p(ptr, mode);
	VideoCommon_DoState(p);
}

void EmuStateChange(PLUGIN_EMUSTATE newState)
{
	Fifo_RunLoop((newState == PLUGIN_EMUSTATE_PLAY) ? true : false);
}

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

// run from the graphics thread
void VideoFifo_CheckSwapRequest()
{
	if (Common::AtomicLoadAcquire(s_swapRequested))
	{
		if (ForceSwap || g_ActiveConfig.bUseXFB)
		{
			Renderer::Swap(s_beginFieldArgs.xfbAddr, s_beginFieldArgs.field, s_beginFieldArgs.fbWidth, s_beginFieldArgs.fbHeight);
		}
		Common::AtomicStoreRelease(s_swapRequested, FALSE);
	}
}

inline bool addrRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
	return !((aLower >= bUpper) || (bLower >= aUpper));
}

// Run from the graphics thread
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	if (Common::AtomicLoadAcquire(s_swapRequested) && g_ActiveConfig.bUseXFB)
	{
		u32 aLower = xfbAddr;
		u32 aUpper = xfbAddr + 2 * fbWidth * fbHeight;
		u32 bLower = s_beginFieldArgs.xfbAddr;
		u32 bUpper = s_beginFieldArgs.xfbAddr + 2 * s_beginFieldArgs.fbWidth * s_beginFieldArgs.fbHeight;

		if (addrRangesOverlap(aLower, aUpper, bLower, bUpper))
			VideoFifo_CheckSwapRequest();
	}
	ForceSwap = false;
}

// Run from the CPU thread (from VideoInterface.cpp)
void Video_BeginField(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{
	if (s_PluginInitialized && g_ActiveConfig.bUseXFB)
	{
		s_beginFieldArgs.xfbAddr = xfbAddr;
		s_beginFieldArgs.field = field;
		s_beginFieldArgs.fbWidth = fbWidth;
		s_beginFieldArgs.fbHeight = fbHeight;

		Common::AtomicStoreRelease(s_swapRequested, TRUE);
		if (g_VideoInitialize.bOnThread)
		{
			while (Common::AtomicLoadAcquire(s_swapRequested) && !s_FifoShuttingDown)
				//Common::SleepCurrentThread(1);
				Common::YieldCPU();
		}
		else
			VideoFifo_CheckSwapRequest();
	}
}

void Video_EndField()
{
}

void Video_AddMessage(const char* pstr, u32 milliseconds)
{
	OSD::AddMessage(pstr, milliseconds);
}

HRESULT ScreenShot(const char* filename)
{
	return S_OK;
}

void Video_Screenshot(const char* _szFilename)
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

inline void Video_GatherPipeBursted(void)
{
    CommandProcessor::GatherPipeBursted();
}

void Video_WaitForFrameFinish(void)
{
    CommandProcessor::WaitForFrameFinish();
}
