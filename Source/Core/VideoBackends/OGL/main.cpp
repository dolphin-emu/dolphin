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

#include "Common/GL/GLInterfaceBase.h"
#include "Common/GL/GLUtil.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/PerfQuery.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureCache.h"
#include "VideoBackends/OGL/TextureConverter.h"
#include "VideoBackends/OGL/VertexManager.h"
#include "VideoBackends/OGL/VideoBackend.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoCommon.h"
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

void VideoBackend::InitBackendInfo()
{
  g_Config.backend_info.api_type = APIType::OpenGL;
  g_Config.backend_info.MaxTextureSize = 16384;
  g_Config.backend_info.bSupportsExclusiveFullscreen = false;
  g_Config.backend_info.bSupportsOversizedViewports = true;
  g_Config.backend_info.bSupportsGeometryShaders = true;
  g_Config.backend_info.bSupportsComputeShaders = false;
  g_Config.backend_info.bSupports3DVision = false;
  g_Config.backend_info.bSupportsPostProcessing = true;
  g_Config.backend_info.bSupportsSSAA = true;
  g_Config.backend_info.bSupportsReversedDepthRange = true;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsInternalResolutionFrameDumps = true;

  // TODO: There is a bug here, if texel buffers are not supported the graphics options
  // will show the option when it is not supported. The only way around this would be
  // creating a context when calling this function to determine what is available.
  g_Config.backend_info.bSupportsGPUTextureDecoding = true;

  // Overwritten in Render.cpp later
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsPrimitiveRestart = true;
  g_Config.backend_info.bSupportsPaletteConversion = true;
  g_Config.backend_info.bSupportsClipControl = true;
  g_Config.backend_info.bSupportsDepthClamp = true;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;

  g_Config.backend_info.Adapters.clear();

  // aamodes - 1 is to stay consistent with D3D (means no AA)
  g_Config.backend_info.AAModes = {1, 2, 4, 8};
}

bool VideoBackend::InitializeGLExtensions()
{
  // Init extension support.
  if (!GLExtensions::Init())
  {
    // OpenGL 2.0 is required for all shader based drawings. There is no way to get this by
    // extensions
    PanicAlert("GPU: OGL ERROR: Does your video card support OpenGL 2.0?");
    return false;
  }

  if (GLExtensions::Version() < 300)
  {
    // integer vertex attributes require a gl3 only function
    PanicAlert("GPU: OGL ERROR: Need OpenGL version 3.\n"
               "GPU: Does your video card support OpenGL 3?");
    return false;
  }

  return true;
}

bool VideoBackend::FillBackendInfo()
{
  // check for the max vertex attributes
  GLint numvertexattribs = 0;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numvertexattribs);
  if (numvertexattribs < 16)
  {
    PanicAlert("GPU: OGL ERROR: Number of attributes %d not enough.\n"
               "GPU: Does your video card support OpenGL 2.x?",
               numvertexattribs);
    return false;
  }

  // check the max texture width and height
  GLint max_texture_size = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
  g_Config.backend_info.MaxTextureSize = static_cast<u32>(max_texture_size);
  if (max_texture_size < 1024)
  {
    PanicAlert("GL_MAX_TEXTURE_SIZE too small at %i - must be at least 1024.", max_texture_size);
    return false;
  }

  // TODO: Move the remaining fields from the Renderer constructor here.
  return true;
}

bool VideoBackend::Initialize(void* window_handle)
{
  InitBackendInfo();
  InitializeShared();

  InitInterface();
  GLInterface->SetMode(GLInterfaceMode::MODE_DETECT);
  if (!GLInterface->Create(window_handle, g_ActiveConfig.iStereoMode == STEREO_QUADBUFFER))
    return false;

  return true;
}

// This is called after Initialize() from the Core
// Run from the graphics thread
void VideoBackend::Video_Prepare()
{
  GLInterface->MakeCurrent();
  if (!InitializeGLExtensions() || !FillBackendInfo())
  {
    // TODO: Handle this better. We'll likely end up crashing anyway, but this method doesn't
    // return anything, so we can't inform the caller that startup failed.
    return;
  }

  g_renderer = std::make_unique<Renderer>();

  g_vertex_manager = std::make_unique<VertexManager>();
  g_perf_query = GetPerfQuery();
  ProgramShaderCache::Init();
  g_texture_cache = std::make_unique<TextureCache>();
  g_sampler_cache = std::make_unique<SamplerCache>();
  static_cast<Renderer*>(g_renderer.get())->Init();
  TextureConverter::Init();
  BoundingBox::Init(g_renderer->GetTargetWidth(), g_renderer->GetTargetHeight());
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
  static_cast<Renderer*>(g_renderer.get())->Shutdown();
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
