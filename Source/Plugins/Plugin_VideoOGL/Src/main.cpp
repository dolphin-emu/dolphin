// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.



// OpenGL Backend Documentation
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
#include "Atomic.h"
#include "Thread.h"
#include "LogManager.h"

#include <cstdarg>
#include <algorithm>

#ifdef _WIN32
#include "EmuWindow.h"
#include "IniFile.h"
#endif

#if defined(HAVE_WX) && HAVE_WX
#include "VideoConfigDiag.h"
#include "Debugger/DebuggerPanel.h"
#endif // HAVE_WX

#include "MainBase.h"
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
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "ProgramShaderCache.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "TextureConverter.h"
#include "PostProcessing.h"
#include "OnScreenDisplay.h"
#include "DLCache.h"
#include "FramebufferManager.h"
#include "Core.h"
#include "Host.h"
#include "SamplerCache.h"
#include "PerfQuery.h"

#include "VideoState.h"
#include "IndexGenerator.h"
#include "VideoBackend.h"
#include "ConfigManager.h"

namespace OGL
{

std::string VideoBackend::GetName()
{
	return "OGL";
}

std::string VideoBackend::GetDisplayName()
{
	return "OpenGL";
}

void GetShaders(std::vector<std::string> &shaders)
{
	shaders.clear();
	if (File::IsDirectory(File::GetUserPath(D_SHADERS_IDX)))
	{
		File::FSTEntry entry;
		File::ScanDirectoryTree(File::GetUserPath(D_SHADERS_IDX), entry);
		for (u32 i = 0; i < entry.children.size(); i++) 
		{
			std::string name = entry.children[i].virtualName.c_str();
			if (!strcasecmp(name.substr(name.size() - 4).c_str(), ".txt")) {
				name = name.substr(0, name.size() - 4);
				shaders.push_back(name);
			}
		}
		std::sort(shaders.begin(), shaders.end());
	}
	else
	{
		File::CreateDir(File::GetUserPath(D_SHADERS_IDX).c_str());
	}
}

void InitBackendInfo()
{
	g_Config.backend_info.APIType = API_OPENGL;
	g_Config.backend_info.bUseRGBATextures = true;
	g_Config.backend_info.bUseMinimalMipCount = false;
	g_Config.backend_info.bSupports3DVision = false;
	//g_Config.backend_info.bSupportsDualSourceBlend = true; // is gpu dependent and must be set in renderer
	g_Config.backend_info.bSupportsFormatReinterpretation = true;
	g_Config.backend_info.bSupportsPixelLighting = true;
	//g_Config.backend_info.bSupportsEarlyZ = true; // is gpu dependent and must be set in renderer

	// aamodes
	const char* caamodes[] = {_trans("None"), "2x", "4x", "8x", "8x CSAA", "8xQ CSAA", "16x CSAA", "16xQ CSAA", "4x SSAA"};
	g_Config.backend_info.AAModes.assign(caamodes, caamodes + sizeof(caamodes)/sizeof(*caamodes));

	// pp shaders
	GetShaders(g_Config.backend_info.PPShaders);
}

void VideoBackend::ShowConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	InitBackendInfo();
	VideoConfigDiag diag((wxWindow*)_hParent, "OpenGL", "gfx_opengl");
	diag.ShowModal();
#endif
}

bool VideoBackend::Initialize(void *&window_handle)
{
	InitializeShared();
	InitBackendInfo();

	frameCount = 0;

	g_Config.Load((File::GetUserPath(D_CONFIG_IDX) + "gfx_opengl.ini").c_str());
	g_Config.GameIniLoad(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIni.c_str());
	g_Config.UpdateProjectionHack();
	g_Config.VerifyValidity();
	UpdateActiveConfig();

	InitInterface();
	if (!GLInterface->Create(window_handle))
		return false;

	// Do our OSD callbacks	
	OSD::DoCallbacks(OSD::OSD_INIT);

	s_BackendInitialized = true;

	return true;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
	GLInterface->MakeCurrent();

	g_renderer = new Renderer;

	s_efbAccessRequested = false;
	s_FifoShuttingDown = false;
	s_swapRequested = false;

	CommandProcessor::Init();
	PixelEngine::Init();

	BPInit();
	g_vertex_manager = new VertexManager;
	g_perf_query = new PerfQuery;
	Fifo_Init(); // must be done before OpcodeDecoder_Init()
	OpcodeDecoder_Init();
	IndexGenerator::Init();
	VertexShaderManager::Init();
	PixelShaderManager::Init();
	ProgramShaderCache::Init();
	PostProcessing::Init();
	g_texture_cache = new TextureCache();
	g_sampler_cache = new SamplerCache();
	Renderer::Init();
	GL_REPORT_ERRORD();
	VertexLoaderManager::Init();
	TextureConverter::Init();
#ifndef _M_GENERIC
	DLCache::Init();
#endif

	// Notify the core that the video backend is ready
	Host_Message(WM_USER_CREATE);
}

void VideoBackend::Shutdown()
{
	s_BackendInitialized = false;

	// Do our OSD callbacks	
	OSD::DoCallbacks(OSD::OSD_SHUTDOWN);

	GLInterface->Shutdown();
}

void VideoBackend::Video_Cleanup() {
	
	if (g_renderer)
	{
		s_efbAccessRequested = false;
		s_FifoShuttingDown = false;
		s_swapRequested = false;
#ifndef _M_GENERIC
		DLCache::Shutdown();
#endif
		Fifo_Shutdown();

		// The following calls are NOT Thread Safe
		// And need to be called from the video thread
		Renderer::Shutdown();
		TextureConverter::Shutdown();
		VertexLoaderManager::Shutdown();
		delete g_sampler_cache;
		g_sampler_cache = NULL;
		delete g_texture_cache;
		g_texture_cache = NULL;
		PostProcessing::Shutdown();
		ProgramShaderCache::Shutdown();
		VertexShaderManager::Shutdown();
		PixelShaderManager::Shutdown();
		delete g_perf_query;
		g_perf_query = NULL;
		delete g_vertex_manager;
		g_vertex_manager = NULL;
		OpcodeDecoder_Shutdown();
		delete g_renderer;
		g_renderer = NULL;
		GLInterface->ClearCurrent();
	}
}

}
