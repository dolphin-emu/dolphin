// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/OGL/OGLConfig.h"

#include <cstdio>
#include <string>
#include <string_view>

#include "Common/Assert.h"
#include "Common/GL/GLContext.h"
#include "Common/GL/GLExtensions/GLExtensions.h"
#include "Common/Logging/LogManager.h"
#include "Common/MsgHandler.h"

#include "Core/Config/GraphicsSettings.h"

#include "VideoBackends/OGL/OGLTexture.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
void InitDriverInfo()
{
  const std::string_view svendor(g_ogl_config.gl_vendor);
  const std::string_view srenderer(g_ogl_config.gl_renderer);
  const std::string_view sversion(g_ogl_config.gl_version);
  DriverDetails::Vendor vendor = DriverDetails::VENDOR_UNKNOWN;
  DriverDetails::Driver driver = DriverDetails::DRIVER_UNKNOWN;
  DriverDetails::Family family = DriverDetails::Family::UNKNOWN;
  double version = 0.0;

  // Get the vendor first
  if (svendor == "NVIDIA Corporation")
  {
    if (srenderer != "NVIDIA Tegra")
    {
      vendor = DriverDetails::VENDOR_NVIDIA;
    }
    else
    {
      vendor = DriverDetails::VENDOR_TEGRA;
    }
  }
  else if (svendor == "ATI Technologies Inc." || svendor == "Advanced Micro Devices, Inc.")
  {
    vendor = DriverDetails::VENDOR_ATI;
  }
  else if (sversion.find("Mesa") != std::string::npos)
  {
    vendor = DriverDetails::VENDOR_MESA;
  }
  else if (svendor.find("Intel") != std::string::npos)
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
    else if (srenderer.find("AMD") != std::string::npos ||
             srenderer.find("ATI") != std::string::npos)
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
      ERROR_LOG_FMT(VIDEO, "Version changeID overflow - change:{} scale:{}", change, change_scale);
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

bool PopulateConfig(GLContext* m_main_gl_context)
{
  bool bSuccess = true;
  bool supports_glsl_cache = false;

  g_ogl_config.gl_vendor = (const char*)glGetString(GL_VENDOR);
  g_ogl_config.gl_renderer = (const char*)glGetString(GL_RENDERER);
  g_ogl_config.gl_version = (const char*)glGetString(GL_VERSION);

  if (!m_main_gl_context->IsGLES())
  {
    if (!GLExtensions::Supports("GL_ARB_framebuffer_object"))
    {
      // We want the ogl3 framebuffer instead of the ogl2 one for better blitting support.
      // It's also compatible with the gles3 one.
      PanicAlertFmtT("GPU: ERROR: Need GL_ARB_framebuffer_object for multiple render targets.\n"
                     "GPU: Does your video card support OpenGL 3.0?");
      bSuccess = false;
    }

    if (!GLExtensions::Supports("GL_ARB_vertex_array_object"))
    {
      // This extension is used to replace lots of pointer setting function.
      // Also gles3 requires to use it.
      PanicAlertFmtT("GPU: OGL ERROR: Need GL_ARB_vertex_array_object.\n"
                     "GPU: Does your video card support OpenGL 3.0?");
      bSuccess = false;
    }

    if (!GLExtensions::Supports("GL_ARB_map_buffer_range"))
    {
      // ogl3 buffer mapping for better streaming support.
      // The ogl2 one also isn't in gles3.
      PanicAlertFmtT("GPU: OGL ERROR: Need GL_ARB_map_buffer_range.\n"
                     "GPU: Does your video card support OpenGL 3.0?");
      bSuccess = false;
    }

    if (!GLExtensions::Supports("GL_ARB_uniform_buffer_object"))
    {
      // ubo allow us to keep the current constants on shader switches
      // we also can stream them much nicer and pack into it whatever we want to
      PanicAlertFmtT("GPU: OGL ERROR: Need GL_ARB_uniform_buffer_object.\n"
                     "GPU: Does your video card support OpenGL 3.1?");
      bSuccess = false;
    }
    else if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_UBO))
    {
      PanicAlertFmtT(
          "Buggy GPU driver detected.\n"
          "Please either install the closed-source GPU driver or update your Mesa 3D version.");
      bSuccess = false;
    }

    if (!GLExtensions::Supports("GL_ARB_sampler_objects"))
    {
      // Our sampler cache uses this extension. It could easyly be workaround and it's by far the
      // highest requirement, but it seems that no driver lacks support for it.
      PanicAlertFmtT("GPU: OGL ERROR: Need GL_ARB_sampler_objects.\n"
                     "GPU: Does your video card support OpenGL 3.3?");
      bSuccess = false;
    }
  }

  // Copy the GPU name to g_Config, so Analytics can see it.
  g_Config.backend_info.AdapterName = g_ogl_config.gl_renderer;

  g_Config.backend_info.bSupportsDualSourceBlend =
      (GLExtensions::Supports("GL_ARB_blend_func_extended") ||
       GLExtensions::Supports("GL_EXT_blend_func_extended"));
  g_Config.backend_info.bSupportsPrimitiveRestart =
      !DriverDetails::HasBug(DriverDetails::BUG_PRIMITIVE_RESTART) &&
      ((GLExtensions::Version() >= 310) || GLExtensions::Supports("GL_NV_primitive_restart"));
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
  g_Config.backend_info.bSupportsClipControl = GLExtensions::Supports("GL_ARB_clip_control");
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
  g_ogl_config.SupportedMultisampleTexStorage = MultisampleTexStorageType::TexStorageNone;
  g_ogl_config.bSupportsImageLoadStore = GLExtensions::Supports("GL_ARB_shader_image_load_store");
  g_ogl_config.bSupportsConservativeDepth = GLExtensions::Supports("GL_ARB_conservative_depth");
  g_ogl_config.bSupportsAniso = GLExtensions::Supports("GL_EXT_texture_filter_anisotropic");
  g_Config.backend_info.bSupportsComputeShaders = GLExtensions::Supports("GL_ARB_compute_shader");
  g_Config.backend_info.bSupportsST3CTextures =
      GLExtensions::Supports("GL_EXT_texture_compression_s3tc");
  g_Config.backend_info.bSupportsBPTCTextures =
      GLExtensions::Supports("GL_ARB_texture_compression_bptc");
  g_Config.backend_info.bSupportsCoarseDerivatives =
      GLExtensions::Supports("GL_ARB_derivative_control") || GLExtensions::Version() >= 450;
  g_Config.backend_info.bSupportsTextureQueryLevels =
      GLExtensions::Supports("GL_ARB_texture_query_levels") || GLExtensions::Version() >= 430;

  if (GLExtensions::Supports("GL_ARB_shader_storage_buffer_object"))
  {
    GLint fs = 0;
    GLint vs = 0;
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &fs);
    glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &vs);
    g_Config.backend_info.bSupportsFragmentStoresAndAtomics = fs >= 1;
    g_Config.backend_info.bSupportsVSLinePointExpand = vs >= 1;
  }
  else
  {
    g_Config.backend_info.bSupportsFragmentStoresAndAtomics = false;
    g_Config.backend_info.bSupportsVSLinePointExpand = false;
  }

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

  if (m_main_gl_context->IsGLES())
  {
    g_ogl_config.SupportedESPointSize =
        GLExtensions::Supports("GL_OES_geometry_point_size") ? EsPointSizeType::PointSizeOes :
        GLExtensions::Supports("GL_EXT_geometry_point_size") ? EsPointSizeType::PointSizeExt :
                                                               EsPointSizeType::PointSizeNone;
    g_ogl_config.SupportedESTextureBuffer =
        GLExtensions::Supports("VERSION_GLES_3_2")      ? EsTexbufType::TexbufCore :
        GLExtensions::Supports("GL_OES_texture_buffer") ? EsTexbufType::TexbufOes :
        GLExtensions::Supports("GL_EXT_texture_buffer") ? EsTexbufType::TexbufExt :
                                                          EsTexbufType::TexbufNone;

    supports_glsl_cache = true;
    g_ogl_config.bSupportsGLSync = true;

    // TODO: Implement support for GL_EXT_clip_cull_distance when there is an extension for
    // depth clamping.
    g_Config.backend_info.bSupportsDepthClamp = false;

    // GLES does not support logic op.
    g_Config.backend_info.bSupportsLogicOp = false;

    // glReadPixels() can't be used with non-color formats. But, if we support
    // ARB_get_texture_sub_image (unlikely, except maybe on NVIDIA), we can use that instead.
    g_Config.backend_info.bSupportsDepthReadback = g_ogl_config.bSupportsTextureSubImage;

    // GL_TEXTURE_LOD_BIAS is not supported on GLES.
    g_Config.backend_info.bSupportsLodBiasInSampler = false;

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
          g_Config.backend_info.bSupportsGeometryShaders &&
          g_ogl_config.SupportedESPointSize != EsPointSizeType::PointSizeNone;
      g_Config.backend_info.bSupportsSSAA = g_ogl_config.bSupportsAEP;
      g_Config.backend_info.bSupportsFragmentStoresAndAtomics = true;
      g_ogl_config.bSupportsMSAA = true;
      g_ogl_config.bSupportsTextureStorage = true;
      if (GLExtensions::Supports("GL_OES_texture_storage_multisample_2d_array"))
        g_ogl_config.SupportedMultisampleTexStorage = MultisampleTexStorageType::TexStorageOes;
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
      g_Config.backend_info.bSupportsGSInstancing =
          g_ogl_config.SupportedESPointSize != EsPointSizeType::PointSizeNone;
      g_Config.backend_info.bSupportsPaletteConversion = true;
      g_Config.backend_info.bSupportsSSAA = true;
      g_Config.backend_info.bSupportsFragmentStoresAndAtomics = true;
      g_ogl_config.bSupportsCopySubImage = true;
      g_ogl_config.bSupportsGLBaseVertex = true;
      g_ogl_config.bSupportsDebug = true;
      g_ogl_config.bSupportsMSAA = true;
      g_ogl_config.bSupportsTextureStorage = true;
      g_ogl_config.SupportedMultisampleTexStorage = MultisampleTexStorageType::TexStorageCore;
      g_Config.backend_info.bSupportsBitfield = true;
      g_Config.backend_info.bSupportsDynamicSamplerIndexing = true;
      g_Config.backend_info.bSupportsSettingObjectNames = true;
    }
  }
  else
  {
    if (GLExtensions::Supports("GL_ARB_texture_storage_multisample"))
      g_ogl_config.SupportedMultisampleTexStorage = MultisampleTexStorageType::TexStorageCore;

    if (GLExtensions::Version() < 300)
    {
      PanicAlertFmtT("GPU: OGL ERROR: Need at least GLSL 1.30\n"
                     "GPU: Does your video card support OpenGL 3.0?\n"
                     "GPU: Your driver supports GLSL {0}",
                     reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
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
      g_ogl_config.bSupportsExplicitLayoutInShader =
          GLExtensions::Supports("GL_ARB_explicit_attrib_location");
    }
    else if (GLExtensions::Version() >= 430)
    {
      // TODO: We should really parse the GL_SHADING_LANGUAGE_VERSION token.
      if (GLExtensions::Version() >= 450)
      {
        g_ogl_config.eSupportedGLSLVersion = Glsl450;
      }
      else
      {
        g_ogl_config.eSupportedGLSLVersion = Glsl430;
      }
      g_ogl_config.bSupportsTextureStorage = true;
      g_ogl_config.SupportedMultisampleTexStorage = MultisampleTexStorageType::TexStorageCore;
      g_ogl_config.bSupportsImageLoadStore = true;
      g_ogl_config.bSupportsExplicitLayoutInShader = true;
      g_Config.backend_info.bSupportsSSAA = true;
      g_Config.backend_info.bSupportsSettingObjectNames = true;

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

  // Supported by all GS-supporting ES and 4.3+
  g_Config.backend_info.bSupportsGLLayerInFS = g_Config.backend_info.bSupportsGeometryShaders &&
                                               g_ogl_config.eSupportedGLSLVersion >= Glsl430;

  g_Config.backend_info.bSupportsBBox = g_Config.backend_info.bSupportsFragmentStoresAndAtomics;

  // Either method can do early-z tests. See PixelShaderGen for details.
  g_Config.backend_info.bSupportsEarlyZ =
      g_ogl_config.bSupportsImageLoadStore || g_ogl_config.bSupportsConservativeDepth;

  g_Config.backend_info.AAModes.clear();
  if (g_ogl_config.bSupportsMSAA)
  {
    bool supportsGetInternalFormat =
        GLExtensions::Supports("VERSION_4_2") || GLExtensions::Supports("VERSION_GLES_3");
    if (supportsGetInternalFormat)
    {
      // Note: GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES should technically be used for
      // GL_OES_texture_storage_multisample_2d_array, but both are 0x9102 so it does not matter.

      std::vector<int> color_aa_modes;
      {
        GLenum colorInternalFormat = OGLTexture::GetGLInternalFormatForTextureFormat(
            FramebufferManager::GetEFBColorFormat(), true);
        GLint num_color_sample_counts = 0;
        glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, colorInternalFormat,
                              GL_NUM_SAMPLE_COUNTS, 1, &num_color_sample_counts);

        ASSERT_MSG(VIDEO, num_color_sample_counts >= 0,
                   "negative GL_NUM_SAMPLE_COUNTS for colors does not make sense: {}",
                   num_color_sample_counts);
        color_aa_modes.reserve(num_color_sample_counts + 1);
        if (num_color_sample_counts > 0)
        {
          color_aa_modes.resize(num_color_sample_counts);

          static_assert(sizeof(GLint) == sizeof(u32));
          glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, colorInternalFormat, GL_SAMPLES,
                                num_color_sample_counts,
                                reinterpret_cast<GLint*>(color_aa_modes.data()));
          ASSERT_MSG(VIDEO, std::is_sorted(color_aa_modes.rbegin(), color_aa_modes.rend()),
                     "GPU driver didn't return sorted color AA modes: [{}]",
                     fmt::join(color_aa_modes, ", "));
        }

        if (color_aa_modes.empty() || color_aa_modes.back() != 1)
          color_aa_modes.push_back(1);
      }

      std::vector<int> depth_aa_modes;
      {
        GLenum depthInternalFormat = OGLTexture::GetGLInternalFormatForTextureFormat(
            FramebufferManager::GetEFBColorFormat(), true);
        GLint num_depth_sample_counts = 0;
        glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, depthInternalFormat,
                              GL_NUM_SAMPLE_COUNTS, 1, &num_depth_sample_counts);

        ASSERT_MSG(VIDEO, num_depth_sample_counts >= 0,
                   "negative GL_NUM_SAMPLE_COUNTS for depth does not make sense: {}",
                   num_depth_sample_counts);
        depth_aa_modes.reserve(num_depth_sample_counts + 1);
        if (num_depth_sample_counts > 0)
        {
          depth_aa_modes.resize(num_depth_sample_counts);

          static_assert(sizeof(GLint) == sizeof(u32));
          glGetInternalformativ(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, depthInternalFormat, GL_SAMPLES,
                                num_depth_sample_counts,
                                reinterpret_cast<GLint*>(depth_aa_modes.data()));
          ASSERT_MSG(VIDEO, std::is_sorted(depth_aa_modes.rbegin(), depth_aa_modes.rend()),
                     "GPU driver didn't return sorted depth AA modes: [{}]",
                     fmt::join(depth_aa_modes, ", "));
        }

        if (depth_aa_modes.empty() || depth_aa_modes.back() != 1)
          depth_aa_modes.push_back(1);
      }

      // The spec says supported sample formats are returned in descending numeric order.
      // It also says "Only positive values are returned", but does not specify whether 1 is
      // included or not; it seems like NVIDIA and Intel GPUs do not include it.
      // We've inserted 1 at the back of both if not present to handle this.
      g_Config.backend_info.AAModes.clear();
      g_Config.backend_info.AAModes.reserve(std::min(color_aa_modes.size(), depth_aa_modes.size()));
      // We only want AA modes that are supported for both the color and depth textures. Probably
      // the support is the same, though. rbegin/rend are used to swap the order ahead of time.
      std::set_intersection(color_aa_modes.rbegin(), color_aa_modes.rend(), depth_aa_modes.rbegin(),
                            depth_aa_modes.rend(),
                            std::back_inserter(g_Config.backend_info.AAModes));
    }
    else
    {
      // The documentation for glGetInternalformativ says its value is at least the minimum of
      // GL_MAX_SAMPLES, GL_MAX_COLOR_TEXTURE_SAMPLES, and GL_MAX_DEPTH_TEXTURE_SAMPLES (and
      // GL_MAX_INTEGER_SAMPLES for integer textures, assumed not applicable here).
      GLint max_color_samples = 0;
      glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &max_color_samples);
      GLint max_depth_samples = 0;
      glGetIntegerv(GL_MAX_COLOR_TEXTURE_SAMPLES, &max_depth_samples);
      // Note: The desktop OpenGL ref pages don't actually say that GL_MAX_SAMPLES is a valid
      // parameter for glGetIntegerv (though the ES ones do). However, MAX_SAMPLES is
      // referenced in the GL 3.1 spec and by GL_ARB_texture_multisample (which is written against
      // the OpenGL 3.1 spec), so presumably it is valid.
      GLint max_samples = 0;
      glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
      u32 supported_max_samples =
          static_cast<u32>(std::min({max_samples, max_color_samples, max_depth_samples}));

      while (supported_max_samples > 1)
      {
        g_Config.backend_info.AAModes.push_back(supported_max_samples);
        supported_max_samples /= 2;
      }
      g_Config.backend_info.AAModes.push_back(1);
      // The UI wants ascending order
      std::reverse(g_Config.backend_info.AAModes.begin(), g_Config.backend_info.AAModes.end());
    }
  }
  else
  {
    g_Config.backend_info.AAModes = {1};
  }

  const bool bSupportsIsHelperInvocation = g_ogl_config.bIsES ?
                                               g_ogl_config.eSupportedGLSLVersion >= GlslEs320 :
                                               g_ogl_config.eSupportedGLSLVersion >= Glsl450;
  g_ogl_config.bSupportsKHRShaderSubgroup =
      GLExtensions::Supports("GL_KHR_shader_subgroup") && bSupportsIsHelperInvocation;
  if (g_ogl_config.bSupportsKHRShaderSubgroup)
  {
    // Check for the features: basic + arithmetic + ballot
    GLint supported_features = 0;
    glGetIntegerv(GL_SUBGROUP_SUPPORTED_FEATURES_KHR, &supported_features);
    if (~supported_features &
        (GL_SUBGROUP_FEATURE_BASIC_BIT_KHR | GL_SUBGROUP_FEATURE_ARITHMETIC_BIT_KHR |
         GL_SUBGROUP_FEATURE_BALLOT_BIT_KHR))
    {
      g_ogl_config.bSupportsKHRShaderSubgroup = false;
    }
  }

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

  int samples;
  glGetIntegerv(GL_SAMPLES, &samples);
  if (samples > 1)
  {
    // MSAA on default framebuffer isn't working because of glBlitFramebuffer.
    // It also isn't useful as we don't render anything to the default framebuffer.
    // We also try to get a non-msaa fb, so this only happens when forced by the driver.
    PanicAlertFmtT(
        "The graphics driver is forcibly enabling anti-aliasing for Dolphin. You need to "
        "turn this off in the graphics driver's settings in order for Dolphin to work.\n\n"
        "(MSAA with {0} samples found on default framebuffer)",
        samples);
    bSuccess = false;
  }

  if (!bSuccess)
    return false;

  g_Config.VerifyValidity();
  UpdateActiveConfig();

  OSD::AddMessage(fmt::format("Video Info: {}, {}, {}", g_ogl_config.gl_vendor,
                              g_ogl_config.gl_renderer, g_ogl_config.gl_version),
                  5000);

  if (!g_ogl_config.bSupportsGLBufferStorage && !g_ogl_config.bSupportsGLPinnedMemory)
  {
    OSD::AddMessage(fmt::format("Your OpenGL driver does not support {}_buffer_storage.",
                                m_main_gl_context->IsGLES() ? "EXT" : "ARB"),
                    60000);
    OSD::AddMessage("This device's performance may be poor.", 60000);
  }

  INFO_LOG_FMT(VIDEO, "Video Info: {}, {}, {}", g_ogl_config.gl_vendor, g_ogl_config.gl_renderer,
               g_ogl_config.gl_version);

  const std::string missing_extensions = fmt::format(
      "{}{}{}{}{}{}{}{}{}{}{}{}{}{}",
      g_ActiveConfig.backend_info.bSupportsDualSourceBlend ? "" : "DualSourceBlend ",
      g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ? "" : "PrimitiveRestart ",
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

  if (missing_extensions.empty())
    INFO_LOG_FMT(VIDEO, "All used OGL Extensions are available.");
  else
    WARN_LOG_FMT(VIDEO, "Missing OGL Extensions: {}", missing_extensions);

  return true;
}

}  // namespace OGL
