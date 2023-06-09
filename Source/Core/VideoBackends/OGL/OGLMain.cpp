// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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

#include "VideoBackends/OGL/VideoBackend.h"

#include <memory>
#include <string>
#include <vector>

#include "Common/Common.h"
#include "Common/GL/GLContext.h"
#include "Common/GL/GLUtil.h"
#include "Common/MsgHandler.h"

#include "Core/Config/GraphicsSettings.h"

#include "VideoBackends/OGL/OGLBoundingBox.h"
#include "VideoBackends/OGL/OGLConfig.h"
#include "VideoBackends/OGL/OGLGfx.h"
#include "VideoBackends/OGL/OGLPerfQuery.h"
#include "VideoBackends/OGL/OGLVertexManager.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/SamplerCache.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
std::string VideoBackend::GetName() const
{
  return NAME;
}

std::string VideoBackend::GetDisplayName() const
{
  if (g_ogl_config.bIsES)
    return _trans("OpenGL ES");
  else
    return _trans("OpenGL");
}

void VideoBackend::InitBackendInfo(const WindowSystemInfo& wsi)
{
  std::unique_ptr<GLContext> temp_gl_context =
      GLContext::Create(wsi, g_Config.stereo_mode == StereoMode::QuadBuffer, true, false,
                        Config::Get(Config::GFX_PREFER_GLES));

  if (!temp_gl_context)
    return;

  FillBackendInfo(temp_gl_context.get());
}

bool VideoBackend::InitializeGLExtensions(GLContext* context)
{
  // Init extension support.
  if (!GLExtensions::Init(context))
  {
    // OpenGL 2.0 is required for all shader based drawings. There is no way to get this by
    // extensions
    PanicAlertFmtT("GPU: OGL ERROR: Does your video card support OpenGL 2.0?");
    return false;
  }

  if (GLExtensions::Version() < 300)
  {
    // integer vertex attributes require a gl3 only function
    PanicAlertFmtT("GPU: OGL ERROR: Need OpenGL version 3.\n"
                   "GPU: Does your video card support OpenGL 3?");
    return false;
  }

  return true;
}

bool VideoBackend::FillBackendInfo(GLContext* context)
{
  if (!InitializeGLExtensions(context))
    return false;

  g_Config.backend_info.api_type = APIType::OpenGL;
  g_Config.backend_info.MaxTextureSize = 16384;
  g_Config.backend_info.bUsesLowerLeftOrigin = true;
  g_Config.backend_info.bSupportsExclusiveFullscreen = false;
  g_Config.backend_info.bSupportsGeometryShaders = true;
  g_Config.backend_info.bSupportsComputeShaders = false;
  g_Config.backend_info.bSupports3DVision = false;
  g_Config.backend_info.bSupportsPostProcessing = true;
  g_Config.backend_info.bSupportsSSAA = true;
  g_Config.backend_info.bSupportsReversedDepthRange = true;
  g_Config.backend_info.bSupportsLogicOp = true;
  g_Config.backend_info.bSupportsMultithreading = false;
  g_Config.backend_info.bSupportsCopyToVram = true;
  g_Config.backend_info.bSupportsLargePoints = true;
  g_Config.backend_info.bSupportsDepthReadback = true;
  g_Config.backend_info.bSupportsPartialDepthCopies = true;
  g_Config.backend_info.bSupportsShaderBinaries = false;
  g_Config.backend_info.bSupportsPipelineCacheData = false;
  g_Config.backend_info.bSupportsLodBiasInSampler = true;
  g_Config.backend_info.bSupportsPartialMultisampleResolve = true;
  // Unneccessary since OGL doesn't use pipelines
  g_Config.backend_info.bSupportsDynamicVertexLoader = false;

  // TODO: There is a bug here, if texel buffers or SSBOs/atomics are not supported the graphics
  // options will show the option when it is not supported. The only way around this would be
  // creating a context when calling this function to determine what is available.
  g_Config.backend_info.bSupportsGPUTextureDecoding = true;
  g_Config.backend_info.bSupportsBBox = true;

  // Overwritten in OGLConfig.cpp later
  g_Config.backend_info.bSupportsDualSourceBlend = true;
  g_Config.backend_info.bSupportsPrimitiveRestart = true;
  g_Config.backend_info.bSupportsPaletteConversion = true;
  g_Config.backend_info.bSupportsClipControl = true;
  g_Config.backend_info.bSupportsDepthClamp = true;
  g_Config.backend_info.bSupportsST3CTextures = false;
  g_Config.backend_info.bSupportsBPTCTextures = false;
  g_Config.backend_info.bSupportsCoarseDerivatives = false;
  g_Config.backend_info.bSupportsTextureQueryLevels = false;
  g_Config.backend_info.bSupportsSettingObjectNames = false;

  g_Config.backend_info.bUsesExplictQuadBuffering = true;

  g_Config.backend_info.Adapters.clear();

  // aamodes - 1 is to stay consistent with D3D (means no AA)
  g_Config.backend_info.AAModes = {1, 2, 4, 8};

  // check for the max vertex attributes
  GLint numvertexattribs = 0;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numvertexattribs);
  if (numvertexattribs < 16)
  {
    PanicAlertFmtT("GPU: OGL ERROR: Number of attributes {0} not enough.\n"
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
    PanicAlertFmtT("GL_MAX_TEXTURE_SIZE is {0} - must be at least 1024.", max_texture_size);
    return false;
  }

  if (!PopulateConfig(context))
  {
    // Not all needed extensions are supported, so we have to stop here.
    // Else some of the next calls might crash.
    return false;
  }

  // TODO: Move the remaining fields from the Renderer constructor here.
  return true;
}

bool VideoBackend::Initialize(const WindowSystemInfo& wsi)
{
  std::unique_ptr<GLContext> main_gl_context =
      GLContext::Create(wsi, g_Config.stereo_mode == StereoMode::QuadBuffer, true, false,
                        Config::Get(Config::GFX_PREFER_GLES));
  if (!main_gl_context)
    return false;

  if (!FillBackendInfo(main_gl_context.get()))
    return false;

  auto gfx = std::make_unique<OGLGfx>(std::move(main_gl_context), wsi.render_surface_scale);
  ProgramShaderCache::Init();
  g_sampler_cache = std::make_unique<SamplerCache>();

  auto vertex_manager = std::make_unique<VertexManager>();
  auto perf_query = GetPerfQuery(gfx->IsGLES());
  auto bounding_box = std::make_unique<OGLBoundingBox>();

  return InitializeShared(std::move(gfx), std::move(vertex_manager), std::move(perf_query),
                          std::move(bounding_box));
}

void VideoBackend::Shutdown()
{
  ShutdownShared();

  ProgramShaderCache::Shutdown();
  g_sampler_cache.reset();
}
}  // namespace OGL
