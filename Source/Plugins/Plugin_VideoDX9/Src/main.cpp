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
#include <d3dx9.h>

#include "Common.h"
#include "Atomic.h"
#include "Thread.h"
#include "LogManager.h"
#include "GlobalControl.h"

#if defined(HAVE_WX) && HAVE_WX
#include "Debugger/Debugger.h"
GFXDebuggerDX9 *m_DebuggerFrame = NULL;
#endif // HAVE_WX

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
#include "OnScreenDisplay.h"
#include "DlgSettings.h"
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "W32Util/Misc.h"
#include "EmuWindow.h"
#include "VideoState.h"
#include "XFBConvert.h"

// Having to include this is TERRIBLY ugly. FIXME x100
#include "../../../Core/Core/Src/ConfigManager.h" // FIXME

#include "Utils.h"

HINSTANCE g_hInstance = NULL;
SVideoInitialize g_VideoInitialize;
PLUGIN_GLOBALS* globals = NULL;
bool s_initialized;

static u32 s_efbAccessRequested = FALSE;

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
#ifdef _WIN32
	WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
#endif

wxWindow* GetParentedWxWindow(HWND Parent)
{
#ifdef _WIN32
	wxSetInstance((HINSTANCE)g_hInstance);
#endif
	wxWindow *win = new wxWindow();
#ifdef _WIN32
	win->SetHWND((WXHWND)Parent);
	win->AdoptAttributesFromHWND();
#endif
	return win;
}
#endif

#if defined(HAVE_WX) && HAVE_WX
void DllDebugger(HWND _hParent, bool Show)
{
	if (!m_DebuggerFrame)
		m_DebuggerFrame = new GFXDebuggerDX9(GetParentedWxWindow(_hParent));

	if (Show)
		m_DebuggerFrame->Show();
	else
		m_DebuggerFrame->Hide();
}
#else
void DllDebugger(HWND _hParent, bool Show) { }
#endif

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
			// Use wxInitialize() if you don't want GUI instead of the following 12 lines
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);
			if (!wxTheApp || !wxTheApp->CallOnInit())
				return FALSE;
			#endif
		}
		break;
	case DLL_PROCESS_DETACH:
		#if defined(HAVE_WX) && HAVE_WX
			// This causes a "stop hang", if the gfx config dialog has been opened.
			// Old comment: "Use wxUninitialize() if you don't want GUI"
			wxEntryCleanup();
		#endif
		break;
	default:
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}

unsigned int Callback_PeekMessages()
{
	//TODO: peek message
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
	char temp[512];
	sprintf_s(temp, 512, "SVN R%i: DX9: %s", SVN_REV, text);
    SetWindowTextA(EmuWindow::GetWnd(), temp);
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

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals) {
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager *)globals->logManager);
}

void DllAbout(HWND _hParent)
{
	DialogBoxA(g_hInstance,(LPCSTR)IDD_ABOUT,_hParent,(DLGPROC)AboutProc);
}

void DllConfig(HWND _hParent)
{
	// If not initialized, only init D3D so we can enumerate resolutions.
	if (!s_initialized) D3D::Init();
	DlgSettings_Show(g_hInstance, _hParent);
	if (!s_initialized)	D3D::Shutdown();
}

void Initialize(void *init)
{
    SVideoInitialize *_pVideoInitialize = (SVideoInitialize*)init;
	frameCount = 0;
	g_VideoInitialize = *_pVideoInitialize;

	g_Config.Load(FULL_CONFIG_DIR "gfx_dx9.ini");
	g_Config.GameIniLoad(globals->game_ini);
	UpdateProjectionHack(g_Config.iPhackvalue);	// DX9 projection hack could be disabled by commenting out this line

    // create the window
	if (!g_Config.RenderToMainframe || g_VideoInitialize.pWindowHandle == NULL) // ignore parent for this plugin
		g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create(NULL, g_hInstance, _T("Loading - Please wait."));
	else
		g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create((HWND)g_VideoInitialize.pWindowHandle, g_hInstance, _T("Loading - Please wait."));
	if (g_VideoInitialize.pWindowHandle == NULL)
	{
		ERROR_LOG(VIDEO, "An error has occurred while trying to create the window.");
		return;
	}
	EmuWindow::Show();
	g_VideoInitialize.pPeekMessages = Callback_PeekMessages;
	g_VideoInitialize.pUpdateFPSDisplay = UpdateFPSDisplay;
	if (FAILED(D3D::Init()))
	{
		MessageBox(GetActiveWindow(), _T("Unable to initialize Direct3D. Please make sure that you have the latest version of DirectX 9.0c correctly installed."), _T("Fatal Error"), MB_OK);
		return;
	}
	InitXFBConvTables();
	OSD::AddMessage("Dolphin Direct3D9 Video Plugin.", 5000);
    _pVideoInitialize->pPeekMessages = g_VideoInitialize.pPeekMessages;
    _pVideoInitialize->pUpdateFPSDisplay = g_VideoInitialize.pUpdateFPSDisplay;
    _pVideoInitialize->pWindowHandle = g_VideoInitialize.pWindowHandle;
	s_initialized = true;
}

