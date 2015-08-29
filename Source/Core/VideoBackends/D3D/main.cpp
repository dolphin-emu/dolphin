// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"

#include "VideoBackends/D3D/BoundingBox.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DUtil.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"
#include "VideoBackends/D3D/Globals.h"
#include "VideoBackends/D3D/PerfQuery.h"
#include "VideoBackends/D3D/PixelShaderCache.h"
#include "VideoBackends/D3D/TextureCache.h"
#include "VideoBackends/D3D/VertexManager.h"
#include "VideoBackends/D3D/VertexShaderCache.h"
#include "VideoBackends/D3D/VideoBackend.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VR.h"

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

std::string VideoBackend::GetName() const
{
	return "D3D";
}

std::string VideoBackend::GetDisplayName() const
{
	return "Direct3D";
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

	g_Config.backend_info.APIType = API_D3D;
	g_Config.backend_info.bSupportsExclusiveFullscreen = true;
	g_Config.backend_info.bSupportsDualSourceBlend = true;
	g_Config.backend_info.bSupportsPrimitiveRestart = true;
	g_Config.backend_info.bSupportsOversizedViewports = false;
	g_Config.backend_info.bSupportsGeometryShaders = true;
	g_Config.backend_info.bSupports3DVision = true;
	g_Config.backend_info.bSupportsPostProcessing = false;
	g_Config.backend_info.bSupportsPaletteConversion = true;

	IDXGIFactory* factory;
	IDXGIAdapter* ad;
	//Oculus SDK Bug: Once OpenGL mode has been attached in Direct Mode,
	//running the next line will cause a crash in Oculus SDK.  Seems to work
    //If you never ShutdownVR() the OpenGL mode, and never InitVR() for the
	//Direct3D mode, but that is very hacky.  Running ovr_Initialize(); here stops
	//crash, bug kills Direct Mode. Wait until SDK fixes this issue?
#if defined(OVR_MAJOR_VERSION) && OVR_MAJOR_VERSION >= 6
	if (DX11::PCreateDXGIFactory1)
		hr = DX11::PCreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
	else
		hr = E_NOINTERFACE;
#else
	hr = DX11::PCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
#endif
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
			std::string samples;
			std::vector<DXGI_SAMPLE_DESC> modes = DX11::D3D::EnumAAModes(ad);
			for (unsigned int i = 0; i < modes.size(); ++i)
			{
				if (i == 0)
					samples = _trans("None");
				else if (modes[i].Quality)
					samples = StringFromFormat(_trans("%d samples (quality level %d)"), modes[i].Count, modes[i].Quality);
				else
					samples = StringFromFormat(_trans("%d samples"), modes[i].Count);

				g_Config.backend_info.AAModes.push_back(samples);
			}

			bool shader_model_5_supported = (DX11::D3D::GetFeatureLevel(ad) >= D3D_FEATURE_LEVEL_11_0);

			// Requires the earlydepthstencil attribute (only available in shader model 5)
			g_Config.backend_info.bSupportsEarlyZ = shader_model_5_supported;

			// Requires full UAV functionality (only available in shader model 5)
			g_Config.backend_info.bSupportsBBox = shader_model_5_supported;

			// Requires the instance attribute (only available in shader model 5)
			g_Config.backend_info.bSupportsGSInstancing = shader_model_5_supported;
		}
		g_Config.backend_info.Adapters.push_back(UTF16ToUTF8(desc.Description));
		ad->Release();
	}
	factory->Release();

	// Clear ppshaders string vector
	g_Config.backend_info.PPShaders.clear();
	g_Config.backend_info.AnaglyphShaders.clear();

	DX11::D3D::UnloadDXGI();
	DX11::D3D::UnloadD3D();
}

void VideoBackend::ShowConfig(void *hParent)
{
	InitBackendInfo();
	Host_ShowVideoConfig(hParent, GetDisplayName(), "gfx_dx11");
}

bool VideoBackend::Initialize(void *window_handle)
{
	if (window_handle == nullptr)
		return false;

	InitializeShared();
	InitBackendInfo();

	frameCount = 0;

	g_Config.Load(File::GetUserPath(D_CONFIG_IDX) + "gfx_dx11.ini");
	g_Config.GameIniLoad();
	g_Config.UpdateProjectionHack();
	g_Config.VerifyValidity();
	UpdateActiveConfig();

	m_window_handle = window_handle;

	s_BackendInitialized = true;

	return true;
}

bool VideoBackend::InitializeOtherThread(void *window_handle, std::thread *video_thread)
{
	m_video_thread = video_thread;
	return true;
}

void VideoBackend::Video_Prepare()
{
	// internal interfaces
	g_renderer = new Renderer(m_window_handle);
	g_texture_cache = new TextureCache;
	g_vertex_manager = new VertexManager;
	g_perf_query = new PerfQuery;
	VertexShaderCache::Init();
	PixelShaderCache::Init();
	GeometryShaderCache::Init();
	D3D::InitUtils();

	// VideoCommon
	BPInit();
	Fifo_Init();
	IndexGenerator::Init();
	VertexLoaderManager::Init();
	OpcodeDecoder_Init();
	VertexShaderManager::Init();
	PixelShaderManager::Init();
	GeometryShaderManager::Init();
	CommandProcessor::Init();
	PixelEngine::Init();
	BBox::Init();

	// Tell the host that the window is ready
	Host_Message(WM_USER_CREATE);
}

void VideoBackend::Video_PrepareOtherThread()
{
	// In OpenGL this would be GLInterface->MakeCurrent();
	// probably not needed for multithreaded Direct3D.
}

void VideoBackend::Shutdown()
{
	s_BackendInitialized = false;

	// TODO: should be in Video_Cleanup
	if (g_renderer)
	{
		// VideoCommon
		Fifo_Shutdown();
		CommandProcessor::Shutdown();
		GeometryShaderManager::Shutdown();
		PixelShaderManager::Shutdown();
		VertexShaderManager::Shutdown();
		OpcodeDecoder_Shutdown();
		VertexLoaderManager::Shutdown();

		// internal interfaces
		D3D::ShutdownUtils();
		PixelShaderCache::Shutdown();
		VertexShaderCache::Shutdown();
		GeometryShaderCache::Shutdown();
		BBox::Shutdown();

		delete g_perf_query;
		delete g_vertex_manager;
		delete g_texture_cache;
		delete g_renderer;
		g_renderer = nullptr;
		g_texture_cache = nullptr;
		ShutdownVR();
	}
}

void VideoBackend::ShutdownOtherThread()
{
}

void VideoBackend::Video_Cleanup()
{
}

void VideoBackend::Video_CleanupOtherThread()
{
}

}
