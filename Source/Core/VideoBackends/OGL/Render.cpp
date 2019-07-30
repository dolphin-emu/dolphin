// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/Render.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "Common/Assert.h"
#include "Common/Atomic.h"
#include "Common/CommonTypes.h"
#include "Common/GL/GLContext.h"
#include "Common/GL/GLUtil.h"
#include "Common/Logging/LogManager.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Core.h"

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/OGLPipeline.h"
#include "VideoBackends/OGL/OGLShader.h"
#include "VideoBackends/OGL/OGLTexture.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/StreamBuffer.h"
#include "VideoBackends/OGL/VertexManager.h"

#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

namespace OGL
{
VideoConfig g_ogl_config;

static void APIENTRY ErrorCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                   GLsizei length, const char* message, const void* userParam)
{
  const char* s_source;
  const char* s_type;

  // Performance - DualCore driver performance warning:
  // DualCore application thread syncing with server thread
  if (id == 0x200b0)
    return;

  switch (source)
  {
  case GL_DEBUG_SOURCE_API_ARB:
    s_source = "API";
    break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
    s_source = "Window System";
    break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
    s_source = "Shader Compiler";
    break;
  case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
    s_source = "Third Party";
    break;
  case GL_DEBUG_SOURCE_APPLICATION_ARB:
    s_source = "Application";
    break;
  case GL_DEBUG_SOURCE_OTHER_ARB:
    s_source = "Other";
    break;
  default:
    s_source = "Unknown";
    break;
  }
  switch (type)
  {
  case GL_DEBUG_TYPE_ERROR_ARB:
    s_type = "Error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
    s_type = "Deprecated";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
    s_type = "Undefined";
    break;
  case GL_DEBUG_TYPE_PORTABILITY_ARB:
    s_type = "Portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE_ARB:
    s_type = "Performance";
    break;
  case GL_DEBUG_TYPE_OTHER_ARB:
    s_type = "Other";
    break;
  default:
    s_type = "Unknown";
    break;
  }
  switch (severity)
  {
  case GL_DEBUG_SEVERITY_HIGH_ARB:
    ERROR_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message);
    break;
  case GL_DEBUG_SEVERITY_MEDIUM_ARB:
    WARN_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message);
    break;
  case GL_DEBUG_SEVERITY_LOW_ARB:
    DEBUG_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message);
    break;
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    DEBUG_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message);
    break;
  default:
    ERROR_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message);
    break;
  }
}

// Two small Fallbacks to avoid GL_ARB_ES2_compatibility
static void APIENTRY DepthRangef(GLfloat neardepth, GLfloat fardepth)
{
  glDepthRange(neardepth, fardepth);
}
static void APIENTRY ClearDepthf(GLfloat depthval)
{
  glClearDepth(depthval);
}

static void InitDriverInfo()
{
  std::string svendor = std::string(g_ogl_config.gl_vendor);
  std::string srenderer = std::string(g_ogl_config.gl_renderer);
  std::string sversion = std::string(g_ogl_config.gl_version);
  DriverDetails::Vendor vendor = DriverDetails::VENDOR_UNKNOWN;
  DriverDetails::Driver driver = DriverDetails::DRIVER_UNKNOWN;
  DriverDetails::Family family = DriverDetails::Family::UNKNOWN;
  double version = 0.0;

  // Get the vendor first
  if (svendor == "NVIDIA Corporation" && srenderer != "NVIDIA Tegra")
  {
    vendor = DriverDetails::VENDOR_NVIDIA;
  }
  else if (svendor == "ATI Technologies Inc." || svendor == "Advanced Micro Devices, Inc.")
  {
    vendor = DriverDetails::VENDOR_ATI;
  }
  else if (std::string::npos != sversion.find("Mesa"))
  {
    vendor = DriverDetails::VENDOR_MESA;
  }
  else if (std::string::npos != svendor.find("Intel"))
  {
    vendor = DriverDetails::VENDOR_INTEL;
  }
  else if (svendor == "ARM")
  {
    vendor = DriverDetails::VENDOR_ARM;
  }
  else if (svendor == "http://limadriver.org/")
  {
    vendor = DriverDetails::VENDOR_ARM;
    driver = DriverDetails::DRIVER_LIMA;
  }
  else if (svendor == "Qualcomm")
  {
    vendor = DriverDetails::VENDOR_QUALCOMM;
  }
  else if (svendor == "Imagination Technologies")
  {
    vendor = DriverDetails::VENDOR_IMGTEC;
  }
  else if (svendor == "NVIDIA Corporation" && srenderer == "NVIDIA Tegra")
  {
    vendor = DriverDetails::VENDOR_TEGRA;
  }
  else if (svendor == "Vivante Corporation")
  {
    vendor = DriverDetails::VENDOR_VIVANTE;
  }

  // Get device family and driver version...if we care about it
  switch (vendor)
  {
  case DriverDetails::VENDOR_QUALCOMM:
  {
    driver = DriverDetails::DRIVER_QUALCOMM;
    double glVersion;
    sscanf(g_ogl_config.gl_version, "OpenGL ES %lg V@%lg", &glVersion, &version);
  }
  break;
  case DriverDetails::VENDOR_ARM:
    // Currently the Mali-T line has two families in it.
    // Mali-T6xx and Mali-T7xx
    // These two families are similar enough that they share bugs in their drivers.
    //
    // Mali drivers provide no way to explicitly find out what video driver is running.
    // This is similar to how we can't find the Nvidia driver version in Windows.
    // Good thing is that ARM introduces a new video driver about once every two years so we can
    // find the driver version by the features it exposes.
    // r2p0 - No OpenGL ES 3.0 support (We don't support this)
    // r3p0 - OpenGL ES 3.0 support
    // r4p0 - Supports 'GL_EXT_shader_pixel_local_storage' extension.

    driver = DriverDetails::DRIVER_ARM;
    if (GLExtensions::Supports("GL_EXT_shader_pixel_local_storage"))
      version = 400;
    else
      version = 300;
    break;
  case DriverDetails::VENDOR_MESA:
  {
    if (svendor == "nouveau")
    {
      driver = DriverDetails::DRIVER_NOUVEAU;
    }
    else if (svendor == "Intel Open Source Technology Center")
    {
      driver = DriverDetails::DRIVER_I965;
      if (srenderer.find("Sandybridge") != std::string::npos)
        family = DriverDetails::Family::INTEL_SANDY;
      else if (srenderer.find("Ivybridge") != std::string::npos)
        family = DriverDetails::Family::INTEL_IVY;
    }
    else if (std::string::npos != srenderer.find("AMD") ||
             std::string::npos != srenderer.find("ATI"))
    {
      driver = DriverDetails::DRIVER_R600;
    }

    int major = 0;
    int minor = 0;
    int release = 0;
    sscanf(g_ogl_config.gl_version, "%*s (Core Profile) Mesa %d.%d.%d", &major, &minor, &release);
    version = 100 * major + 10 * minor + release;
  }
  break;
  case DriverDetails::VENDOR_INTEL:  // Happens in OS X/Windows
  {
    u32 market_name;
    sscanf(g_ogl_config.gl_renderer, "Intel HD Graphics %d", &market_name);
    switch (market_name)
    {
    case 2000:
    case 3000:
      family = DriverDetails::Family::INTEL_SANDY;
      break;
    case 2500:
    case 4000:
      family = DriverDetails::Family::INTEL_IVY;
      break;
    default:
      family = DriverDetails::Family::UNKNOWN;
      break;
    };
#ifdef _WIN32
    int glmajor = 0;
    int glminor = 0;
    int major = 0;
    int minor = 0;
    int release = 0;
    int revision = 0;
    // Example version string: '4.3.0 - Build 10.18.10.3907'
    sscanf(g_ogl_config.gl_version, "%d.%d.0 - Build %d.%d.%d.%d", &glmajor, &glminor, &major,
           &minor, &release, &revision);
    version = 100000000 * major + 1000000 * minor + 10000 * release + revision;
    version /= 10000;
#endif
  }
  break;
  case DriverDetails::VENDOR_NVIDIA:
  {
    int glmajor = 0;
    int glminor = 0;
    int glrelease = 0;
    int major = 0;
    int minor = 0;
    // TODO: this is known to be broken on Windows
    // Nvidia seems to have removed their driver version from this string, so we can't get it.
    // hopefully we'll never have to workaround Nvidia bugs
    sscanf(g_ogl_config.gl_version, "%d.%d.%d NVIDIA %d.%d", &glmajor, &glminor, &glrelease, &major,
           &minor);
    version = 100 * major + minor;
  }
  break;
  case DriverDetails::VENDOR_IMGTEC:
  {
    // Example version string:
    // "OpenGL ES 3.2 build 1.9@4850625"
    // Ends up as "109.4850625" - "1.9" being the branch, "4850625" being the build's change ID
    // The change ID only makes sense to compare within a branch
    driver = DriverDetails::DRIVER_IMGTEC;
    double gl_version;
    int major, minor, change;
    constexpr double change_scale = 10000000;
    sscanf(g_ogl_config.gl_version, "OpenGL ES %lg build %d.%d@%d", &gl_version, &major, &minor,
           &change);
    version = 100 * major + minor;
    if (change >= change_scale)
    {
      ERROR_LOG(VIDEO, "Version changeID overflow - change:%d scale:%f", change, change_scale);
    }
    else
    {
      version += static_cast<double>(change) / change_scale;
    }
  }
  break;
  // We don't care about these
  default:
    break;
  }
  DriverDetails::Init(DriverDetails::API_OPENGL, vendor, driver, version, family);
}

