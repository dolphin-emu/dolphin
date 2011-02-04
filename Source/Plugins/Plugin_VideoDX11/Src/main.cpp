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

#include <wx/wx.h>

#include "LogManager.h"

#include "BPStructs.h"
#include "CommandProcessor.h"
#include "Fifo.h"
#include "OnScreenDisplay.h"
#include "OpcodeDecoding.h"
#include "PixelEngine.h"
#include "PixelShaderManager.h"
#include "VideoConfig.h"
#include "VertexLoaderManager.h"
#include "VertexShaderManager.h"
#include "Core.h"

#include "DebuggerPanel.h"
#include "DLCache.h"
#include "EmuWindow.h"
#include "FileUtil.h"
#include "Globals.h"
#include "IniFile.h"
#include "VideoConfigDiag.h"

#include "D3DUtil.h"
#include "D3DBase.h"
#include "PixelShaderCache.h"
#include "TextureCache.h"
#include "VertexManager.h"
#include "VertexShaderCache.h"

#include "VideoBackend.h"
#include "ConfigManager.h"

namespace DX11
{

void*& VideoWindowHandle()
{
	return SConfig::GetInstance().m_LocalCoreStartupParameter.hMainWindow;
}

unsigned int VideoBackend::PeekMessages()
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

void VideoBackend::UpdateFPSDisplay(const char *text)
{
	char temp[512];
	sprintf_s(temp, sizeof temp, "%s | DX11 | %s", svn_rev_str, text);
	SetWindowTextA(EmuWindow::GetWnd(), temp);
}

//void GetDllInfo(PLUGIN_INFO* _PluginInfo)
//{
//	_PluginInfo->Version = 0x0100;
//	//_PluginInfo->Type = PLUGIN_TYPE_VIDEO;
//#ifdef DEBUGFAST
//	sprintf_s(_PluginInfo->Name, 100, "Dolphin Direct3D11 (DebugFast)");
//#elif defined _DEBUG
//	sprintf_s(_PluginInfo->Name, 100, "Dolphin Direct3D11 (Debug)");
//#else
//	sprintf_s(_PluginInfo->Name, 100, "Dolphin Direct3D11");
//#endif
//}

std::string VideoBackend::GetName()
{
	return "Direct3D11";
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
	g_Config.backend_info.bSupportsFormatReinterpretation = true;
	g_Config.backend_info.bSupportsPixelLighting = true;
}

void VideoBackend::ShowConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	InitBackendInfo();

	HRESULT hr = DX11::D3D::LoadDXGI();
	if (SUCCEEDED(hr)) hr = DX11::D3D::LoadD3D();
	if (FAILED(hr))
	{
		DX11::D3D::UnloadDXGI();
		return;
	}

	IDXGIFactory* factory;
	IDXGIAdapter* ad;
	hr = DX11::PCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
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
			DX11::D3D::EnumAAModes(ad, modes);
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

	g_Config.backend_info.Adapters.clear();
	DX11::D3D::UnloadDXGI();
	DX11::D3D::UnloadD3D();
#endif
}

void VideoBackend::Initialize()
{
	InitBackendInfo();

	frameCount = 0;

	g_Config.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_dx11.ini").c_str());
	g_Config.GameIniLoad(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIni.c_str());
	UpdateProjectionHack(g_Config.iPhackvalue, g_Config.sPhackvalue);
	UpdateActiveConfig();

	VideoWindowHandle() = (void*)EmuWindow::Create((HWND)VideoWindowHandle(), GetModuleHandle(0), _T("Loading - Please wait."));
	if (VideoWindowHandle() == NULL)
	{
		ERROR_LOG(VIDEO, "An error has occurred while trying to create the window.");
		return;
	}

	OSD::AddMessage("Dolphin Direct3D11 Video Plugin.", 5000);
	s_PluginInitialized = true;
}

void VideoBackend::Video_Prepare()
{
	// Better be safe...
	s_efbAccessRequested = FALSE;
	s_FifoShuttingDown = FALSE;
	s_swapRequested = FALSE;

	// internal interfaces
	g_renderer = new Renderer;
	g_texture_cache = new TextureCache;
	g_vertex_manager = new VertexManager;
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
	Core::Callback_CoreMessage(WM_USER_CREATE);
}

void VideoBackend::Shutdown()
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

}
