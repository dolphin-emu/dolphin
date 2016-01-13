// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
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

#include <memory>
#include <string>
#include <vector>

#include "Common/Atomic.h"
#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/GL/GLInterfaceBase.h"
#include "Common/GL/GLUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/PerfQuery.h"
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
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{

// Draw messages on top of the screen
unsigned int VideoBackend::PeekMessages()
{
	return GLInterface->PeekMessages();
}

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

std::string VideoBackend::GetConfigName() const
{
	return "gfx_opengl";
}

static std::vector<std::string> GetShaders(const std::string &sub_dir = "")
{
	std::vector<std::string> paths = DoFileSearch({".glsl"}, {
		File::GetUserPath(D_SHADERS_IDX) + sub_dir,
		File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir
	});
	std::vector<std::string> result;
	for (std::string path : paths)
	{
		std::string name;
		SplitPath(path, nullptr, &name, nullptr);
		result.push_back(name);
	}
	return result;
}

void VideoBackend::InitBackendInfo()
{
	g_Config.backend_info.APIType = API_OPENGL;
	g_Config.backend_info.bSupportsExclusiveFullscreen = false;
	g_Config.backend_info.bSupportsOversizedViewports = true;
	g_Config.backend_info.bSupportsGeometryShaders = true;
	g_Config.backend_info.bSupports3DVision = false;
	g_Config.backend_info.bSupportsPostProcessing = true;
	g_Config.backend_info.bSupportsSSAA = true;

	// Overwritten in Render.cpp later
	g_Config.backend_info.bSupportsDualSourceBlend = true;
	g_Config.backend_info.bSupportsPrimitiveRestart = true;
	g_Config.backend_info.bSupportsPaletteConversion = true;
	g_Config.backend_info.bSupportsClipControl = true;

	g_Config.backend_info.Adapters.clear();

	// aamodes - 1 is to stay consistent with D3D (means no AA)
	g_Config.backend_info.AAModes = { 1, 2, 4, 8 };

	// pp shaders
	g_Config.backend_info.PPShaders = GetShaders("");
	g_Config.backend_info.AnaglyphShaders = GetShaders(ANAGLYPH_DIR DIR_SEP);
}

bool VideoBackend::Initialize(void* window_handle)
{
	InitBackendInfo();
	InitializeShared();

	InitInterface();
	GLInterface->SetMode(GLInterfaceMode::MODE_DETECT);
	if (!GLInterface->Create(window_handle))
		return false;

	return true;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
	GLInterface->MakeCurrent();

	g_renderer = std::make_unique<Renderer>();

	g_vertex_manager = std::make_unique<VertexManager>();
	g_perf_query = GetPerfQuery();
	ProgramShaderCache::Init();
	g_texture_cache = std::make_unique<TextureCache>();
	g_sampler_cache = std::make_unique<SamplerCache>();
	Renderer::Init();
	TextureConverter::Init();
	BoundingBox::Init();
}

void VideoBackend::Shutdown()
{
	GLInterface->Shutdown();
	GLInterface.reset();
	ShutdownShared();
}

void VideoBackend::Video_Cleanup()
{
	// The following calls are NOT Thread Safe
	// And need to be called from the video thread
	CleanupShared();
	Renderer::Shutdown();
	BoundingBox::Shutdown();
	TextureConverter::Shutdown();
	g_sampler_cache.reset();
	g_texture_cache.reset();
	ProgramShaderCache::Shutdown();
	g_perf_query.reset();
	g_vertex_manager.reset();
	g_renderer.reset();
	GLInterface->ClearCurrent();
}

}