// Init functions
Renderer::Renderer(std::unique_ptr<GLContext> main_gl_context, float backbuffer_scale)
    : ::Renderer(static_cast<int>(std::max(main_gl_context->GetBackBufferWidth(), 1u)),
                 static_cast<int>(std::max(main_gl_context->GetBackBufferHeight(), 1u)),
                 backbuffer_scale, AbstractTextureFormat::RGBA8),
      m_main_gl_context(std::move(main_gl_context)),
      m_current_rasterization_state(RenderState::GetInvalidRasterizationState()),
      m_current_depth_state(RenderState::GetInvalidDepthState()),
      m_current_blend_state(RenderState::GetInvalidBlendingState())
{
  // Create the window framebuffer.
  if (!m_main_gl_context->IsHeadless())
  {
    m_system_framebuffer = std::make_unique<OGLFramebuffer>(
        nullptr, nullptr, AbstractTextureFormat::RGBA8, AbstractTextureFormat::Undefined,
        std::max(m_main_gl_context->GetBackBufferWidth(), 1u),
        std::max(m_main_gl_context->GetBackBufferHeight(), 1u), 1, 1, 0);
    m_current_framebuffer = m_system_framebuffer.get();
  }

  bool bSuccess = true;
  bool supports_glsl_cache = false;

  g_ogl_config.gl_vendor = (const char*)glGetString(GL_VENDOR);
  g_ogl_config.gl_renderer = (const char*)glGetString(GL_RENDERER);
  g_ogl_config.gl_version = (const char*)glGetString(GL_VERSION);

  InitDriverInfo();

  if (!m_main_gl_context->IsGLES())
  {
    if (!GLExtensions::Supports("GL_ARB_framebuffer_object"))
    {
      // We want the ogl3 framebuffer instead of the ogl2 one for better blitting support.
      // It's also compatible with the gles3 one.
      PanicAlert("GPU: ERROR: Need GL_ARB_framebuffer_object for multiple render targets.\n"
                 "GPU: Does your video card support OpenGL 3.0?");
      bSuccess = false;
    }

    if (!GLExtensions::Supports("GL_ARB_vertex_array_object"))
    {
      // This extension is used to replace lots of pointer setting function.
      // Also gles3 requires to use it.
      PanicAlert("GPU: OGL ERROR: Need GL_ARB_vertex_array_object.\n"
                 "GPU: Does your video card support OpenGL 3.0?");
      bSuccess = false;
    }

    if (!GLExtensions::Supports("GL_ARB_map_buffer_range"))
    {
      // ogl3 buffer mapping for better streaming support.
      // The ogl2 one also isn't in gles3.
      PanicAlert("GPU: OGL ERROR: Need GL_ARB_map_buffer_range.\n"
                 "GPU: Does your video card support OpenGL 3.0?");
      bSuccess = false;
    }

    if (!GLExtensions::Supports("GL_ARB_uniform_buffer_object"))
    {
      // ubo allow us to keep the current constants on shader switches
      // we also can stream them much nicer and pack into it whatever we want to
      PanicAlert("GPU: OGL ERROR: Need GL_ARB_uniform_buffer_object.\n"
                 "GPU: Does your video card support OpenGL 3.1?");
      bSuccess = false;
    }
    else if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_UBO))
    {
      PanicAlert(
          "Buggy GPU driver detected.\n"
          "Please either install the closed-source GPU driver or update your Mesa 3D version.");
      bSuccess = false;
    }

    if (!GLExtensions::Supports("GL_ARB_sampler_objects"))
    {
      // Our sampler cache uses this extension. It could easyly be workaround and it's by far the
      // highest requirement, but it seems that no driver lacks support for it.
      PanicAlert("GPU: OGL ERROR: Need GL_ARB_sampler_objects.\n"
                 "GPU: Does your video card support OpenGL 3.3?");
      bSuccess = false;
    }

    // OpenGL 3 doesn't provide GLES like float functions for depth.
    // They are in core in OpenGL 4.1, so almost every driver should support them.
    // But for the oldest ones, we provide fallbacks to the old double functions.
    if (!GLExtensions::Supports("GL_ARB_ES2_compatibility"))
    {
      glDepthRangef = DepthRangef;
      glClearDepthf = ClearDepthf;
    }
  }

  // Copy the GPU name to g_Config, so Analytics can see it.
  g_Config.backend_info.AdapterName = g_ogl_config.gl_renderer;

  g_Config.backend_info.bSupportsDualSourceBlend =
      (GLExtensions::Supports("GL_ARB_blend_func_extended") ||
       GLExtensions::Supports("GL_EXT_blend_func_extended"));
  g_Config.backend_info.bSupportsFragmentStoresAndAtomics =
      GLExtensions::Supports("GL_ARB_shader_storage_buffer_object");
  g_Config.backend_info.bSupportsBBox = g_Config.backend_info.bSupportsFragmentStoresAndAtomics;
  g_Config.backend_info.bSupportsGSInstancing = GLExtensions::Supports("GL_ARB_gpu_shader5");
  g_Config.backend_info.bSupportsSSAA = GLExtensions::Supports("GL_ARB_gpu_shader5") &&
                                        GLExtensions::Supports("GL_ARB_sample_shading");
  g_Config.backend_info.bSupportsGeometryShaders =
      GLExtensions::Version() >= 320 &&
      !DriverDetails::HasBug(DriverDetails::BUG_BROKEN_GEOMETRY_SHADERS);
  g_Config.backend_info.bSupportsPaletteConversion =
      GLExtensions::Supports("GL_ARB_texture_buffer_object") ||
      GLExtensions::Supports("GL_OES_texture_buffer") ||
      GLExtensions::Supports("GL_EXT_texture_buffer");
  g_Config.backend_info.bSupportsClipControl = GLExtensions::Supports("GL_ARB_clip_control") ||
                                               GLExtensions::Supports("GL_EXT_clip_control");
  g_ogl_config.bSupportsCopySubImage =
      (GLExtensions::Supports("GL_ARB_copy_image") || GLExtensions::Supports("GL_NV_copy_image") ||
       GLExtensions::Supports("GL_EXT_copy_image") ||
       GLExtensions::Supports("GL_OES_copy_image")) &&
      !DriverDetails::HasBug(DriverDetails::BUG_BROKEN_COPYIMAGE);
  g_ogl_config.bSupportsTextureSubImage = GLExtensions::Supports("ARB_get_texture_sub_image");

  // Desktop OpenGL supports the binding layout if it supports 420pack
  // OpenGL ES 3.1 supports it implicitly without an extension
  g_Config.backend_info.bSupportsBindingLayout =
      GLExtensions::Supports("GL_ARB_shading_language_420pack");

  // Clip distance support is useless without a method to clamp the depth range
  g_Config.backend_info.bSupportsDepthClamp = GLExtensions::Supports("GL_ARB_depth_clamp");

  // Desktop OpenGL supports bitfield manulipation and dynamic sampler indexing if it supports
  // shader5. OpenGL ES 3.1 supports it implicitly without an extension
  g_Config.backend_info.bSupportsBitfield = GLExtensions::Supports("GL_ARB_gpu_shader5");
  g_Config.backend_info.bSupportsDynamicSamplerIndexing =
      GLExtensions::Supports("GL_ARB_gpu_shader5");

  g_ogl_config.bIsES = m_main_gl_context->IsGLES();
  supports_glsl_cache = GLExtensions::Supports("GL_ARB_get_program_binary");
  g_ogl_config.bSupportsGLPinnedMemory = GLExtensions::Supports("GL_AMD_pinned_memory");
  g_ogl_config.bSupportsGLSync = GLExtensions::Supports("GL_ARB_sync");
  g_ogl_config.bSupportsGLBaseVertex = GLExtensions::Supports("GL_ARB_draw_elements_base_vertex") ||
                                       GLExtensions::Supports("GL_EXT_draw_elements_base_vertex") ||
                                       GLExtensions::Supports("GL_OES_draw_elements_base_vertex");
  g_ogl_config.bSupportsGLBufferStorage = GLExtensions::Supports("GL_ARB_buffer_storage") ||
                                          GLExtensions::Supports("GL_EXT_buffer_storage");
  g_ogl_config.bSupportsMSAA = GLExtensions::Supports("GL_ARB_texture_multisample");
  g_ogl_config.bSupportViewportFloat = GLExtensions::Supports("GL_ARB_viewport_array");
  g_ogl_config.bSupportsDebug =
      GLExtensions::Supports("GL_KHR_debug") || GLExtensions::Supports("GL_ARB_debug_output");
  g_ogl_config.bSupportsTextureStorage = GLExtensions::Supports("GL_ARB_texture_storage");
  g_ogl_config.bSupports3DTextureStorageMultisample =
      GLExtensions::Supports("GL_ARB_texture_storage_multisample") ||
      GLExtensions::Supports("GL_OES_texture_storage_multisample_2d_array");
  g_ogl_config.bSupports2DTextureStorageMultisample =
      GLExtensions::Supports("GL_ARB_texture_storage_multisample");
  g_ogl_config.bSupportsImageLoadStore = GLExtensions::Supports("GL_ARB_shader_image_load_store");
  g_ogl_config.bSupportsConservativeDepth = GLExtensions::Supports("GL_ARB_conservative_depth");
  g_ogl_config.bSupportsAniso = GLExtensions::Supports("GL_EXT_texture_filter_anisotropic");
  g_Config.backend_info.bSupportsComputeShaders = GLExtensions::Supports("GL_ARB_compute_shader");
  g_Config.backend_info.bSupportsST3CTextures =
      GLExtensions::Supports("GL_EXT_texture_compression_s3tc");
  g_Config.backend_info.bSupportsBPTCTextures =
      GLExtensions::Supports("GL_ARB_texture_compression_bptc");

  if (m_main_gl_context->IsGLES())
  {
    g_ogl_config.SupportedESPointSize =
        GLExtensions::Supports("GL_OES_geometry_point_size") ?
            1 :
            GLExtensions::Supports("GL_EXT_geometry_point_size") ? 2 : 0;
    g_ogl_config.SupportedESTextureBuffer = GLExtensions::Supports("VERSION_GLES_3_2") ?
                                                EsTexbufType::TexbufCore :
                                                GLExtensions::Supports("GL_OES_texture_buffer") ?
                                                EsTexbufType::TexbufOes :
                                                GLExtensions::Supports("GL_EXT_texture_buffer") ?
                                                EsTexbufType::TexbufExt :
                                                EsTexbufType::TexbufNone;

    supports_glsl_cache = true;
    g_ogl_config.bSupportsGLSync = true;

    // TODO: Implement support for GL_EXT_clip_cull_distance when there is an extension for
    // depth clamping.
    g_Config.backend_info.bSupportsDepthClamp = false;

    // GLES does not support logic op.
    g_Config.backend_info.bSupportsLogicOp = false;

    if (GLExtensions::Supports("GL_EXT_shader_framebuffer_fetch"))
    {
      g_ogl_config.SupportedFramebufferFetch = EsFbFetchType::FbFetchExt;
    }
    else if (GLExtensions::Supports("GL_ARM_shader_framebuffer_fetch"))
    {
      g_ogl_config.SupportedFramebufferFetch = EsFbFetchType::FbFetchArm;
    }
    else
    {
      g_ogl_config.SupportedFramebufferFetch = EsFbFetchType::FbFetchNone;
    }
    g_Config.backend_info.bSupportsFramebufferFetch =
        g_ogl_config.SupportedFramebufferFetch != EsFbFetchType::FbFetchNone;

    if (GLExtensions::Version() == 300)
    {
      g_ogl_config.eSupportedGLSLVersion = GlslEs300;
      g_ogl_config.bSupportsAEP = false;
      g_ogl_config.bSupportsTextureStorage = true;
      g_Config.backend_info.bSupportsGeometryShaders = false;
    }
    else if (GLExtensions::Version() == 310)
    {
      g_ogl_config.eSupportedGLSLVersion = GlslEs310;
      g_ogl_config.bSupportsAEP = GLExtensions::Supports("GL_ANDROID_extension_pack_es31a");
      g_Config.backend_info.bSupportsBindingLayout = true;
      g_ogl_config.bSupportsImageLoadStore = true;
      g_Config.backend_info.bSupportsGeometryShaders = g_ogl_config.bSupportsAEP;
      g_Config.backend_info.bSupportsComputeShaders = true;
      g_Config.backend_info.bSupportsGSInstancing =
          g_Config.backend_info.bSupportsGeometryShaders && g_ogl_config.SupportedESPointSize > 0;
      g_Config.backend_info.bSupportsSSAA = g_ogl_config.bSupportsAEP;
      g_Config.backend_info.bSupportsFragmentStoresAndAtomics = true;
      g_ogl_config.bSupportsMSAA = true;
      g_ogl_config.bSupportsTextureStorage = true;
      g_ogl_config.bSupports2DTextureStorageMultisample = true;
      g_Config.backend_info.bSupportsBitfield = true;
      g_Config.backend_info.bSupportsDynamicSamplerIndexing = g_ogl_config.bSupportsAEP;
    }
    else
    {
      g_ogl_config.eSupportedGLSLVersion = GlslEs320;
      g_ogl_config.bSupportsAEP = GLExtensions::Supports("GL_ANDROID_extension_pack_es31a");
      g_Config.backend_info.bSupportsBindingLayout = true;
      g_ogl_config.bSupportsImageLoadStore = true;
      g_Config.backend_info.bSupportsGeometryShaders = true;
      g_Config.backend_info.bSupportsComputeShaders = true;
      g_Config.backend_info.bSupportsGSInstancing = g_ogl_config.SupportedESPointSize > 0;
      g_Config.backend_info.bSupportsPaletteConversion = true;
      g_Config.backend_info.bSupportsSSAA = true;
      g_Config.backend_info.bSupportsFragmentStoresAndAtomics = true;
      g_ogl_config.bSupportsCopySubImage = true;
      g_ogl_config.bSupportsGLBaseVertex = true;
      g_ogl_config.bSupportsDebug = true;
      g_ogl_config.bSupportsMSAA = true;
      g_ogl_config.bSupportsTextureStorage = true;
      g_ogl_config.bSupports2DTextureStorageMultisample = true;
      g_ogl_config.bSupports3DTextureStorageMultisample = true;
      g_Config.backend_info.bSupportsBitfield = true;
      g_Config.backend_info.bSupportsDynamicSamplerIndexing = true;
    }
  }
  else
  {
    if (GLExtensions::Version() < 300)
    {
      PanicAlert("GPU: OGL ERROR: Need at least GLSL 1.30\n"
                 "GPU: Does your video card support OpenGL 3.0?\n"
                 "GPU: Your driver supports GLSL %s",
                 (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
      bSuccess = false;
    }
    else if (GLExtensions::Version() == 300)
    {
      g_ogl_config.eSupportedGLSLVersion = Glsl130;
      g_ogl_config.bSupportsImageLoadStore = false;  // layout keyword is only supported on glsl150+
      g_ogl_config.bSupportsConservativeDepth =
          false;  // layout keyword is only supported on glsl150+
      g_Config.backend_info.bSupportsGeometryShaders =
          false;  // geometry shaders are only supported on glsl150+
    }
    else if (GLExtensions::Version() == 310)
    {
      g_ogl_config.eSupportedGLSLVersion = Glsl140;
      g_ogl_config.bSupportsImageLoadStore = false;  // layout keyword is only supported on glsl150+
      g_ogl_config.bSupportsConservativeDepth =
          false;  // layout keyword is only supported on glsl150+
      g_Config.backend_info.bSupportsGeometryShaders =
          false;  // geometry shaders are only supported on glsl150+
    }
    else if (GLExtensions::Version() == 320)
    {
      g_ogl_config.eSupportedGLSLVersion = Glsl150;
    }
    else if (GLExtensions::Version() == 330)
    {
      g_ogl_config.eSupportedGLSLVersion = Glsl330;
    }
    else if (GLExtensions::Version() >= 430)
    {
      // TODO: We should really parse the GL_SHADING_LANGUAGE_VERSION token.
      g_ogl_config.eSupportedGLSLVersion = Glsl430;
      g_ogl_config.bSupportsTextureStorage = true;
      g_ogl_config.bSupportsImageLoadStore = true;
      g_Config.backend_info.bSupportsSSAA = true;

      // Compute shaders are core in GL4.3.
      g_Config.backend_info.bSupportsComputeShaders = true;
      if (GLExtensions::Version() >= 450)
        g_ogl_config.bSupportsTextureSubImage = true;
    }
    else
    {
      g_ogl_config.eSupportedGLSLVersion = Glsl400;
      g_Config.backend_info.bSupportsSSAA = true;

      if (GLExtensions::Version() == 420)
      {
        // Texture storage and shader image load/store are core in GL4.2.
        g_ogl_config.bSupportsTextureStorage = true;
        g_ogl_config.bSupportsImageLoadStore = true;
      }
    }

    // Desktop OpenGL can't have the Android Extension Pack
    g_ogl_config.bSupportsAEP = false;

    // Desktop GL requires GL_PROGRAM_POINT_SIZE set to use gl_PointSize in shaders.
    // It is implicitly enabled in GLES.
    glEnable(GL_PROGRAM_POINT_SIZE);
  }

  // Either method can do early-z tests. See PixelShaderGen for details.
  g_Config.backend_info.bSupportsEarlyZ =
      g_ogl_config.bSupportsImageLoadStore || g_ogl_config.bSupportsConservativeDepth;

  glGetIntegerv(GL_MAX_SAMPLES, &g_ogl_config.max_samples);
  if (g_ogl_config.max_samples < 1 || !g_ogl_config.bSupportsMSAA)
    g_ogl_config.max_samples = 1;

  g_ogl_config.bSupportsShaderThreadShuffleNV =
      GLExtensions::Supports("GL_NV_shader_thread_shuffle");

  // We require texel buffers, image load store, and compute shaders to enable GPU texture decoding.
  // If the driver doesn't expose the extensions, but supports GL4.3/GLES3.1, it will still be
  // enabled in the version check below.
  g_Config.backend_info.bSupportsGPUTextureDecoding =
      g_Config.backend_info.bSupportsPaletteConversion &&
      g_Config.backend_info.bSupportsComputeShaders && g_ogl_config.bSupportsImageLoadStore;

  // Background compiling is supported only when shared contexts aren't broken.
  g_Config.backend_info.bSupportsBackgroundCompiling =
      !DriverDetails::HasBug(DriverDetails::BUG_SHARED_CONTEXT_SHADER_COMPILATION);

  // Program binaries are supported on GL4.1+, ARB_get_program_binary, or ES3.
  if (supports_glsl_cache)
  {
    // We need to check the number of formats supported. If zero, don't bother getting the binaries.
    GLint num_formats = 0;
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &num_formats);
    supports_glsl_cache = num_formats > 0;
  }
  g_Config.backend_info.bSupportsPipelineCacheData = supports_glsl_cache;

  if (g_ogl_config.bSupportsDebug)
  {
    if (GLExtensions::Supports("GL_KHR_debug"))
    {
      glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, true);
      glDebugMessageCallback(ErrorCallback, nullptr);
    }
    else
    {
      glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, true);
      glDebugMessageCallbackARB(ErrorCallback, nullptr);
    }
    if (LogManager::GetInstance()->IsEnabled(LogTypes::HOST_GPU, LogTypes::LERROR))
    {
      glEnable(GL_DEBUG_OUTPUT);
    }
    else
    {
      glDisable(GL_DEBUG_OUTPUT);
    }
  }

  int samples;
  glGetIntegerv(GL_SAMPLES, &samples);
  if (samples > 1)
  {
    // MSAA on default framebuffer isn't working because of glBlitFramebuffer.
    // It also isn't useful as we don't render anything to the default framebuffer.
    // We also try to get a non-msaa fb, so this only happens when forced by the driver.
    PanicAlert("MSAA on default framebuffer isn't supported.\n"
               "Please avoid forcing Dolphin to use MSAA by the driver.\n"
               "%d samples on default framebuffer found.",
               samples);
    bSuccess = false;
  }

  if (!bSuccess)
  {
    // Not all needed extensions are supported, so we have to stop here.
    // Else some of the next calls might crash.
    return;
  }

  g_Config.VerifyValidity();
  UpdateActiveConfig();

  if (!g_ogl_config.bSupportsGLBufferStorage && !g_ogl_config.bSupportsGLPinnedMemory)
  {
    OSD::AddMessage(StringFromFormat("OpenGL driver does not support %s_buffer_storage.",
                                     m_main_gl_context->IsGLES() ? "EXT" : "ARB"),
                    60000);
    OSD::AddMessage("This device's performance will be terrible.", 60000);
  }

  if (g_Config.bEnableGPUTextureDecoding && !g_Config.backend_info.bSupportsGPUTextureDecoding)
  {
    OSD::AddMessage("GPU decode is not supported!", 6000);
  }

  WARN_LOG(VIDEO, "Missing OGL Extensions: %s%s%s%s%s%s%s%s%s%s%s%s%s",
           g_ActiveConfig.backend_info.bSupportsDualSourceBlend ? "" : "DualSourceBlend ",
           g_ActiveConfig.backend_info.bSupportsEarlyZ ? "" : "EarlyZ ",
           g_ogl_config.bSupportsGLPinnedMemory ? "" : "PinnedMemory ",
           supports_glsl_cache ? "" : "ShaderCache ",
           g_ogl_config.bSupportsGLBaseVertex ? "" : "BaseVertex ",
           g_ogl_config.bSupportsGLBufferStorage ? "" : "BufferStorage ",
           g_ogl_config.bSupportsGLSync ? "" : "Sync ", g_ogl_config.bSupportsMSAA ? "" : "MSAA ",
           g_ActiveConfig.backend_info.bSupportsSSAA ? "" : "SSAA ",
           g_ActiveConfig.backend_info.bSupportsGSInstancing ? "" : "GSInstancing ",
           g_ActiveConfig.backend_info.bSupportsClipControl ? "" : "ClipControl ",
           g_ogl_config.bSupportsCopySubImage ? "" : "CopyImageSubData ",
           g_ActiveConfig.backend_info.bSupportsDepthClamp ? "" : "DepthClamp ");

  // Handle VSync on/off
  if (!DriverDetails::HasBug(DriverDetails::BUG_BROKEN_VSYNC))
    m_main_gl_context->SwapInterval(g_ActiveConfig.bVSyncActive);

  if (g_ActiveConfig.backend_info.bSupportsClipControl)
    glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

  if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
  {
    glEnable(GL_CLIP_DISTANCE0);
    glEnable(GL_CLIP_DISTANCE1);
    glEnable(GL_DEPTH_CLAMP);
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment

  glGenFramebuffers(1, &m_shared_read_framebuffer);
  glGenFramebuffers(1, &m_shared_draw_framebuffer);

  UpdateActiveConfig();
}