void Video_Prepare()
{
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
}

void Shutdown()
{
	s_efbAccessRequested = FALSE;

	Fifo_Shutdown();
	VertexManager::Shutdown();
	VertexLoaderManager::Shutdown();
	VertexShaderCache::Shutdown();
	VertexShaderManager::Shutdown();
	PixelShaderCache::Shutdown();
	PixelShaderManager::Shutdown();
	TextureCache::Shutdown();
	OpcodeDecoder_Shutdown();
	Renderer::Shutdown();
	D3D::Shutdown();
	EmuWindow::Close();
	s_initialized = false;
}

void DoState(unsigned char **ptr, int mode) {
	// Clear texture cache because it might have written to RAM
	TextureCache::Invalidate(false);
	// No need to clear shader caches.
	PointerWrap p(ptr, mode);
	VideoCommon_DoState(p);
}

void Video_EnterLoop()
{
	Fifo_EnterLoop(g_VideoInitialize);
}

void Video_ExitLoop()
{
	Fifo_ExitLoop();
}

void Video_SetRendering(bool bEnabled) {
	Fifo_SetRendering(bEnabled);
}

void Video_SendFifoData(u8* _uData, u32 len)
{
	Fifo_SendFifoData(_uData, len);
}

void VideoFifo_CheckSwapRequest()
{
	// CPU swap unimplemented
}

void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	// CPU swap unimplemented
}

void Video_BeginField(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{
	/*
	ConvertXFB(tempBuffer, _pXFB, _dwWidth, _dwHeight);

	// blubb
	static LPDIRECT3DTEXTURE9 pTexture = D3D::CreateTexture2D((BYTE*)tempBuffer, _dwWidth, _dwHeight, _dwWidth, D3DFMT_A8R8G8B8);

	D3D::ReplaceTexture2D(pTexture, (BYTE*)tempBuffer, _dwWidth, _dwHeight, _dwWidth, D3DFMT_A8R8G8B8);
	D3D::dev->SetTexture(0, pTexture);

	D3D::quad2d(0,0,(float)Postprocess::GetWidth(),(float)Postprocess::GetHeight(), 0xFFFFFFFF);

	D3D::EndFrame();
	D3D::BeginFrame();*/
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
	if (D3D::dev == NULL)
		return S_FALSE;

	D3DDISPLAYMODE DisplayMode;
	if (FAILED(D3D::dev->GetDisplayMode(0, &DisplayMode)))
		return S_FALSE;

	LPDIRECT3DSURFACE9 surf;
	if (FAILED(D3D::dev->CreateOffscreenPlainSurface(DisplayMode.Width, DisplayMode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surf, NULL)))
		return S_FALSE;

	if (FAILED(D3D::dev->GetFrontBufferData(0, surf)))
	{
		surf->Release();
		return S_FALSE;
	}

	RECT rect;
    ::GetWindowRect(EmuWindow::GetWnd(), &rect);
	if (FAILED(D3DXSaveSurfaceToFileA(File, D3DXIFF_PNG, surf, NULL, &rect)))
	{
		surf->Release();
		return S_FALSE;
	}

	surf->Release();
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

u32 Video_AccessEFB(EFBAccessType type, u32 x, u32 y)
{
	s_accessEFBArgs.type = type;
	s_accessEFBArgs.x = x;
	s_accessEFBArgs.y = y;

	Common::AtomicStoreRelease(s_efbAccessRequested, TRUE);

	if (g_VideoInitialize.bUseDualCore)
	{
		while (Common::AtomicLoadAcquire(s_efbAccessRequested))
			Common::YieldCPU();
	}
	else
		VideoFifo_CheckEFBAccess();

	return s_AccessEFBResult;
}
