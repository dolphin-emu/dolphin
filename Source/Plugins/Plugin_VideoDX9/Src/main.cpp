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

#include "Common.h"
#include "Atomic.h"
#include "Thread.h"
#include "LogManager.h"
#include "debugger/debugger.h"

#if defined(HAVE_WX) && HAVE_WX
#include "DlgSettings.h"
GFXConfigDialogDX *m_ConfigFrame = NULL;
#endif // HAVE_WX

#if defined(HAVE_WX) && HAVE_WX
#include "Debugger/Debugger.h"
#endif // HAVE_WX

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
#include "EmuWindow.h"
#include "VideoState.h"
#include "XFBConvert.h"
#include "render.h"

HINSTANCE g_hInstance = NULL;
SVideoInitialize g_VideoInitialize;
PLUGIN_GLOBALS* globals = NULL;
static bool s_PluginInitialized = false;

volatile u32 s_swapRequested = FALSE;
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

bool HandleDisplayList(u32 address, u32 size)
{
	return false;
}

bool IsD3D()
{
	return true;
}

// This is used for the functions right below here which use wxwidgets
#if defined(HAVE_WX) && HAVE_WX
WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

void *DllDebugger(void *_hParent, bool Show)
{
#if defined(HAVE_WX) && HAVE_WX
	return new GFXDebuggerDX9((wxWindow *)_hParent);
#else
	return NULL;
#endif
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


void UpdateFPSDisplay(const char *text)
{
	TCHAR temp[512];
	swprintf_s(temp, 512, _T("SVN R%s: DX9: %hs"), svn_rev_str, text);
	SetWindowText(EmuWindow::GetWnd(), temp);
}

void GetDllInfo (PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_VIDEO;
#ifdef DEBUGFAST
	sprintf_s(_PluginInfo->Name, 100, "Dolphin Direct3D9 (DebugFast)");
#else
#ifndef _DEBUG
	sprintf_s(_PluginInfo->Name, 100, "Dolphin Direct3D9");
#else
	sprintf_s(_PluginInfo->Name, 100, "Dolphin Direct3D9 (Debug)");
#endif
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager*)globals->logManager);
}

void DllAbout(HWND _hParent)
{
	//DialogBox(g_hInstance,(LPCTSTR)IDD_ABOUT,_hParent,(DLGPROC)AboutProc);
}

void DllConfig(void *_hParent)
{
	// If not initialized, only init D3D so we can enumerate resolutions.
	if (!s_PluginInitialized)
		D3D::Init();
	g_Config.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx9.ini").c_str());
	g_Config.GameIniLoad(globals->game_ini);
	UpdateActiveConfig();
#if defined(HAVE_WX) && HAVE_WX
	m_ConfigFrame = new GFXConfigDialogDX((wxWindow *)_hParent);

	m_ConfigFrame->CreateGUIControls();
	m_ConfigFrame->ShowModal();
	m_ConfigFrame->Destroy();
#endif
	if (!s_PluginInitialized)
		D3D::Shutdown();
}

void Initialize(void *init)
{
	frameCount = 0;
	SVideoInitialize *_pVideoInitialize = (SVideoInitialize*)init;
	g_VideoInitialize = *_pVideoInitialize;
	InitXFBConvTables();

	g_Config.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx9.ini").c_str());
	g_Config.GameIniLoad(globals->game_ini);
	UpdateProjectionHack(g_Config.iPhackvalue);	// DX9 projection hack could be disabled by commenting out this line
	UpdateActiveConfig();
	
	g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create((HWND)g_VideoInitialize.pWindowHandle, g_hInstance, _T("Loading - Please wait."));
	if (g_VideoInitialize.pWindowHandle == NULL)
	{
		ERROR_LOG(VIDEO, "An error has occurred while trying to create the window.");
		return;
	}
	else if (FAILED(D3D::Init()))
	{
		MessageBox(GetActiveWindow(), _T("Unable to initialize Direct3D. Please make sure that you have the latest version of DirectX 9.0c correctly installed."), _T("Fatal Error"), MB_ICONERROR|MB_OK);
		return;
	}

	g_VideoInitialize.pPeekMessages = &Callback_PeekMessages;
	g_VideoInitialize.pUpdateFPSDisplay = &UpdateFPSDisplay;

	_pVideoInitialize->pPeekMessages = g_VideoInitialize.pPeekMessages;
	_pVideoInitialize->pUpdateFPSDisplay = g_VideoInitialize.pUpdateFPSDisplay;
	_pVideoInitialize->pWindowHandle = g_VideoInitialize.pWindowHandle;

	OSD::AddMessage("Dolphin Direct3D9 Video Plugin.", 5000);
	s_PluginInitialized = true;
}

void Video_Prepare()
{
	// Better be safe...
	s_efbAccessRequested = FALSE;
	s_FifoShuttingDown = FALSE;
	s_swapRequested = FALSE;
	Renderer::Init();
	TextureCache::Init();
	BPInit();
	VertexManager::Init();
	Fifo_Init();
	VertexLoaderManager::Init();
	OpcodeDecoder_Init();
	VertexShaderManager::Init();
	PixelShaderManager::Init();
	CommandProcessor::Init();
	PixelEngine::Init();

	// Tell the host the window is ready
	g_VideoInitialize.pCoreMessage(WM_USER_CREATE);
}

void Shutdown()
{
	s_efbAccessRequested = FALSE;
	s_FifoShuttingDown = FALSE;
	s_swapRequested = FALSE;
	Fifo_Shutdown();
	CommandProcessor::Shutdown();
	VertexManager::Shutdown();
	VertexLoaderManager::Shutdown();
	VertexShaderManager::Shutdown();
	PixelShaderManager::Shutdown();
	TextureCache::Shutdown();
	OpcodeDecoder_Shutdown();
	Renderer::Shutdown();
	D3D::Shutdown();
	EmuWindow::Close();
	s_PluginInitialized = false;
}

void DoState(unsigned char **ptr, int mode) {
	// Clear texture cache because it might have written to RAM
	CommandProcessor::FifoCriticalEnter();
	TextureCache::Invalidate(false);
	CommandProcessor::FifoCriticalLeave();
	// No need to clear shader caches.
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

void Video_SetRendering(bool bEnabled) {
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

inline bool addrRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
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

void Video_EndField()
{
}

void Video_AddMessage(const char* pstr, u32 milliseconds)
{
	OSD::AddMessage(pstr,milliseconds);
}

HRESULT ScreenShot(const char *File)
{
	Renderer::SetScreenshot(File);
	return S_OK;
}

void Video_Screenshot(const char *_szFilename)
{
	if (ScreenShot(_szFilename) != S_OK)
		PanicAlert("Error while capturing screen");
	else {
		std::string message =  "Saved ";
		message += _szFilename;
		OSD::AddMessage(message.c_str(), 2000);
	}
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

u32 Video_AccessEFB(EFBAccessType type, u32 x, u32 y,u32 InputData)
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

bool Video_IsFifoBusy(void)
{
	return CommandProcessor::isFifoBusy;
}