Renderer::~Renderer() = default;

bool Renderer::IsHeadless() const
{
  return m_main_gl_context->IsHeadless();
}

void Renderer::Shutdown()
{
  ::Renderer::Shutdown();

  glDeleteFramebuffers(1, &m_shared_draw_framebuffer);
  glDeleteFramebuffers(1, &m_shared_read_framebuffer);
}

std::unique_ptr<AbstractTexture> Renderer::CreateTexture(const TextureConfig& config)
{
  return std::make_unique<OGLTexture>(config);
}

std::unique_ptr<AbstractStagingTexture> Renderer::CreateStagingTexture(StagingTextureType type,
                                                                       const TextureConfig& config)
{
  return OGLStagingTexture::Create(type, config);
}

std::unique_ptr<AbstractFramebuffer> Renderer::CreateFramebuffer(AbstractTexture* color_attachment,
                                                                 AbstractTexture* depth_attachment)
{
  return OGLFramebuffer::Create(static_cast<OGLTexture*>(color_attachment),
                                static_cast<OGLTexture*>(depth_attachment));
}

std::unique_ptr<AbstractShader> Renderer::CreateShaderFromSource(ShaderStage stage,
                                                                 std::string_view source)
{
  return OGLShader::CreateFromSource(stage, source);
}

