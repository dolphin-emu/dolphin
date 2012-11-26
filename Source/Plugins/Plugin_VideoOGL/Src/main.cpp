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
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "VertexShaderManager.h"
#include "CommandProcessor.h"
#include "PixelEngine.h"
#include "TextureConverter.h"
#include "PostProcessing.h"
#include "OnScreenDisplay.h"
#include "DLCache.h"
#include "FramebufferManager.h"
#include "Core.h"
#include "Host.h"

#include "VideoState.h"
#include "VideoBackend.h"
#include "ConfigManager.h"

namespace OGL
{

std::string VideoBackend::GetName()
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
                        if (!strcasecmp(name.substr(name.size() - 4).c_str(), ".txt"))
                                name = name.substr(0, name.size() - 4);
                        shaders.push_back(name);
                }
        }
        else
        {
                File::CreateDir(File::GetUserPath(D_SHADERS_IDX).c_str());
        }
}

void InitBackendInfo()
{
	g_Config.backend_info.APIType = API_OPENGL;
	g_Config.backend_info.bUseRGBATextures = false;
	g_Config.backend_info.bSupports3DVision = false;
	g_Config.backend_info.bSupportsDualSourceBlend = false; // supported, but broken
	g_Config.backend_info.bSupportsFormatReinterpretation = false;
	g_Config.backend_info.bSupportsPixelLighting = true;

	// aamodes
	const char* caamodes[] = {"None", "2x", "4x", "8x", "8x CSAA", "8xQ CSAA", "16x CSAA", "16xQ CSAA"};
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

	if (!OpenGL_Create(window_handle))
		return false;

	s_BackendInitialized = true;

	return true;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
	OpenGL_MakeCurrent();

	g_renderer = new Renderer;

	s_efbAccessRequested = false;
	s_FifoShuttingDown = false;
	s_swapRequested = false;

	CommandProcessor::Init();
	PixelEngine::Init();

	g_texture_cache = new TextureCache;

	BPInit();
	g_vertex_manager = new VertexManager;
	Fifo_Init(); // must be done before OpcodeDecoder_Init()
	OpcodeDecoder_Init();
	VertexShaderCache::Init();
	VertexShaderManager::Init();
	PixelShaderCache::Init();
	PixelShaderManager::Init();
	PostProcessing::Init();
	GL_REPORT_ERRORD();
	VertexLoaderManager::Init();
	TextureConverter::Init();
	DLCache::Init();

	// Notify the core that the video backend is ready
	Host_Message(WM_USER_CREATE);
}

void VideoBackend::Shutdown()
{
	s_BackendInitialized = false;

	if (g_renderer)
	{
		s_efbAccessRequested = false;
		s_FifoShuttingDown = false;
		s_swapRequested = false;
		DLCache::Shutdown();
		Fifo_Shutdown();
		PostProcessing::Shutdown();

		// The following calls are NOT Thread Safe
		// And need to be called from the video thread
		TextureConverter::Shutdown();
		VertexLoaderManager::Shutdown();
		VertexShaderCache::Shutdown();
		VertexShaderManager::Shutdown();
		PixelShaderManager::Shutdown();
		PixelShaderCache::Shutdown();
		delete g_vertex_manager;
		delete g_texture_cache;
		OpcodeDecoder_Shutdown();
		delete g_renderer;
		g_renderer = NULL;
		g_texture_cache = NULL;
	}
	OpenGL_Shutdown();
}

}
