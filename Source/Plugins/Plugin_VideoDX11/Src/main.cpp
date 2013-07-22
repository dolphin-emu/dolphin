// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
#include "Host.h"

#include "Debugger/DebuggerPanel.h"
#include "DLCache.h"
#include "EmuWindow.h"
#include "IndexGenerator.h"
#include "FileUtil.h"
#include "Globals.h"
#include "IniFile.h"
#include "VideoConfigDiag.h"

#include "D3DUtil.h"
#include "D3DBase.h"
#include "PerfQuery.h"
#include "PixelShaderCache.h"
#include "TextureCache.h"
#include "VertexManager.h"
#include "VertexShaderCache.h"

#include "VideoBackend.h"
#include "ConfigManager.h"

namespace DX11
{

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
	TCHAR temp[512];
	swprintf_s(temp, sizeof(temp)/sizeof(TCHAR), _T("%hs | DX11 | %hs"), scm_rev_str, text);
	EmuWindow::SetWindowText(temp);
}

std::string VideoBackend::GetName()
{
	return "DX11";
}

std::string VideoBackend::GetDisplayName()
{
	return "Direct3D11";
}

void InitBackendInfo()
{
	HRESULT hr = DX11::D3D::LoadDXGI();
	if (SUCCEEDED(hr)) hr = DX11::D3D::LoadD3D();
	if (FAILED(hr))
	{
		DX11::D3D::UnloadDXGI();
		return;
	}

	g_Config.backend_info.APIType = API_D3D11;
	g_Config.backend_info.bUseRGBATextures = true; // the GX formats barely match any D3D11 formats
	g_Config.backend_info.bUseMinimalMipCount = true;
	g_Config.backend_info.bSupports3DVision = false;
	g_Config.backend_info.bSupportsDualSourceBlend = true;
	g_Config.backend_info.bSupportsFormatReinterpretation = true;
	g_Config.backend_info.bSupportsPixelLighting = true;
	g_Config.backend_info.bSupportsPrimitiveRestart = true;

	IDXGIFactory* factory;
	IDXGIAdapter* ad;
	hr = DX11::PCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(hr))
		PanicAlert("Failed to create IDXGIFactory object");

	// adapters
	g_Config.backend_info.Adapters.clear();
	g_Config.backend_info.AAModes.clear();
	while (factory->EnumAdapters((UINT)g_Config.backend_info.Adapters.size(), &ad) != DXGI_ERROR_NOT_FOUND)
	{
		const size_t adapter_index = g_Config.backend_info.Adapters.size();

		DXGI_ADAPTER_DESC desc;
		ad->GetDesc(&desc);

		// TODO: These don't get updated on adapter change, yet
		if (adapter_index == g_Config.iAdapter)
		{
			char buf[32];
			std::vector<DXGI_SAMPLE_DESC> modes;
			modes = DX11::D3D::EnumAAModes(ad);
			for (unsigned int i = 0; i < modes.size(); ++i)
			{
				if (i == 0) sprintf_s(buf, 32, _trans("None"));
				else if (modes[i].Quality) sprintf_s(buf, 32, _trans("%d samples (quality level %d)"), modes[i].Count, modes[i].Quality);
				else sprintf_s(buf, 32, _trans("%d samples"), modes[i].Count);
				g_Config.backend_info.AAModes.push_back(buf);
			}

			// Requires the earlydepthstencil attribute (only available in shader model 5)
			g_Config.backend_info.bSupportsEarlyZ = (DX11::D3D::GetFeatureLevel(ad) == D3D_FEATURE_LEVEL_11_0);
		}

		g_Config.backend_info.Adapters.push_back(UTF16ToUTF8(desc.Description));
		ad->Release();
	}

	factory->Release();

	// Clear ppshaders string vector
	g_Config.backend_info.PPShaders.clear();

	DX11::D3D::UnloadDXGI();
	DX11::D3D::UnloadD3D();
}

void VideoBackend::ShowConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	InitBackendInfo();
	VideoConfigDiag diag((wxWindow*)_hParent, _trans("Direct3D11"), "gfx_dx11");
	diag.ShowModal();
#endif
}

bool VideoBackend::Initialize(void *&window_handle)
{
	InitializeShared();
	InitBackendInfo();

	frameCount = 0;

	g_Config.Load((File::GetUserPath(D_CONFIG_IDX) + "gfx_dx11.ini").c_str());
	g_Config.GameIniLoad(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIni.c_str());
	g_Config.UpdateProjectionHack();
	g_Config.VerifyValidity();
	UpdateActiveConfig();

	window_handle = (void*)EmuWindow::Create((HWND)window_handle, GetModuleHandle(0), _T("Loading - Please wait."));
	if (window_handle == NULL)
	{
		ERROR_LOG(VIDEO, "An error has occurred while trying to create the window.");
		return false;
	}

	s_BackendInitialized = true;

	return true;
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
	g_perf_query = new PerfQuery;
	VertexShaderCache::Init();
	PixelShaderCache::Init();
	D3D::InitUtils();

	// VideoCommon
	BPInit();
	Fifo_Init();
	IndexGenerator::Init();
	VertexLoaderManager::Init();
	OpcodeDecoder_Init();
	VertexShaderManager::Init();
	PixelShaderManager::Init();
	CommandProcessor::Init();
	PixelEngine::Init();
	DLCache::Init();

	// Tell the host that the window is ready
	Host_Message(WM_USER_CREATE);
}

void VideoBackend::Shutdown()
{
	s_BackendInitialized = false;

	// TODO: should be in Video_Cleanup
	if (g_renderer)
	{
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
		delete g_perf_query;
		delete g_vertex_manager;
		delete g_texture_cache;
		delete g_renderer;
		g_renderer = NULL;
		g_texture_cache = NULL;
	}
}

void VideoBackend::Video_Cleanup() {
}

}