std::unique_ptr<AbstractShader> Renderer::CreateShaderFromBinary(ShaderStage stage,
                                                                 const void* data, size_t length)
{
  return nullptr;
}

std::unique_ptr<AbstractPipeline> Renderer::CreatePipeline(const AbstractPipelineConfig& config,
                                                           const void* cache_data,
                                                           size_t cache_data_length)
{
  return OGLPipeline::Create(config, cache_data, cache_data_length);
}

void Renderer::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  glScissor(rc.left, rc.top, rc.GetWidth(), rc.GetHeight());
}

u16 Renderer::BBoxRead(int index)
{
  // swap 2 and 3 for top/bottom
  if (index >= 2)
    index ^= 1;

  int value = BoundingBox::Get(index);
  if (index >= 2)
  {
    // up/down -- we have to swap up and down
    value = EFB_HEIGHT - value;
  }

  return static_cast<u16>(value);
}

void Renderer::BBoxWrite(int index, u16 value)
{
  s32 swapped_value = value;
  if (index >= 2)
  {
    index ^= 1;  // swap 2 and 3 for top/bottom
    swapped_value = EFB_HEIGHT - swapped_value;
  }

  BoundingBox::Set(index, swapped_value);
}

void Renderer::SetViewport(float x, float y, float width, float height, float near_depth,
                           float far_depth)
{
  if (g_ogl_config.bSupportViewportFloat)
  {
    glViewportIndexedf(0, x, y, width, height);
  }
  else
  {
    auto iceilf = [](float f) { return static_cast<GLint>(std::ceil(f)); };
    glViewport(iceilf(x), iceilf(y), iceilf(width), iceilf(height));
  }

  glDepthRangef(near_depth, far_depth);
}

