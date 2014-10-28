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

#include <algorithm>
#include <cstdarg>

#include "Common/Atomic.h"
#include "Common/CommonPaths.h"
#include "Common/Thread.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/GLInterfaceBase.h"
#include "VideoBackends/OGL/GLUtil.h"
#include "VideoBackends/OGL/PerfQuery.h"
#include "VideoBackends/OGL/PostProcessing.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureCache.h"
#include "VideoBackends/OGL/TextureConverter.h"
#include "VideoBackends/OGL/VertexManager.h"
#include "VideoBackends/OGL/VideoBackend.h"

#include "VideoCommon/BPStructs.h"
#include "VideoCommon/CommandProcessor.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/MainBase.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/VideoState.h"

namespace OGL
{

std::string VideoBackend::GetName() const
{
	return "OGL";
}

std::string VideoBackend::GetDisplayName() const
{
	if (GLInterface != nullptr && GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES3)
		return "OpenGLES";
	else
		return "OpenGL";
}

static void GetShaders(std::vector<std::string> &shaders)
{
	std::set<std::string> already_found;

	shaders.clear();
	static const std::string directories[] = {
		File::GetUserPath(D_SHADERS_IDX),
		File::GetSysDirectory() + SHADERS_DIR DIR_SEP,
	};
	for (auto& directory : directories)
	{
		if (!File::IsDirectory(directory))
			continue;

		File::FSTEntry entry;
		File::ScanDirectoryTree(directory, entry);
		for (auto& file : entry.children)
		{
			std::string name = file.virtualName;
			if (name.size() < 5)
				continue;
			if (strcasecmp(name.substr(name.size() - 5).c_str(), ".glsl"))
				continue;

			name = name.substr(0, name.size() - 5);
			if (already_found.find(name) != already_found.end())
				continue;

			already_found.insert(name);
			shaders.push_back(name);
		}
	}
	std::sort(shaders.begin(), shaders.end());
}

static void InitBackendInfo()
{
	g_Config.backend_info.APIType = API_OPENGL;
	g_Config.backend_info.bUseMinimalMipCount = false;
	g_Config.backend_info.bSupportsExclusiveFullscreen = false;
	//g_Config.backend_info.bSupportsDualSourceBlend = true; // is gpu dependent and must be set in renderer
	//g_Config.backend_info.bSupportsEarlyZ = true; // is gpu dependent and must be set in renderer
	g_Config.backend_info.bSupportsOversizedViewports = true;

	g_Config.backend_info.Adapters.clear();

	// aamodes
	const char* caamodes[] = {_trans("None"), "2x", "4x", "8x", "4x SSAA"};
	g_Config.backend_info.AAModes.assign(caamodes, caamodes + sizeof(caamodes)/sizeof(*caamodes));

	// pp shaders
	GetShaders(g_Config.backend_info.PPShaders);
}

void VideoBackend::ShowConfig(void *_hParent)
{
	InitBackendInfo();
	Host_ShowVideoConfig(_hParent, GetDisplayName(), "gfx_opengl");
}

bool VideoBackend::Initialize(void *window_handle)
{
	InitializeShared();
	InitBackendInfo();

	frameCount = 0;

	g_Config.Load(File::GetUserPath(D_CONFIG_IDX) + "gfx_opengl.ini");
	g_Config.GameIniLoad();
	g_Config.UpdateProjectionHack();
	g_Config.VerifyValidity();
	UpdateActiveConfig();

	InitInterface();
	GLInterface->SetMode(GLInterfaceMode::MODE_DETECT);
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
	g_texture_cache = new TextureCache();
	g_sampler_cache = new SamplerCache();
	Renderer::Init();
	VertexLoaderManager::Init();
	TextureConverter::Init();

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

void VideoBackend::Video_Cleanup()
{
	if (g_renderer)
	{
		Fifo_Shutdown();

		// The following calls are NOT Thread Safe
		// And need to be called from the video thread
		Renderer::Shutdown();
		TextureConverter::Shutdown();
		VertexLoaderManager::Shutdown();
		delete g_sampler_cache;
		g_sampler_cache = nullptr;
		delete g_texture_cache;
		g_texture_cache = nullptr;
		ProgramShaderCache::Shutdown();
		VertexShaderManager::Shutdown();
		PixelShaderManager::Shutdown();
		delete g_perf_query;
		g_perf_query = nullptr;
		delete g_vertex_manager;
		g_vertex_manager = nullptr;
		OpcodeDecoder_Shutdown();
		delete g_renderer;
		g_renderer = nullptr;
		GLInterface->ClearCurrent();
	}
}

}
