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

#include "Common/Common.h"
#include "Common/GL/GLContext.h"
#include "Common/GL/GLUtil.h"
#include "Common/MsgHandler.h"

#include "Core/Config/GraphicsSettings.h"

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/PerfQuery.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/VertexManager.h"
#include "VideoBackends/OGL/VideoBackend.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
std::string VideoBackend::GetName() const
{
  return "OGL";
}

std::string VideoBackend::GetDisplayName() const
{
  if (g_ogl_config.bIsES)
    return _trans("OpenGL ES");
  else
    return _trans("OpenGL");
}

void VideoBackend::InitBackendInfo()
{
  g_Config.backend_info.api_type = APIType::OpenGL;
  g_Config.backend_info.MaxTextureSize = 16384;
  g_Config.backend_info.bUsesLowerLeftOrigin = true;
  g_Config.backend_info.bSupportsExclusiveFullscreen = false;
  g_Config.backend_info.bSupportsOversizedViewports = true;
  g_Config.backend_info.bSupportsGeometryShaders = true;
  g_Config.backend_info.bSupportsComputeShaders = false;
  g_Config.backend_info.bSupportsPostProcessing = true;
  g_Config.backend_info.bSupportsSSAA = true;
  g_Config.backend_info.bSupportsReversedDepthRange = true;
  g_Config.backend_info.bSupportsLogicOp = true;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsCopyToVram = true;
  g_Config.backend_info.bSupportsLargePoints = true;
  g_Config.backend_info.bSupportsPartialDepthCopies = true;
  g_Config.backend_info.bSupportsShaderBinaries = false;
  g_Config.backend_info.bSupportsPipelineCacheData = false;

  // TODO: There is a bug here, if texel buffers or SSBOs/atomics are not supported the graphics
  // options will show the option when it is not supported. The only way around this would be
  // creating a context when calling this function to determine what is available.
  g_Config.backend_info.bSupportsGPUTextureDecoding = true;
  g_Config.backend_info.bSupportsBBox = true;

  // Overwritten in Render.cpp later
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsPaletteConversion = true;
  g_Config.backend_info.bSupportsClipControl = true;
  g_Config.backend_info.bSupportsDepthClamp = true;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;

  g_Config.backend_info.Adapters.clear();

  // aamodes - 1 is to stay consistent with D3D (means no AA)
  g_Config.backend_info.AAModes = {1, 2, 4, 8};
}

bool VideoBackend::InitializeGLExtensions(GLContext* context)
{
  // Init extension support.
  if (!GLExtensions::Init(context))
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
  InitBackendInfo();

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

bool VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  std::unique_ptr<GLContext> main_gl_context =
      GLContext::Create(wsi, true, false,
                        Config::Get(Config::GFX_PREFER_GLES));
  if (!main_gl_context)
    return false;

  if (!InitializeGLExtensions(main_gl_context.get()) || !FillBackendInfo())
    return false;

  InitializeShared();
  g_renderer = std::make_unique<Renderer>(std::move(main_gl_context), wsi.render_surface_scale);
  ProgramShaderCache::Init();
  g_vertex_manager = std::make_unique<VertexManager>();
  g_shader_cache = std::make_unique<VideoCommon::ShaderCache>();
  g_framebuffer_manager = std::make_unique<FramebufferManager>();
  g_perf_query = GetPerfQuery();
  g_texture_cache = std::make_unique<TextureCacheBase>();
  g_sampler_cache = std::make_unique<SamplerCache>();
  BoundingBox::Init();

  if (!g_vertex_manager->Initialize() || !g_shader_cache->Initialize() ||
      !g_renderer->Initialize() || !g_framebuffer_manager->Initialize() ||
      !g_texture_cache->Initialize())
  {
    PanicAlert("Failed to initialize renderer classes");
    Shutdown();
    return false;
  }

  g_shader_cache->InitializeShaderCache();
  return true;
}

void VideoBackend::Shutdown()
{
  g_shader_cache->Shutdown();
  g_renderer->Shutdown();
  BoundingBox::Shutdown();
  g_sampler_cache.reset();
  g_texture_cache.reset();
  g_perf_query.reset();
  g_vertex_manager.reset();
  g_framebuffer_manager.reset();
  g_shader_cache.reset();
  ProgramShaderCache::Shutdown();
  g_renderer.reset();
  ShutdownShared();
}
}  // namespace OGL