void Renderer::Draw(u32 base_vertex, u32 num_vertices)
{
  glDrawArrays(static_cast<const OGLPipeline*>(m_current_pipeline)->GetGLPrimitive(), base_vertex,
               num_vertices);
}

void Renderer::DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex)
{
  if (g_ogl_config.bSupportsGLBaseVertex)
  {
    glDrawElementsBaseVertex(static_cast<const OGLPipeline*>(m_current_pipeline)->GetGLPrimitive(),
                             num_indices, GL_UNSIGNED_SHORT,
                             static_cast<u16*>(nullptr) + base_index, base_vertex);
  }
  else
  {
    glDrawElements(static_cast<const OGLPipeline*>(m_current_pipeline)->GetGLPrimitive(),
                   num_indices, GL_UNSIGNED_SHORT, static_cast<u16*>(nullptr) + base_index);
  }
}

void Renderer::DispatchComputeShader(const AbstractShader* shader, u32 groups_x, u32 groups_y,
                                     u32 groups_z)
{
  glUseProgram(static_cast<const OGLShader*>(shader)->GetGLComputeProgramID());
  glDispatchCompute(groups_x, groups_y, groups_z);

  // We messed up the program binding, so restore it.
  ProgramShaderCache::InvalidateLastProgram();
  if (m_current_pipeline)
    static_cast<const OGLPipeline*>(m_current_pipeline)->GetProgram()->shader.Bind();

  // Barrier to texture can be used for reads.
  if (m_bound_image_texture)
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
}

