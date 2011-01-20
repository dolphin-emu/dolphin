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

#include "VideoConfig.h"
#include "Fifo.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "VertexLoaderManager.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "OnScreenDisplay.h"
#include "VideoState.h"
#include "XFBConvert.h"
#include "Render.h"

#include "MainBase.h"
#include "main.h"
#include "VideoConfigDiag.h"
#include "TextureCache.h"
#include "VertexManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderCache.h"
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "EmuWindow.h"
#include "FramebufferManager.h"
#include "DLCache.h"
#include "DebuggerPanel.h"
#include "IniFile.h"

HINSTANCE g_hInstance = NULL;

wxLocale *InitLanguageSupport()
{
	wxLocale *m_locale;
	unsigned int language = 0;

	IniFile ini;
	ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	ini.Get("Interface", "Language", &language, wxLANGUAGE_DEFAULT);

	// Load language if possible, fall back to system default otherwise
	if(wxLocale::IsAvailable(language))
	{
		m_locale = new wxLocale(language);

		m_locale->AddCatalogLookupPathPrefix(wxT("Languages"));

		m_locale->AddCatalog(wxT("dolphin-emu"));

		if(!m_locale->IsOk())
		{
			PanicAlertT("Error loading selected language. Falling back to system default.");
			delete m_locale;
			m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
		}
	}
	else
	{
		PanicAlertT("The selected language is not supported by your system. Falling back to system default.");
		m_locale = new wxLocale(wxLANGUAGE_DEFAULT);
	}
	return m_locale;
}

// This is used for the functions right below here which use wxwidgets
WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);

void *DllDebugger(void *_hParent, bool Show)
{
	return new GFXDebuggerPanel((wxWindow*)_hParent);
}

class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp)
WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpvReserved)
{
	static wxLocale *m_locale;
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{
			wxSetInstance((HINSTANCE)hinstDLL);
			wxInitialize();
			m_locale = InitLanguageSupport();
		}
		break;
	case DLL_PROCESS_DETACH:
		wxUninitialize();
		delete m_locale;
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
	char temp[512];
	sprintf_s(temp, sizeof temp, "%s | DX11 | %s", svn_rev_str, text);
	SetWindowTextA(EmuWindow::GetWnd(), temp);
}

void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_VIDEO;
#ifdef DEBUGFAST
	sprintf_s(_PluginInfo->Name, 100, "Dolphin Direct3D11 (DebugFast)");
#elif defined _DEBUG
	sprintf_s(_PluginInfo->Name, 100, "Dolphin Direct3D11 (Debug)");
#else
	sprintf_s(_PluginInfo->Name, 100, "Dolphin Direct3D11");
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager*)globals->logManager);
}

void InitBackendInfo()
{
	g_Config.backend_info.APIType = API_D3D11;
	g_Config.backend_info.bUseRGBATextures = true; // the GX formats barely match any D3D11 formats
	g_Config.backend_info.bSupportsEFBToRAM = false;
	g_Config.backend_info.bSupportsRealXFB = false;
	g_Config.backend_info.bSupports3DVision = false;
	g_Config.backend_info.bAllowSignedBytes = true;
	g_Config.backend_info.bSupportsDualSourceBlend = true;
	g_Config.backend_info.bSupportsFormatReinterpretation = false;
	g_Config.backend_info.bSupportsPixelLighting = true;
}

void DllConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	InitBackendInfo();

	HRESULT hr = D3D::LoadDXGI();
	if (SUCCEEDED(hr)) hr = D3D::LoadD3D();
	if (FAILED(hr))
	{
		D3D::UnloadDXGI();
		return;
	}

	IDXGIFactory* factory;
	IDXGIAdapter* ad;
	hr = PCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(hr))
		PanicAlert("Failed to create IDXGIFactory object");

	char tmpstr[512] = {};
	DXGI_ADAPTER_DESC desc;
	// adapters
	g_Config.backend_info.Adapters.clear();
	g_Config.backend_info.AAModes.clear();
	while (factory->EnumAdapters((UINT)g_Config.backend_info.Adapters.size(), &ad) != DXGI_ERROR_NOT_FOUND)
	{
		ad->GetDesc(&desc);
		WideCharToMultiByte(/*CP_UTF8*/CP_ACP, 0, desc.Description, -1, tmpstr, 512, 0, false);

		// TODO: These don't get updated on adapter change, yet
		if (g_Config.backend_info.Adapters.size() == g_Config.iAdapter)
		{
			char buf[32];
			std::vector<DXGI_SAMPLE_DESC> modes;
			D3D::EnumAAModes(ad, modes);
			for (unsigned int i = 0; i < modes.size(); ++i)
			{
				if (i == 0) sprintf_s(buf, 32, "None");
				else if (modes[i].Quality) sprintf_s(buf, 32, "%d samples (quality level %d)", modes[i].Count, modes[i].Quality);
				else sprintf_s(buf, 32, "%d samples", modes[i].Count);
				g_Config.backend_info.AAModes.push_back(buf);
			}
		}

		g_Config.backend_info.Adapters.push_back(tmpstr);
		ad->Release();
	}

	factory->Release();

	VideoConfigDiag *const diag = new VideoConfigDiag((wxWindow*)_hParent, _trans("Direct3D11"), "gfx_dx11");
	diag->ShowModal();
	diag->Destroy();

	D3D::UnloadDXGI();
	D3D::UnloadD3D();
#endif
}

void Initialize(void *init)
{
	InitBackendInfo();

	frameCount = 0;
	SVideoInitialize *_pVideoInitialize = (SVideoInitialize*)init;
	// Create a shortcut to _pVideoInitialize that can also update it
	g_VideoInitialize = *(_pVideoInitialize);
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

	// Now the window handle is written
	_pVideoInitialize->pWindowHandle = g_VideoInitialize.pWindowHandle;

	OSD::AddMessage("Dolphin Direct3D11 Video Plugin.", 5000);
	s_PluginInitialized = true;
}

void Video_Prepare()
{
	// Better be safe...
	s_efbAccessRequested = FALSE;
	s_FifoShuttingDown = FALSE;
	s_swapRequested = FALSE;

	// internal interfaces
	g_renderer = new DX11::Renderer;
	g_texture_cache = new DX11::TextureCache;
	g_vertex_manager = new DX11::VertexManager;
	VertexShaderCache::Init();
	PixelShaderCache::Init();
	D3D::InitUtils();

	// VideoCommon
	BPInit();
	Fifo_Init();
	VertexLoaderManager::Init();
	OpcodeDecoder_Init();
	VertexShaderManager::Init();
	PixelShaderManager::Init();
	CommandProcessor::Init();
	PixelEngine::Init();
	DLCache::Init();

	// Tell the host that the window is ready
	g_VideoInitialize.pCoreMessage(WM_USER_CREATE);
}

void Shutdown()
{
	s_PluginInitialized = false;

	s_efbAccessRequested = FALSE;
	s_FifoShuttingDown = FALSE;
	s_swapRequested = FALSE;

	// VideoCommon
	DLCache::Shutdown();
	Fifo_Shutdown();
	CommandProcessor::Shutdown();
	PixelShaderManager::Shutdown();
	VertexShaderManager::Shutdown();
	OpcodeDecoder_Shutdown();
	VertexLoaderManager::Shutdown();

	// internal interfaces
	D3D::ShutdownUtils();
	PixelShaderCache::Shutdown();
	VertexShaderCache::Shutdown();
	delete g_vertex_manager;
	delete g_texture_cache;
	delete g_renderer;
	EmuWindow::Close();

	s_PluginInitialized = false;
}
