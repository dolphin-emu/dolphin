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

#ifndef _MAIN_H_
#define _MAIN_H_

#include "PluginSpecs_Video.h"

#include "Renderer.h"
#include "TextureCache.h"
#include "VertexManager.h"
#include "PixelShaderCache.h"
#include "VertexShaderCache.h"
#include "FramebufferManager.h"

extern SVideoInitialize g_VideoInitialize;
extern volatile u32 s_swapRequested;
extern PLUGIN_GLOBALS *g_globals;

extern RendererBase *g_renderer;
extern TextureCacheBase *g_texture_cache;
extern VertexManagerBase *g_vertex_manager;
extern VertexShaderCacheBase* g_vertex_shader_cache;
extern PixelShaderCacheBase* g_pixel_shader_cache;
extern FramebufferManagerBase* g_framebuffer_manager;

extern int frameCount;

void VideoFifo_CheckEFBAccess();
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight);

enum
{
	GFXAPI_SOFTWARE,
	GFXAPI_OPENGL,
	GFXAPI_D3D9,
	GFXAPI_D3D11,
};

extern const char* const g_gfxapi_names[];
extern int g_gfxapi;

#endif