void Renderer::ClearScreen(const MathUtil::Rectangle<int>& rc, bool colorEnable, bool alphaEnable,
                           bool zEnable, u32 color, u32 z)
{
  g_framebuffer_manager->FlushEFBPokes();
  g_framebuffer_manager->FlagPeekCacheAsOutOfDate();

  u32 clear_mask = 0;
  if (colorEnable || alphaEnable)
  {
    glColorMask(colorEnable, colorEnable, colorEnable, alphaEnable);
    glClearColor(float((color >> 16) & 0xFF) / 255.0f, float((color >> 8) & 0xFF) / 255.0f,
                 float((color >> 0) & 0xFF) / 255.0f, float((color >> 24) & 0xFF) / 255.0f);
    clear_mask = GL_COLOR_BUFFER_BIT;
  }
  if (zEnable)
  {
    glDepthMask(zEnable ? GL_TRUE : GL_FALSE);
    glClearDepthf(float(z & 0xFFFFFF) / 16777216.0f);
    clear_mask |= GL_DEPTH_BUFFER_BIT;
  }

  // Update rect for clearing the picture
  // glColorMask/glDepthMask/glScissor affect glClear (glViewport does not)
  const auto converted_target_rc =
      ConvertFramebufferRectangle(ConvertEFBRectangle(rc), m_current_framebuffer);
  SetScissorRect(converted_target_rc);

  glClear(clear_mask);

  // Restore color/depth mask.
  if (colorEnable || alphaEnable)
  {
    glColorMask(m_current_blend_state.colorupdate, m_current_blend_state.colorupdate,
                m_current_blend_state.colorupdate, m_current_blend_state.alphaupdate);
  }
  if (zEnable)
    glDepthMask(m_current_depth_state.updateenable);

  // Scissor rect must be restored.
  BPFunctions::SetScissor();
}

void Renderer::SetFramebuffer(AbstractFramebuffer* framebuffer)
{
  if (m_current_framebuffer == framebuffer)
    return;

  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<OGLFramebuffer*>(framebuffer)->GetFBO());
  m_current_framebuffer = framebuffer;
}

void Renderer::SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer)
{
  // EXT_discard_framebuffer could be used here to save bandwidth on tilers.
  SetFramebuffer(framebuffer);
}

void Renderer::SetAndClearFramebuffer(AbstractFramebuffer* framebuffer,
                                      const ClearColor& color_value, float depth_value)
{
  SetFramebuffer(framebuffer);

  glDisable(GL_SCISSOR_TEST);
  GLbitfield clear_mask = 0;
  if (framebuffer->HasColorBuffer())
  {
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(color_value[0], color_value[1], color_value[2], color_value[3]);
    clear_mask |= GL_COLOR_BUFFER_BIT;
  }
  if (framebuffer->HasDepthBuffer())
  {
    glDepthMask(GL_TRUE);
    glClearDepthf(depth_value);
    clear_mask |= GL_DEPTH_BUFFER_BIT;
  }
  glClear(clear_mask);
  glEnable(GL_SCISSOR_TEST);

  // Restore color/depth mask.
  if (framebuffer->HasColorBuffer())
  {
    glColorMask(m_current_blend_state.colorupdate, m_current_blend_state.colorupdate,
                m_current_blend_state.colorupdate, m_current_blend_state.alphaupdate);
  }
  if (framebuffer->HasDepthBuffer())
    glDepthMask(m_current_depth_state.updateenable);
}

void Renderer::BindBackbuffer(const ClearColor& clear_color)
{
  CheckForSurfaceChange();
  CheckForSurfaceResize();
  SetAndClearFramebuffer(m_system_framebuffer.get(), clear_color);
}

void Renderer::PresentBackbuffer()
{
  if (g_ogl_config.bSupportsDebug)
  {
    if (LogManager::GetInstance()->IsEnabled(LogTypes::HOST_GPU, LogTypes::LERROR))
      glEnable(GL_DEBUG_OUTPUT);
    else
      glDisable(GL_DEBUG_OUTPUT);
  }

  // Swap the back and front buffers, presenting the image.
  m_main_gl_context->Swap();
}

void Renderer::OnConfigChanged(u32 bits)
{
  if (bits & CONFIG_CHANGE_BIT_VSYNC && !DriverDetails::HasBug(DriverDetails::BUG_BROKEN_VSYNC))
    m_main_gl_context->SwapInterval(g_ActiveConfig.bVSyncActive);

  if (bits & CONFIG_CHANGE_BIT_ANISOTROPY)
    g_sampler_cache->Clear();
}

void Renderer::Flush()
{
  // ensure all commands are sent to the GPU.
  // Otherwise the driver could batch several frames together.
  glFlush();
}

void Renderer::WaitForGPUIdle()
{
  glFinish();
}

void Renderer::CheckForSurfaceChange()
{
  if (!m_surface_changed.TestAndClear())
    return;

  m_main_gl_context->UpdateSurface(m_new_surface_handle);
  m_new_surface_handle = nullptr;

  // With a surface change, the window likely has new dimensions.
  m_backbuffer_width = m_main_gl_context->GetBackBufferWidth();
  m_backbuffer_height = m_main_gl_context->GetBackBufferHeight();
  m_system_framebuffer->UpdateDimensions(m_backbuffer_width, m_backbuffer_height);
}

void Renderer::CheckForSurfaceResize()
{
  if (!m_surface_resized.TestAndClear())
    return;

  m_main_gl_context->Update();
  m_backbuffer_width = m_main_gl_context->GetBackBufferWidth();
  m_backbuffer_height = m_main_gl_context->GetBackBufferHeight();
  m_system_framebuffer->UpdateDimensions(m_backbuffer_width, m_backbuffer_height);
}

void Renderer::BeginUtilityDrawing()
{
  ::Renderer::BeginUtilityDrawing();
  if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
  {
    glDisable(GL_CLIP_DISTANCE0);
    glDisable(GL_CLIP_DISTANCE1);
  }
}

void Renderer::EndUtilityDrawing()
{
  ::Renderer::EndUtilityDrawing();
  if (g_ActiveConfig.backend_info.bSupportsDepthClamp)
  {
    glEnable(GL_CLIP_DISTANCE0);
    glEnable(GL_CLIP_DISTANCE1);
  }
}

void Renderer::ApplyRasterizationState(const RasterizationState state)
{
  if (m_current_rasterization_state == state)
    return;

  // none, ccw, cw, ccw
  if (state.cullmode != GenMode::CULL_NONE)
  {
    // TODO: GX_CULL_ALL not supported, yet!
    glEnable(GL_CULL_FACE);
    glFrontFace(state.cullmode == GenMode::CULL_FRONT ? GL_CCW : GL_CW);
  }
  else
  {
    glDisable(GL_CULL_FACE);
  }

  m_current_rasterization_state = state;
}

void Renderer::ApplyDepthState(const DepthState state)
{
  if (m_current_depth_state == state)
    return;

  if (state.testenable)
  {
    const GLenum glCmpFuncs[8] = {GL_NEVER,   GL_LESS,     GL_EQUAL,  GL_LEQUAL,
                                  GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS};

    glEnable(GL_DEPTH_TEST);
    glDepthMask(state.updateenable ? GL_TRUE : GL_FALSE);
    glDepthFunc(glCmpFuncs[state.func]);
  }
  else
  {
    // if the test is disabled write is disabled too
    // TODO: When PE performance metrics are being emulated via occlusion queries, we should
    // (probably?) enable depth test with depth function ALWAYS here
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
  }

  m_current_depth_state = state;
}

void Renderer::ApplyBlendingState(const BlendingState state)
{
  if (m_current_blend_state == state)
    return;

  bool useDualSource =
      g_ActiveConfig.backend_info.bSupportsDualSourceBlend && state.IsDualSourceBlend();

  const GLenum src_factors[8] = {GL_ZERO,
                                 GL_ONE,
                                 GL_DST_COLOR,
                                 GL_ONE_MINUS_DST_COLOR,
                                 useDualSource ? GL_SRC1_ALPHA : (GLenum)GL_SRC_ALPHA,
                                 useDualSource ? GL_ONE_MINUS_SRC1_ALPHA :
                                                 (GLenum)GL_ONE_MINUS_SRC_ALPHA,
                                 GL_DST_ALPHA,
                                 GL_ONE_MINUS_DST_ALPHA};
  const GLenum dst_factors[8] = {GL_ZERO,
                                 GL_ONE,
                                 GL_SRC_COLOR,
                                 GL_ONE_MINUS_SRC_COLOR,
                                 useDualSource ? GL_SRC1_ALPHA : (GLenum)GL_SRC_ALPHA,
                                 useDualSource ? GL_ONE_MINUS_SRC1_ALPHA :
                                                 (GLenum)GL_ONE_MINUS_SRC_ALPHA,
                                 GL_DST_ALPHA,
                                 GL_ONE_MINUS_DST_ALPHA};

  if (state.blendenable)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);

  // Always call glBlendEquationSeparate and glBlendFuncSeparate, even when
  // GL_BLEND is disabled, as a workaround for some bugs (possibly graphics
  // driver issues?). See https://bugs.dolphin-emu.org/issues/10120 : "Sonic
  // Adventure 2 Battle: graphics crash when loading first Dark level"
  GLenum equation = state.subtract ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD;
  GLenum equationAlpha = state.subtractAlpha ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD;
  glBlendEquationSeparate(equation, equationAlpha);
  glBlendFuncSeparate(src_factors[state.srcfactor], dst_factors[state.dstfactor],
                      src_factors[state.srcfactoralpha], dst_factors[state.dstfactoralpha]);

  // Logic ops aren't available in GLES3
  if (g_Config.backend_info.bSupportsLogicOp)
  {
    if (state.logicopenable)
    {
      const GLenum logic_op_codes[16] = {
          GL_CLEAR,         GL_AND,         GL_AND_REVERSE, GL_COPY,
          GL_AND_INVERTED,  GL_NOOP,        GL_XOR,         GL_OR,
          GL_NOR,           GL_EQUIV,       GL_INVERT,      GL_OR_REVERSE,
          GL_COPY_INVERTED, GL_OR_INVERTED, GL_NAND,        GL_SET};

      glEnable(GL_COLOR_LOGIC_OP);
      glLogicOp(logic_op_codes[state.logicmode]);
    }
    else
    {
      glDisable(GL_COLOR_LOGIC_OP);
    }
  }
  else if (state.logicopenable && !g_Config.backend_info.bSupportsFramebufferFetch)
  {
    OSD::AddTypedMessage(OSD::MessageType::LogicOpsNotice, "Logic ops aren't available!");
  }

  glColorMask(state.colorupdate, state.colorupdate, state.colorupdate, state.alphaupdate);
  m_current_blend_state = state;
}

void Renderer::SetPipeline(const AbstractPipeline* pipeline)
{
  if (m_current_pipeline == pipeline)
    return;

  if (pipeline)
  {
    ApplyRasterizationState(static_cast<const OGLPipeline*>(pipeline)->GetRasterizationState());
    ApplyDepthState(static_cast<const OGLPipeline*>(pipeline)->GetDepthState());
    ApplyBlendingState(static_cast<const OGLPipeline*>(pipeline)->GetBlendingState());
    ProgramShaderCache::BindVertexFormat(
        static_cast<const OGLPipeline*>(pipeline)->GetVertexFormat());
    static_cast<const OGLPipeline*>(pipeline)->GetProgram()->shader.Bind();
  }
  else
  {
    ProgramShaderCache::InvalidateLastProgram();
    glUseProgram(0);
  }
  m_current_pipeline = pipeline;
}

void Renderer::SetTexture(u32 index, const AbstractTexture* texture)
{
  const OGLTexture* gl_texture = static_cast<const OGLTexture*>(texture);
  if (m_bound_textures[index] == gl_texture)
    return;

  glActiveTexture(GL_TEXTURE0 + index);
  if (gl_texture)
    glBindTexture(gl_texture->GetGLTarget(), gl_texture->GetGLTextureId());
  else
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  m_bound_textures[index] = gl_texture;
}

void Renderer::SetSamplerState(u32 index, const SamplerState& state)
{
  g_sampler_cache->SetSamplerState(index, state);
}

void Renderer::SetComputeImageTexture(AbstractTexture* texture, bool read, bool write)
{
  if (m_bound_image_texture == texture)
    return;

  if (texture)
  {
    const GLenum access = read ? (write ? GL_READ_WRITE : GL_READ_ONLY) : GL_WRITE_ONLY;
    glBindImageTexture(0, static_cast<OGLTexture*>(texture)->GetGLTextureId(), 0, GL_TRUE, 0,
                       access, static_cast<OGLTexture*>(texture)->GetGLFormatForImageTexture());
  }
  else
  {
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
  }

  m_bound_image_texture = texture;
}

void Renderer::UnbindTexture(const AbstractTexture* texture)
{
  for (size_t i = 0; i < m_bound_textures.size(); i++)
  {
    if (m_bound_textures[i] != texture)
      continue;

    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + i));
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    m_bound_textures[i] = nullptr;
  }

  if (m_bound_image_texture == texture)
  {
    glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA8);
    m_bound_image_texture = nullptr;
  }
}

std::unique_ptr<VideoCommon::AsyncShaderCompiler> Renderer::CreateAsyncShaderCompiler()
{
  return std::make_unique<SharedContextAsyncShaderCompiler>();
}

void Renderer::BindSharedReadFramebuffer()
{
  glBindFramebuffer(GL_READ_FRAMEBUFFER, m_shared_read_framebuffer);
}

void Renderer::BindSharedDrawFramebuffer()
{
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_shared_draw_framebuffer);
}

void Renderer::RestoreFramebufferBinding()
{
  glBindFramebuffer(
      GL_FRAMEBUFFER,
      m_current_framebuffer ? static_cast<OGLFramebuffer*>(m_current_framebuffer)->GetFBO() : 0);
}

}  // namespace OGL
