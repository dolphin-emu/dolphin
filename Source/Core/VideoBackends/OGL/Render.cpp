// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Common/Atomic.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "Common/GL/GLInterfaceBase.h"
#include "Common/GL/GLUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"

#include "VideoBackends/OGL/BoundingBox.h"
#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/PostProcessing.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/RasterFont.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/TextureCache.h"
#include "VideoBackends/OGL/VertexManager.h"

#if defined(HAVE_LIBAV) || defined (_WIN32)
#include "VideoCommon/AVIDump.h"
#endif
#include "VideoCommon/BPFunctions.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"


void VideoConfig::UpdateProjectionHack()
{
	::UpdateProjectionHack(g_Config.iPhackvalue, g_Config.sPhackvalue);
}

static int OSDInternalW, OSDInternalH;

namespace OGL
{

VideoConfig g_ogl_config;

// Declarations and definitions
// ----------------------------
static std::unique_ptr<RasterFont> s_raster_font;

// 1 for no MSAA. Use s_MSAASamples > 1 to check for MSAA.
static int s_MSAASamples = 1;
static int s_last_multisamples = 1;
static bool s_last_stereo_mode = false;
static bool s_last_xfb_mode = false;

static u32 s_blendMode;

static bool s_vsync;

// EFB cache related
static const u32 EFB_CACHE_RECT_SIZE = 64; // Cache 64x64 blocks.
static const u32 EFB_CACHE_WIDTH = (EFB_WIDTH + EFB_CACHE_RECT_SIZE - 1) / EFB_CACHE_RECT_SIZE; // round up
static const u32 EFB_CACHE_HEIGHT = (EFB_HEIGHT + EFB_CACHE_RECT_SIZE - 1) / EFB_CACHE_RECT_SIZE;
static bool s_efbCacheValid[2][EFB_CACHE_WIDTH * EFB_CACHE_HEIGHT];
static bool s_efbCacheIsCleared = false;
static std::vector<u32> s_efbCache[2][EFB_CACHE_WIDTH * EFB_CACHE_HEIGHT]; // 2 for PEEK_Z and PEEK_COLOR

static void APIENTRY ErrorCallback( GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, const void* userParam)
{
	const char *s_source;
	const char *s_type;

	switch (source)
	{
		case GL_DEBUG_SOURCE_API_ARB:             s_source = "API"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:   s_source = "Window System"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: s_source = "Shader Compiler"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:     s_source = "Third Party"; break;
		case GL_DEBUG_SOURCE_APPLICATION_ARB:     s_source = "Application"; break;
		case GL_DEBUG_SOURCE_OTHER_ARB:           s_source = "Other"; break;
		default:                                  s_source = "Unknown"; break;
	}
	switch (type)
	{
		case GL_DEBUG_TYPE_ERROR_ARB:               s_type = "Error"; break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: s_type = "Deprecated"; break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:  s_type = "Undefined"; break;
		case GL_DEBUG_TYPE_PORTABILITY_ARB:         s_type = "Portability"; break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB:         s_type = "Performance"; break;
		case GL_DEBUG_TYPE_OTHER_ARB:               s_type = "Other"; break;
		default:                                    s_type = "Unknown"; break;
	}
	switch (severity)
	{
		case GL_DEBUG_SEVERITY_HIGH_ARB:     ERROR_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message); break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB:   WARN_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message); break;
		case GL_DEBUG_SEVERITY_LOW_ARB:      WARN_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message); break;
		case GL_DEBUG_SEVERITY_NOTIFICATION: DEBUG_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message); break;
		default:                             ERROR_LOG(HOST_GPU, "id: %x, source: %s, type: %s - %s", id, s_source, s_type, message); break;
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
	double version = 0.0;
	u32 family = 0;

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
				driver = DriverDetails::DRIVER_NOUVEAU;
			else if (svendor == "Intel Open Source Technology Center")
				driver = DriverDetails::DRIVER_I965;
			else if (std::string::npos != srenderer.find("AMD") || std::string::npos != srenderer.find("ATI"))
				driver = DriverDetails::DRIVER_R600;

			int major = 0;
			int minor = 0;
			int release = 0;
			sscanf(g_ogl_config.gl_version, "%*s (Core Profile) Mesa %d.%d.%d", &major, &minor, &release);
			version = 100*major + 10*minor + release;
		}
		break;
		case DriverDetails::VENDOR_INTEL: // Happens in OS X/Windows
		{
			sscanf(g_ogl_config.gl_renderer, "Intel HD Graphics %d", &family);
#ifdef _WIN32
			int glmajor = 0;
			int glminor = 0;
			int major = 0;
			int minor = 0;
			int release = 0;
			int revision = 0;
			// Example version string: '4.3.0 - Build 10.18.10.3907'
			sscanf(g_ogl_config.gl_version, "%d.%d.0 - Build %d.%d.%d.%d", &glmajor, &glminor, &major, &minor, &release, &revision);
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
			sscanf(g_ogl_config.gl_version, "%d.%d.%d NVIDIA %d.%d", &glmajor, &glminor, &glrelease, &major, &minor);
			version = 100*major + minor;
		}
		break;
		// We don't care about these
		default:
		break;
	}
	DriverDetails::Init(vendor, driver, version, family);
}

// Init functions
Renderer::Renderer()
{
	OSDInternalW = 0;
	OSDInternalH = 0;

	s_blendMode = 0;

	bool bSuccess = true;

	// Init extension support.
	if (!GLExtensions::Init())
	{
		// OpenGL 2.0 is required for all shader based drawings. There is no way to get this by extensions
		PanicAlert("GPU: OGL ERROR: Does your video card support OpenGL 2.0?");
		bSuccess = false;
	}

	g_ogl_config.gl_vendor = (const char*)glGetString(GL_VENDOR);
	g_ogl_config.gl_renderer = (const char*)glGetString(GL_RENDERER);
	g_ogl_config.gl_version = (const char*)glGetString(GL_VERSION);
	g_ogl_config.glsl_version = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);

	InitDriverInfo();

	if (GLExtensions::Version() < 300)
	{
		// integer vertex attributes require a gl3 only function
		PanicAlert("GPU: OGL ERROR: Need OpenGL version 3.\n"
				"GPU: Does your video card support OpenGL 3?");
		bSuccess = false;
	}

	// check for the max vertex attributes
	GLint numvertexattribs = 0;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numvertexattribs);
	if (numvertexattribs < 16)
	{
		PanicAlert("GPU: OGL ERROR: Number of attributes %d not enough.\n"
				"GPU: Does your video card support OpenGL 2.x?",
				numvertexattribs);
		bSuccess = false;
	}

	// check the max texture width and height
	GLint max_texture_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&max_texture_size);
	if (max_texture_size < 1024)
	{
		PanicAlert("GL_MAX_TEXTURE_SIZE too small at %i - must be at least 1024.",
				max_texture_size);
		bSuccess = false;
	}

	if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGL)
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
		else if (DriverDetails::HasBug(DriverDetails::BUG_BROKENUBO))
		{
			PanicAlert("Buggy GPU driver detected.\n"
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

	g_Config.backend_info.bSupportsDualSourceBlend = GLExtensions::Supports("GL_ARB_blend_func_extended") ||
	                                                 GLExtensions::Supports("GL_EXT_blend_func_extended");
	g_Config.backend_info.bSupportsPrimitiveRestart = !DriverDetails::HasBug(DriverDetails::BUG_PRIMITIVERESTART) &&
				((GLExtensions::Version() >= 310) || GLExtensions::Supports("GL_NV_primitive_restart"));
	g_Config.backend_info.bSupportsBBox = GLExtensions::Supports("GL_ARB_shader_storage_buffer_object");
	g_Config.backend_info.bSupportsGSInstancing = GLExtensions::Supports("GL_ARB_gpu_shader5");
	g_Config.backend_info.bSupportsSSAA = GLExtensions::Supports("GL_ARB_gpu_shader5") && GLExtensions::Supports("GL_ARB_sample_shading");
	g_Config.backend_info.bSupportsGeometryShaders = GLExtensions::Version() >= 320 && !DriverDetails::HasBug(DriverDetails::BUG_BROKENGEOMETRYSHADERS);
	g_Config.backend_info.bSupportsPaletteConversion = GLExtensions::Supports("GL_ARB_texture_buffer_object") ||
	                                                   GLExtensions::Supports("GL_OES_texture_buffer") ||
	                                                   GLExtensions::Supports("GL_EXT_texture_buffer");
	g_Config.backend_info.bSupportsClipControl = GLExtensions::Supports("GL_ARB_clip_control");
	g_ogl_config.bSupportsCopySubImage = (GLExtensions::Supports("GL_ARB_copy_image") ||
	                                      GLExtensions::Supports("GL_NV_copy_image") ||
	                                      GLExtensions::Supports("GL_EXT_copy_image") ||
	                                      GLExtensions::Supports("GL_OES_copy_image")) &&
	                                      !DriverDetails::HasBug(DriverDetails::BUG_BROKENCOPYIMAGE);

	// Desktop OpenGL supports the binding layout if it supports 420pack
	// OpenGL ES 3.1 supports it implicitly without an extension
	g_Config.backend_info.bSupportsBindingLayout = GLExtensions::Supports("GL_ARB_shading_language_420pack");

	g_ogl_config.bSupportsGLSLCache = GLExtensions::Supports("GL_ARB_get_program_binary");
	g_ogl_config.bSupportsGLPinnedMemory = GLExtensions::Supports("GL_AMD_pinned_memory");
	g_ogl_config.bSupportsGLSync = GLExtensions::Supports("GL_ARB_sync");
	g_ogl_config.bSupportsGLBaseVertex = GLExtensions::Supports("GL_ARB_draw_elements_base_vertex") ||
	                                     GLExtensions::Supports("GL_EXT_draw_elements_base_vertex") ||
	                                     GLExtensions::Supports("GL_OES_draw_elements_base_vertex");
	g_ogl_config.bSupportsGLBufferStorage = GLExtensions::Supports("GL_ARB_buffer_storage") ||
	                                        GLExtensions::Supports("GL_EXT_buffer_storage");
	g_ogl_config.bSupportsMSAA = GLExtensions::Supports("GL_ARB_texture_multisample");
	g_ogl_config.bSupportViewportFloat = GLExtensions::Supports("GL_ARB_viewport_array");
	g_ogl_config.bSupportsDebug = GLExtensions::Supports("GL_KHR_debug") ||
	                              GLExtensions::Supports("GL_ARB_debug_output");
	g_ogl_config.bSupports3DTextureStorage = GLExtensions::Supports("GL_ARB_texture_storage_multisample") ||
	                                         GLExtensions::Supports("GL_OES_texture_storage_multisample_2d_array");
	g_ogl_config.bSupports2DTextureStorage = GLExtensions::Supports("GL_ARB_texture_storage_multisample");
	g_ogl_config.bSupportsEarlyFragmentTests = GLExtensions::Supports("GL_ARB_shader_image_load_store");
	g_ogl_config.bSupportsConservativeDepth = GLExtensions::Supports("GL_ARB_conservative_depth");
	g_ogl_config.bSupportsAniso = GLExtensions::Supports("GL_EXT_texture_filter_anisotropic");

	if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES3)
	{
		g_ogl_config.SupportedESPointSize = GLExtensions::Supports("GL_OES_geometry_point_size") ? 1 : GLExtensions::Supports("GL_EXT_geometry_point_size") ? 2 : 0;
		g_ogl_config.SupportedESTextureBuffer = GLExtensions::Supports("VERSION_GLES_3_2") ? ES_TEXBUF_TYPE::TEXBUF_CORE :
		                                        GLExtensions::Supports("GL_OES_texture_buffer") ? ES_TEXBUF_TYPE::TEXBUF_OES :
		                                        GLExtensions::Supports("GL_EXT_texture_buffer") ? ES_TEXBUF_TYPE::TEXBUF_EXT : ES_TEXBUF_TYPE::TEXBUF_NONE;

		g_ogl_config.bSupportsGLSLCache = true;
		g_ogl_config.bSupportsGLSync = true;

		if (strstr(g_ogl_config.glsl_version, "3.0") || DriverDetails::HasBug(DriverDetails::BUG_BROKENGLES31))
		{
			g_ogl_config.eSupportedGLSLVersion = GLSLES_300;
			g_ogl_config.bSupportsAEP = false;
			g_Config.backend_info.bSupportsGeometryShaders = false;
		}
		else if (strstr(g_ogl_config.glsl_version, "3.1"))
		{
			g_ogl_config.eSupportedGLSLVersion = GLSLES_310;
			g_ogl_config.bSupportsAEP = GLExtensions::Supports("GL_ANDROID_extension_pack_es31a");
			g_Config.backend_info.bSupportsBindingLayout = true;
			g_ogl_config.bSupportsEarlyFragmentTests = true;
			g_Config.backend_info.bSupportsGeometryShaders = g_ogl_config.bSupportsAEP;
			g_Config.backend_info.bSupportsGSInstancing = g_Config.backend_info.bSupportsGeometryShaders && g_ogl_config.SupportedESPointSize > 0;
			g_Config.backend_info.bSupportsSSAA = g_ogl_config.bSupportsAEP;
			g_Config.backend_info.bSupportsBBox = true;
			g_ogl_config.bSupportsMSAA = true;
			g_ogl_config.bSupports2DTextureStorage = true;
			if (g_ActiveConfig.iStereoMode > 0 && g_ActiveConfig.iMultisamples > 1 && !g_ogl_config.bSupports3DTextureStorage)
			{
				// GLES 3.1 can't support stereo rendering and MSAA
				OSD::AddMessage("MSAA Stereo rendering isn't supported by your GPU.", 10000);
				g_ActiveConfig.iMultisamples = 1;
			}
		}
		else
		{
			g_ogl_config.eSupportedGLSLVersion = GLSLES_320;
			g_ogl_config.bSupportsAEP = GLExtensions::Supports("GL_ANDROID_extension_pack_es31a");
			g_Config.backend_info.bSupportsBindingLayout = true;
			g_ogl_config.bSupportsEarlyFragmentTests = true;
			g_Config.backend_info.bSupportsGeometryShaders = true;
			g_Config.backend_info.bSupportsGSInstancing = g_ogl_config.SupportedESPointSize > 0;
			g_Config.backend_info.bSupportsPaletteConversion = true;
			g_Config.backend_info.bSupportsSSAA = true;
			g_Config.backend_info.bSupportsBBox = true;
			g_ogl_config.bSupportsCopySubImage = true;
			g_ogl_config.bSupportsGLBaseVertex = true;
			g_ogl_config.bSupportsDebug = true;
			g_ogl_config.bSupportsMSAA = true;
			g_ogl_config.bSupports2DTextureStorage = true;
			g_ogl_config.bSupports3DTextureStorage = true;
		}
	}
	else
	{
		if (strstr(g_ogl_config.glsl_version, "1.00") || strstr(g_ogl_config.glsl_version, "1.10") || strstr(g_ogl_config.glsl_version, "1.20"))
		{
			PanicAlert("GPU: OGL ERROR: Need at least GLSL 1.30\n"
					"GPU: Does your video card support OpenGL 3.0?\n"
					"GPU: Your driver supports GLSL %s", g_ogl_config.glsl_version);
			bSuccess = false;
		}
		else if (strstr(g_ogl_config.glsl_version, "1.30"))
		{
			g_ogl_config.eSupportedGLSLVersion = GLSL_130;
			g_ogl_config.bSupportsEarlyFragmentTests = false; // layout keyword is only supported on glsl150+
			g_ogl_config.bSupportsConservativeDepth = false; // layout keyword is only supported on glsl150+
			g_Config.backend_info.bSupportsGeometryShaders = false; // geometry shaders are only supported on glsl150+
		}
		else if (strstr(g_ogl_config.glsl_version, "1.40"))
		{
			g_ogl_config.eSupportedGLSLVersion = GLSL_140;
			g_ogl_config.bSupportsEarlyFragmentTests = false; // layout keyword is only supported on glsl150+
			g_ogl_config.bSupportsConservativeDepth = false; // layout keyword is only supported on glsl150+
			g_Config.backend_info.bSupportsGeometryShaders = false; // geometry shaders are only supported on glsl150+
		}
		else if (strstr(g_ogl_config.glsl_version, "1.50"))
		{
			g_ogl_config.eSupportedGLSLVersion = GLSL_150;
		}
		else if (strstr(g_ogl_config.glsl_version, "3.30"))
		{
			g_ogl_config.eSupportedGLSLVersion = GLSL_330;
		}
		else
		{
			g_ogl_config.eSupportedGLSLVersion = GLSL_400;
			g_Config.backend_info.bSupportsSSAA = true;
		}

		// Desktop OpenGL can't have the Android Extension Pack
		g_ogl_config.bSupportsAEP = false;
	}

	// Either method can do early-z tests. See PixelShaderGen for details.
	g_Config.backend_info.bSupportsEarlyZ = g_ogl_config.bSupportsEarlyFragmentTests || g_ogl_config.bSupportsConservativeDepth;

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
			glEnable(GL_DEBUG_OUTPUT);
		else
			glDisable(GL_DEBUG_OUTPUT);
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
			"%d samples on default framebuffer found.", samples);
		bSuccess = false;
	}

	if (!bSuccess)
	{
		// Not all needed extensions are supported, so we have to stop here.
		// Else some of the next calls might crash.
		return;
	}

	glGetIntegerv(GL_MAX_SAMPLES, &g_ogl_config.max_samples);
	if (g_ogl_config.max_samples < 1 || !g_ogl_config.bSupportsMSAA)
		g_ogl_config.max_samples = 1;

	g_Config.VerifyValidity();
	UpdateActiveConfig();

	OSD::AddMessage(StringFromFormat("Video Info: %s, %s, %s",
				g_ogl_config.gl_vendor,
				g_ogl_config.gl_renderer,
				g_ogl_config.gl_version), 5000);

	WARN_LOG(VIDEO,"Missing OGL Extensions: %s%s%s%s%s%s%s%s%s%s%s%s%s",
			g_ActiveConfig.backend_info.bSupportsDualSourceBlend ? "" : "DualSourceBlend ",
			g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ? "" : "PrimitiveRestart ",
			g_ActiveConfig.backend_info.bSupportsEarlyZ ? "" : "EarlyZ ",
			g_ogl_config.bSupportsGLPinnedMemory ? "" : "PinnedMemory ",
			g_ogl_config.bSupportsGLSLCache ? "" : "ShaderCache ",
			g_ogl_config.bSupportsGLBaseVertex ? "" : "BaseVertex ",
			g_ogl_config.bSupportsGLBufferStorage ? "" : "BufferStorage ",
			g_ogl_config.bSupportsGLSync ? "" : "Sync ",
			g_ogl_config.bSupportsMSAA ? "" : "MSAA ",
			g_ActiveConfig.backend_info.bSupportsSSAA ? "" : "SSAA ",
			g_ActiveConfig.backend_info.bSupportsGSInstancing ? "" : "GSInstancing ",
			g_ActiveConfig.backend_info.bSupportsClipControl ? "" : "ClipControl ",
			g_ogl_config.bSupportsCopySubImage ? "" : "CopyImageSubData "
			);

	s_last_multisamples = g_ActiveConfig.iMultisamples;
	s_MSAASamples = s_last_multisamples;

	s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;
	s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;

	// Decide framebuffer size
	s_backbuffer_width = (int)GLInterface->GetBackBufferWidth();
	s_backbuffer_height = (int)GLInterface->GetBackBufferHeight();

	// Handle VSync on/off
	s_vsync = g_ActiveConfig.IsVSync();
	if (!DriverDetails::HasBug(DriverDetails::BUG_BROKENVSYNC))
		GLInterface->SwapInterval(s_vsync);

	// TODO: Move these somewhere else?
	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

	s_last_efb_scale = g_ActiveConfig.iEFBScale;
	CalculateTargetSize(s_backbuffer_width, s_backbuffer_height);

	PixelShaderManager::SetEfbScaleChanged();

	// Because of the fixed framebuffer size we need to disable the resolution
	// options while running
	g_Config.bRunning = true;

	glStencilFunc(GL_ALWAYS, 0, 0);
	glBlendFunc(GL_ONE, GL_ONE);

	glViewport(0, 0, GetTargetWidth(), GetTargetHeight()); // Reset The Current Viewport
	if (g_ActiveConfig.backend_info.bSupportsClipControl)
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepthf(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment

	glDisable(GL_STENCIL_TEST);
	glEnable(GL_SCISSOR_TEST);

	glScissor(0, 0, GetTargetWidth(), GetTargetHeight());
	glBlendColor(0, 0, 0, 0.5f);
	glClearDepthf(1.0f);

	if (g_ActiveConfig.backend_info.bSupportsPrimitiveRestart)
	{
		if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES3)
		{
			glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
		}
		else
		{
			if (GLExtensions::Version() >= 310)
			{
				glEnable(GL_PRIMITIVE_RESTART);
				glPrimitiveRestartIndex(65535);
			}
			else
			{
				glEnableClientState(GL_PRIMITIVE_RESTART_NV);
				glPrimitiveRestartIndexNV(65535);
			}
		}
	}
	UpdateActiveConfig();
	ClearEFBCache();
}

Renderer::~Renderer()
{
}

void Renderer::Shutdown()
{
	g_framebuffer_manager.reset();

	g_Config.bRunning = false;
	UpdateActiveConfig();

	s_raster_font.reset();
	m_post_processor.reset();

	OpenGL_DeleteAttributelessVAO();
}

void Renderer::Init()
{
	// Initialize the FramebufferManager
	g_framebuffer_manager = std::make_unique<FramebufferManager>(s_target_width, s_target_height,
			s_MSAASamples);

	m_post_processor = std::make_unique<OpenGLPostProcessing>();
	s_raster_font = std::make_unique<RasterFont>();

	OpenGL_CreateAttributelessVAO();
}

void Renderer::RenderText(const std::string& text, int left, int top, u32 color)
{
	const int nBackbufferWidth = (int)GLInterface->GetBackBufferWidth();
	const int nBackbufferHeight = (int)GLInterface->GetBackBufferHeight();

	s_raster_font->printMultilineText(text,
		left * 2.0f / (float)nBackbufferWidth - 1,
		1 - top * 2.0f / (float)nBackbufferHeight,
		0, nBackbufferWidth, nBackbufferHeight, color);
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	TargetRectangle result;
	result.left   = EFBToScaledX(rc.left);
	result.top    = EFBToScaledY(EFB_HEIGHT - rc.top);
	result.right  = EFBToScaledX(rc.right);
	result.bottom = EFBToScaledY(EFB_HEIGHT - rc.bottom);
	return result;
}

// Function: This function handles the OpenGL glScissor() function
// ----------------------------
// Call browser: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg()
//		case 0x52 > SetScissorRect()
// ----------------------------
// bpmem.scissorTL.x, y = 342x342
// bpmem.scissorBR.x, y = 981x821
// Renderer::GetTargetHeight() = the fixed ini file setting
// donkopunchstania - it appears scissorBR is the bottom right pixel inside the scissor box
// therefore the width and height are (scissorBR + 1) - scissorTL
void Renderer::SetScissorRect(const EFBRectangle& rc)
{
	TargetRectangle trc = g_renderer->ConvertEFBRectangle(rc);
	glScissor(trc.left, trc.bottom, trc.GetWidth(), trc.GetHeight());
}

void Renderer::SetColorMask()
{
	// Only enable alpha channel if it's supported by the current EFB format
	GLenum ColorMask = GL_FALSE, AlphaMask = GL_FALSE;
	if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
	{
		if (bpmem.blendmode.colorupdate)
			ColorMask = GL_TRUE;
		if (bpmem.blendmode.alphaupdate && (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24))
			AlphaMask = GL_TRUE;
	}
	glColorMask(ColorMask,  ColorMask,  ColorMask,  AlphaMask);
}

void ClearEFBCache()
{
	if (!s_efbCacheIsCleared)
	{
		s_efbCacheIsCleared = true;
		memset(s_efbCacheValid, 0, sizeof(s_efbCacheValid));
	}
}

void Renderer::UpdateEFBCache(EFBAccessType type, u32 cacheRectIdx, const EFBRectangle& efbPixelRc, const TargetRectangle& targetPixelRc, const void* data)
{
	u32 cacheType = (type == PEEK_Z ? 0 : 1);

	if (!s_efbCache[cacheType][cacheRectIdx].size())
		s_efbCache[cacheType][cacheRectIdx].resize(EFB_CACHE_RECT_SIZE * EFB_CACHE_RECT_SIZE);

	u32 targetPixelRcWidth = targetPixelRc.right - targetPixelRc.left;
	u32 efbPixelRcHeight = efbPixelRc.bottom - efbPixelRc.top;
	u32 efbPixelRcWidth = efbPixelRc.right - efbPixelRc.left;

	for (u32 yCache = 0; yCache < efbPixelRcHeight; ++yCache)
	{
		u32 yEFB = efbPixelRc.top + yCache;
		u32 yPixel = (EFBToScaledY(EFB_HEIGHT - yEFB) + EFBToScaledY(EFB_HEIGHT - yEFB - 1)) / 2;
		u32 yData = yPixel - targetPixelRc.bottom;

		for (u32 xCache = 0; xCache < efbPixelRcWidth; ++xCache)
		{
			u32 xEFB = efbPixelRc.left + xCache;
			u32 xPixel = (EFBToScaledX(xEFB) + EFBToScaledX(xEFB + 1)) / 2;
			u32 xData = xPixel - targetPixelRc.left;
			u32 value;
			if (type == PEEK_Z)
			{
				float* ptr = (float*)data;
				value = MathUtil::Clamp<u32>((u32)(ptr[yData * targetPixelRcWidth + xData] * 16777216.0f), 0, 0xFFFFFF);
			}
			else
			{
				u32* ptr = (u32*)data;
				value = ptr[yData * targetPixelRcWidth + xData];
			}
			s_efbCache[cacheType][cacheRectIdx][yCache * EFB_CACHE_RECT_SIZE + xCache] = value;
		}
	}

	s_efbCacheValid[cacheType][cacheRectIdx] = true;
	s_efbCacheIsCleared = false;
}

// This function allows the CPU to directly access the EFB.
// There are EFB peeks (which will read the color or depth of a pixel)
// and EFB pokes (which will change the color or depth of a pixel).
//
// The behavior of EFB peeks can only be modified by:
// - GX_PokeAlphaRead
// The behavior of EFB pokes can be modified by:
// - GX_PokeAlphaMode (TODO)
// - GX_PokeAlphaUpdate (TODO)
// - GX_PokeBlendMode (TODO)
// - GX_PokeColorUpdate (TODO)
// - GX_PokeDither (TODO)
// - GX_PokeDstAlpha (TODO)
// - GX_PokeZMode (TODO)
u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
	u32 cacheRectIdx = (y / EFB_CACHE_RECT_SIZE) * EFB_CACHE_WIDTH
	                 + (x / EFB_CACHE_RECT_SIZE);

	EFBRectangle efbPixelRc;

	if (type == PEEK_COLOR || type == PEEK_Z)
	{
		// Get the rectangular target region containing the EFB pixel
		efbPixelRc.left = (x / EFB_CACHE_RECT_SIZE) * EFB_CACHE_RECT_SIZE;
		efbPixelRc.top = (y / EFB_CACHE_RECT_SIZE) * EFB_CACHE_RECT_SIZE;
		efbPixelRc.right = std::min(efbPixelRc.left + EFB_CACHE_RECT_SIZE, (u32)EFB_WIDTH);
		efbPixelRc.bottom = std::min(efbPixelRc.top + EFB_CACHE_RECT_SIZE, (u32)EFB_HEIGHT);
	}
	else
	{
		efbPixelRc.left = x;
		efbPixelRc.top = y;
		efbPixelRc.right = x+1;
		efbPixelRc.bottom = y+1;
	}

	TargetRectangle targetPixelRc = ConvertEFBRectangle(efbPixelRc);
	u32 targetPixelRcWidth = targetPixelRc.right - targetPixelRc.left;
	u32 targetPixelRcHeight = targetPixelRc.top - targetPixelRc.bottom;

	// TODO (FIX) : currently, AA path is broken/offset and doesn't return the correct pixel
	switch (type)
	{
	case PEEK_Z:
		{
			if (!s_efbCacheValid[0][cacheRectIdx])
			{
				if (s_MSAASamples > 1)
				{
					g_renderer->ResetAPIState();

					// Resolve our rectangle.
					FramebufferManager::GetEFBDepthTexture(efbPixelRc);
					glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferManager::GetResolvedFramebuffer());

					g_renderer->RestoreAPIState();
				}

				std::unique_ptr<float[]> depthMap(new float[targetPixelRcWidth * targetPixelRcHeight]);

				glReadPixels(targetPixelRc.left, targetPixelRc.bottom, targetPixelRcWidth, targetPixelRcHeight,
				             GL_DEPTH_COMPONENT, GL_FLOAT, depthMap.get());

				UpdateEFBCache(type, cacheRectIdx, efbPixelRc, targetPixelRc, depthMap.get());
			}

			u32 xRect = x % EFB_CACHE_RECT_SIZE;
			u32 yRect = y % EFB_CACHE_RECT_SIZE;
			u32 z = s_efbCache[0][cacheRectIdx][yRect * EFB_CACHE_RECT_SIZE + xRect];

			// if Z is in 16 bit format you must return a 16 bit integer
			if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
				z = z >> 8;

			return z;
		}

	case PEEK_COLOR: // GXPeekARGB
		{
			// Although it may sound strange, this really is A8R8G8B8 and not RGBA or 24-bit...

			// Tested in Killer 7, the first 8bits represent the alpha value which is used to
			// determine if we're aiming at an enemy (0x80 / 0x88) or not (0x70)
			// Wind Waker is also using it for the pictograph to determine the color of each pixel
			if (!s_efbCacheValid[1][cacheRectIdx])
			{
				if (s_MSAASamples > 1)
				{
					g_renderer->ResetAPIState();

					// Resolve our rectangle.
					FramebufferManager::GetEFBColorTexture(efbPixelRc);
					glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferManager::GetResolvedFramebuffer());

					g_renderer->RestoreAPIState();
				}

				std::unique_ptr<u32[]> colorMap(new u32[targetPixelRcWidth * targetPixelRcHeight]);

				if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGLES3)
				// XXX: Swap colours
					glReadPixels(targetPixelRc.left, targetPixelRc.bottom, targetPixelRcWidth, targetPixelRcHeight,
						     GL_RGBA, GL_UNSIGNED_BYTE, colorMap.get());
				else
					glReadPixels(targetPixelRc.left, targetPixelRc.bottom, targetPixelRcWidth, targetPixelRcHeight,
						     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, colorMap.get());

				UpdateEFBCache(type, cacheRectIdx, efbPixelRc, targetPixelRc, colorMap.get());
			}

			u32 xRect = x % EFB_CACHE_RECT_SIZE;
			u32 yRect = y % EFB_CACHE_RECT_SIZE;
			u32 color = s_efbCache[1][cacheRectIdx][yRect * EFB_CACHE_RECT_SIZE + xRect];

			// check what to do with the alpha channel (GX_PokeAlphaRead)
			PixelEngine::UPEAlphaReadReg alpha_read_mode = PixelEngine::GetAlphaReadMode();

			if (bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24)
			{
				color = RGBA8ToRGBA6ToRGBA8(color);
			}
			else if (bpmem.zcontrol.pixel_format == PEControl::RGB565_Z16)
			{
				color = RGBA8ToRGB565ToRGBA8(color);
			}
			if (bpmem.zcontrol.pixel_format != PEControl::RGBA6_Z24)
			{
				color |= 0xFF000000;
			}
			if (alpha_read_mode.ReadMode == 2)
			{
				// GX_READ_NONE
				return color;
			}
			else if (alpha_read_mode.ReadMode == 1)
			{
				// GX_READ_FF
				return (color | 0xFF000000);
			}
			else /*if(alpha_read_mode.ReadMode == 0)*/
			{
				// GX_READ_00
				return (color & 0x00FFFFFF);
			}
		}

	default:
		break;
	}

	return 0;
}

void Renderer::PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points)
{
	FramebufferManager::PokeEFB(type, points, num_points);
}

u16 Renderer::BBoxRead(int index)
{
	int swapped_index = index;
	if (index >= 2)
		swapped_index ^= 1; // swap 2 and 3 for top/bottom

	// Here we get the min/max value of the truncated position of the upscaled and swapped framebuffer.
	// So we have to correct them to the unscaled EFB sizes.
	int value = BoundingBox::Get(swapped_index);

	if (index < 2)
	{
		// left/right
		value = value * EFB_WIDTH / s_target_width;
	}
	else
	{
		// up/down -- we have to swap up and down
		value = value * EFB_HEIGHT / s_target_height;
		value = EFB_HEIGHT - value - 1;
	}
	if (index & 1)
		value++; // fix max values to describe the outer border

	return value;
}

void Renderer::BBoxWrite(int index, u16 _value)
{
	int value = _value; // u16 isn't enough to multiply by the efb width
	if (index & 1)
		value--;
	if (index < 2)
	{
		value = value * s_target_width / EFB_WIDTH;
	}
	else
	{
		index ^= 1; // swap 2 and 3 for top/bottom
		value = EFB_HEIGHT - value - 1;
		value = value * s_target_height / EFB_HEIGHT;
	}

	BoundingBox::Set(index, value);
}

void Renderer::SetViewport()
{
	// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
	// [0] = width/2
	// [1] = height/2
	// [2] = 16777215 * (farz - nearz)
	// [3] = xorig + width/2 + 342
	// [4] = yorig + height/2 + 342
	// [5] = 16777215 * farz

	int scissorXOff = bpmem.scissorOffset.x * 2;
	int scissorYOff = bpmem.scissorOffset.y * 2;

	// TODO: ceil, floor or just cast to int?
	float X = EFBToScaledXf(xfmem.viewport.xOrig - xfmem.viewport.wd - (float)scissorXOff);
	float Y = EFBToScaledYf((float)EFB_HEIGHT - xfmem.viewport.yOrig + xfmem.viewport.ht + (float)scissorYOff);
	float Width = EFBToScaledXf(2.0f * xfmem.viewport.wd);
	float Height = EFBToScaledYf(-2.0f * xfmem.viewport.ht);
	float GLNear = MathUtil::Clamp<float>(xfmem.viewport.farZ - MathUtil::Clamp<float>(xfmem.viewport.zRange, -16777216.0f, 16777216.0f), 0.0f, 16777215.0f) / 16777216.0f;
	float GLFar = MathUtil::Clamp<float>(xfmem.viewport.farZ, 0.0f, 16777215.0f) / 16777216.0f;
	if (Width < 0)
	{
		X += Width;
		Width *= -1;
	}
	if (Height < 0)
	{
		Y += Height;
		Height *= -1;
	}

	// Update the view port
	if (g_ogl_config.bSupportViewportFloat)
	{
		glViewportIndexedf(0, X, Y, Width, Height);
	}
	else
	{
		auto iceilf = [](float f)
		{
			return static_cast<GLint>(ceilf(f));
		};
		glViewport(iceilf(X), iceilf(Y), iceilf(Width), iceilf(Height));
	}
	glDepthRangef(GLFar, GLNear);
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	ResetAPIState();

	// color
	GLboolean const
		color_mask = colorEnable ? GL_TRUE : GL_FALSE,
		alpha_mask = alphaEnable ? GL_TRUE : GL_FALSE;
	glColorMask(color_mask,  color_mask,  color_mask,  alpha_mask);

	glClearColor(
		float((color >> 16) & 0xFF) / 255.0f,
		float((color >> 8) & 0xFF) / 255.0f,
		float((color >> 0) & 0xFF) / 255.0f,
		float((color >> 24) & 0xFF) / 255.0f);

	// depth
	glDepthMask(zEnable ? GL_TRUE : GL_FALSE);

	glClearDepthf(float(z & 0xFFFFFF) / 16777216.0f);

	// Update rect for clearing the picture
	glEnable(GL_SCISSOR_TEST);

	TargetRectangle const targetRc = ConvertEFBRectangle(rc);
	glScissor(targetRc.left, targetRc.bottom, targetRc.GetWidth(), targetRc.GetHeight());

	// glColorMask/glDepthMask/glScissor affect glClear (glViewport does not)
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	RestoreAPIState();

	ClearEFBCache();
}

void Renderer::BlitScreen(TargetRectangle src, TargetRectangle dst, GLuint src_texture, int src_width, int src_height)
{
	if (g_ActiveConfig.iStereoMode == STEREO_SBS || g_ActiveConfig.iStereoMode == STEREO_TAB)
	{
		TargetRectangle leftRc, rightRc;

		// Top-and-Bottom mode needs to compensate for inverted vertical screen coordinates.
		if (g_ActiveConfig.iStereoMode == STEREO_TAB)
			ConvertStereoRectangle(dst, rightRc, leftRc);
		else
			ConvertStereoRectangle(dst, leftRc, rightRc);

		m_post_processor->BlitFromTexture(src, leftRc, src_texture, src_width, src_height, 0);
		m_post_processor->BlitFromTexture(src, rightRc, src_texture, src_width, src_height, 1);
	}
	else
	{
		m_post_processor->BlitFromTexture(src, dst, src_texture, src_width, src_height);
	}
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	if (convtype == 0 || convtype == 2)
	{
		FramebufferManager::ReinterpretPixelData(convtype);
	}
	else
	{
		ERROR_LOG(VIDEO, "Trying to reinterpret pixel data with unsupported conversion type %d", convtype);
	}
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	// Our render target always uses an alpha channel, so we need to override the blend functions to assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;

	bool useDstAlpha = bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate && target_has_alpha;
	bool useDualSource = useDstAlpha && g_ActiveConfig.backend_info.bSupportsDualSourceBlend;

	const GLenum glSrcFactors[8] =
	{
		GL_ZERO,
		GL_ONE,
		GL_DST_COLOR,
		GL_ONE_MINUS_DST_COLOR,
		(useDualSource)  ? GL_SRC1_ALPHA : (GLenum)GL_SRC_ALPHA,
		(useDualSource)  ? GL_ONE_MINUS_SRC1_ALPHA : (GLenum)GL_ONE_MINUS_SRC_ALPHA,
		(target_has_alpha) ? GL_DST_ALPHA : (GLenum)GL_ONE,
		(target_has_alpha) ? GL_ONE_MINUS_DST_ALPHA : (GLenum)GL_ZERO
	};
	const GLenum glDestFactors[8] =
	{
		GL_ZERO,
		GL_ONE,
		GL_SRC_COLOR,
		GL_ONE_MINUS_SRC_COLOR,
		(useDualSource)  ? GL_SRC1_ALPHA : (GLenum)GL_SRC_ALPHA,
		(useDualSource)  ? GL_ONE_MINUS_SRC1_ALPHA : (GLenum)GL_ONE_MINUS_SRC_ALPHA,
		(target_has_alpha) ? GL_DST_ALPHA : (GLenum)GL_ONE,
		(target_has_alpha) ? GL_ONE_MINUS_DST_ALPHA : (GLenum)GL_ZERO
	};

	// blend mode bit mask
	// 0 - blend enable
	// 1 - dst alpha enabled
	// 2 - reverse subtract enable (else add)
	// 3-5 - srcRGB function
	// 6-8 - dstRGB function

	u32 newval = useDualSource << 1;
	newval |= bpmem.blendmode.subtract << 2;

	if (bpmem.blendmode.subtract)
	{
		newval |= 0x0049;   // enable blending src 1 dst 1
	}
	else if (bpmem.blendmode.blendenable)
	{
		newval |= 1;    // enable blending
		newval |= bpmem.blendmode.srcfactor << 3;
		newval |= bpmem.blendmode.dstfactor << 6;
	}

	u32 changes = forceUpdate ? 0xFFFFFFFF : newval ^ s_blendMode;

	if (changes & 1)
	{
		// blend enable change
		(newval & 1) ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
	}

	if (changes & 4)
	{
		// subtract enable change
		GLenum equation = newval & 4 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD;
		GLenum equationAlpha = useDualSource ? GL_FUNC_ADD : equation;

		glBlendEquationSeparate(equation, equationAlpha);
	}

	if (changes & 0x1FA)
	{
		u32 srcidx = (newval >> 3) & 7;
		u32 dstidx = (newval >> 6) & 7;
		GLenum srcFactor = glSrcFactors[srcidx];
		GLenum dstFactor = glDestFactors[dstidx];

		// adjust alpha factors
		if (useDualSource)
		{
			srcidx = BlendMode::ONE;
			dstidx = BlendMode::ZERO;
		}
		else
		{
			// we can't use GL_DST_COLOR or GL_ONE_MINUS_DST_COLOR for source in alpha channel so use their alpha equivalent instead
			if (srcidx == BlendMode::DSTCLR)
				srcidx = BlendMode::DSTALPHA;
			else if (srcidx == BlendMode::INVDSTCLR)
				srcidx = BlendMode::INVDSTALPHA;

			// we can't use GL_SRC_COLOR or GL_ONE_MINUS_SRC_COLOR for destination in alpha channel so use their alpha equivalent instead
			if (dstidx == BlendMode::SRCCLR)
				dstidx = BlendMode::SRCALPHA;
			else if (dstidx == BlendMode::INVSRCCLR)
				dstidx = BlendMode::INVSRCALPHA;
		}
		GLenum srcFactorAlpha = glSrcFactors[srcidx];
		GLenum dstFactorAlpha = glDestFactors[dstidx];
		// blend RGB change
		glBlendFuncSeparate(srcFactor, dstFactor, srcFactorAlpha, dstFactorAlpha);
	}
	s_blendMode = newval;
}

static void DumpFrame(const std::vector<u8>& data, int w, int h)
{
#if defined(HAVE_LIBAV) || defined (_WIN32)
	if (SConfig::GetInstance().m_DumpFrames && !data.empty())
	{
		AVIDump::AddFrame(&data[0], w, h);
	}
#endif
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::SwapImpl(u32 xfbAddr, u32 fbWidth, u32 fbStride, u32 fbHeight, const EFBRectangle& rc, float Gamma)
{
	if (g_ogl_config.bSupportsDebug)
	{
		if (LogManager::GetInstance()->IsEnabled(LogTypes::HOST_GPU, LogTypes::LERROR))
			glEnable(GL_DEBUG_OUTPUT);
		else
			glDisable(GL_DEBUG_OUTPUT);
	}

	static int w = 0, h = 0;
	if (Fifo::g_bSkipCurrentFrame || (!XFBWrited && !g_ActiveConfig.RealXFBEnabled()) || !fbWidth || !fbHeight)
	{
		DumpFrame(frame_data, w, h);
		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	u32 xfbCount = 0;
	const XFBSourceBase* const* xfbSourceList = FramebufferManager::GetXFBSource(xfbAddr, fbStride, fbHeight, &xfbCount);
	if (g_ActiveConfig.VirtualXFBEnabled() && (!xfbSourceList || xfbCount == 0))
	{
		DumpFrame(frame_data, w, h);
		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	ResetAPIState();

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
	TargetRectangle flipped_trc = GetTargetRectangle();

	// Flip top and bottom for some reason; TODO: Fix the code to suck less?
	std::swap(flipped_trc.top, flipped_trc.bottom);

	// Copy the framebuffer to screen.
	const XFBSource* xfbSource = nullptr;

	if (g_ActiveConfig.bUseXFB)
	{
		// draw each xfb source
		for (u32 i = 0; i < xfbCount; ++i)
		{
			xfbSource = (const XFBSource*) xfbSourceList[i];

			TargetRectangle drawRc;
			TargetRectangle sourceRc;
			sourceRc.left = xfbSource->sourceRc.left;
			sourceRc.right = xfbSource->sourceRc.right;
			sourceRc.top = xfbSource->sourceRc.top;
			sourceRc.bottom = xfbSource->sourceRc.bottom;

			if (g_ActiveConfig.bUseRealXFB)
			{
				drawRc = flipped_trc;
				sourceRc.right -= fbStride - fbWidth;
			}
			else
			{
				// use virtual xfb with offset
				int xfbHeight = xfbSource->srcHeight;
				int xfbWidth = xfbSource->srcWidth;
				int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbStride * 2);

				drawRc.top = flipped_trc.top - hOffset * flipped_trc.GetHeight() / (s32)fbHeight;
				drawRc.bottom = flipped_trc.top - (hOffset + xfbHeight) * flipped_trc.GetHeight() / (s32)fbHeight;
				drawRc.left = flipped_trc.left + (flipped_trc.GetWidth() - xfbWidth * flipped_trc.GetWidth() / (s32)fbStride) / 2;
				drawRc.right = flipped_trc.left + (flipped_trc.GetWidth() + xfbWidth * flipped_trc.GetWidth() / (s32)fbStride) / 2;

				// The following code disables auto stretch.  Kept for reference.
				// scale draw area for a 1 to 1 pixel mapping with the draw target
				//float vScale = (float)fbHeight / (float)flipped_trc.GetHeight();
				//float hScale = (float)fbWidth / (float)flipped_trc.GetWidth();
				//drawRc.top *= vScale;
				//drawRc.bottom *= vScale;
				//drawRc.left *= hScale;
				//drawRc.right *= hScale;

				sourceRc.right -= Renderer::EFBToScaledX(fbStride - fbWidth);
			}
			// Tell the OSD Menu about the current internal resolution
			OSDInternalW = xfbSource->sourceRc.GetWidth(); OSDInternalH = xfbSource->sourceRc.GetHeight();

			BlitScreen(sourceRc, drawRc, xfbSource->texture, xfbSource->texWidth, xfbSource->texHeight);
		}
	}
	else
	{
		TargetRectangle targetRc = ConvertEFBRectangle(rc);

		// for msaa mode, we must resolve the efb content to non-msaa
		GLuint tex = FramebufferManager::ResolveAndGetRenderTarget(rc);
		BlitScreen(targetRc, flipped_trc, tex, s_target_width, s_target_height);
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	// Save screenshot
	if (s_bScreenshot)
	{
		std::lock_guard<std::mutex> lk(s_criticalScreenshot);

		if (SaveScreenshot(s_sScreenshotName, flipped_trc))
			OSD::AddMessage("Screenshot saved to " + s_sScreenshotName);

		// Reset settings
		s_sScreenshotName.clear();
		s_bScreenshot = false;
		s_screenshotCompleted.Set();
	}

	// Frame dumps are handled a little differently in Windows
	// Frame dumping disabled entirely on GLES3
	if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGL)
	{
#if defined(HAVE_LIBAV) || defined (_WIN32)
		if (SConfig::GetInstance().m_DumpFrames)
		{
			std::lock_guard<std::mutex> lk(s_criticalScreenshot);
			if (frame_data.empty() || w != flipped_trc.GetWidth() ||
				     h != flipped_trc.GetHeight())
			{
				w = flipped_trc.GetWidth();
				h = flipped_trc.GetHeight();
				frame_data.resize(3 * w * h);
			}
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glReadPixels(flipped_trc.left, flipped_trc.bottom, w, h, GL_BGR, GL_UNSIGNED_BYTE, &frame_data[0]);
			if (w > 0 && h > 0)
			{
				if (!bLastFrameDumped)
				{
					bAVIDumping = AVIDump::Start(w, h);
					if (!bAVIDumping)
					{
						OSD::AddMessage("AVIDump Start failed", 2000);
					}
					else
					{
						OSD::AddMessage(StringFromFormat(
									"Dumping Frames to \"%sframedump0.avi\" (%dx%d RGB24)",
									File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), w, h), 2000);
					}
				}
				if (bAVIDumping)
				{
					FlipImageData(&frame_data[0], w, h);
					AVIDump::AddFrame(&frame_data[0], w, h);
				}

				bLastFrameDumped = true;
			}
			else
			{
				NOTICE_LOG(VIDEO, "Error reading framebuffer");
			}
		}
		else
		{
			if (bLastFrameDumped && bAVIDumping)
			{
				std::vector<u8>().swap(frame_data);
				w = h = 0;
				AVIDump::Stop();
				bAVIDumping = false;
				OSD::AddMessage("Stop dumping frames", 2000);
			}
			bLastFrameDumped = false;
		}
#endif
	}
	// Finish up the current frame, print some stats

	SetWindowSize(fbStride, fbHeight);

	GLInterface->Update(); // just updates the render window position and the backbuffer size

	bool xfbchanged = s_last_xfb_mode != g_ActiveConfig.bUseRealXFB;

	if (FramebufferManagerBase::LastXfbWidth() != fbStride || FramebufferManagerBase::LastXfbHeight() != fbHeight)
	{
		xfbchanged = true;
		unsigned int const last_w = (fbStride < 1 || fbStride > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fbStride;
		unsigned int const last_h = (fbHeight < 1 || fbHeight > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fbHeight;
		FramebufferManagerBase::SetLastXfbWidth(last_w);
		FramebufferManagerBase::SetLastXfbHeight(last_h);
	}

	bool WindowResized = false;
	int W = (int)GLInterface->GetBackBufferWidth();
	int H = (int)GLInterface->GetBackBufferHeight();
	if (W != s_backbuffer_width || H != s_backbuffer_height || s_last_efb_scale != g_ActiveConfig.iEFBScale)
	{
		WindowResized = true;
		s_backbuffer_width = W;
		s_backbuffer_height = H;
		s_last_efb_scale = g_ActiveConfig.iEFBScale;
	}
	bool TargetSizeChanged = false;
	if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height))
	{
		TargetSizeChanged = true;
	}
	if (TargetSizeChanged || xfbchanged || WindowResized ||
	    (s_last_multisamples != g_ActiveConfig.iMultisamples) || (s_last_stereo_mode != (g_ActiveConfig.iStereoMode > 0)))
	{
		s_last_xfb_mode = g_ActiveConfig.bUseRealXFB;

		UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

		if (TargetSizeChanged ||
		    s_last_multisamples != g_ActiveConfig.iMultisamples || s_last_stereo_mode != (g_ActiveConfig.iStereoMode > 0))
		{
			s_last_stereo_mode = g_ActiveConfig.iStereoMode > 0;
			s_last_multisamples = g_ActiveConfig.iMultisamples;
			s_MSAASamples = s_last_multisamples;

			if (s_MSAASamples > 1 && s_MSAASamples > g_ogl_config.max_samples)
			{
				s_MSAASamples = g_ogl_config.max_samples;
				OSD::AddMessage(StringFromFormat("%d Anti Aliasing samples selected, but only %d supported by your GPU.", s_last_multisamples, g_ogl_config.max_samples), 10000);
			}

			g_framebuffer_manager.reset();
			g_framebuffer_manager = std::make_unique<FramebufferManager>(s_target_width, s_target_height, s_MSAASamples);

			PixelShaderManager::SetEfbScaleChanged();
		}
	}

	// ---------------------------------------------------------------------
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Reset viewport for drawing text
	glViewport(0, 0, GLInterface->GetBackBufferWidth(), GLInterface->GetBackBufferHeight());

	DrawDebugText();

	// Do our OSD callbacks
	OSD::DoCallbacks(OSD::CallbackType::OnFrame);
	OSD::DrawMessages();

	if (s_SurfaceNeedsChanged.IsSet())
	{
		GLInterface->UpdateSurface();
		s_SurfaceNeedsChanged.Clear();
		s_ChangedSurface.Set();
	}

	// Copy the rendered frame to the real window
	GLInterface->Swap();

	// Clear framebuffer
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (s_vsync != g_ActiveConfig.IsVSync())
	{
		s_vsync = g_ActiveConfig.IsVSync();
		if (!DriverDetails::HasBug(DriverDetails::BUG_BROKENVSYNC))
			GLInterface->SwapInterval(s_vsync);
	}

	// Clean out old stuff from caches. It's not worth it to clean out the shader caches.
	TextureCache::Cleanup(frameCount);

	// Render to the framebuffer.
	FramebufferManager::SetFramebuffer(0);

	RestoreAPIState();

	g_Config.iSaveTargetId = 0;

	UpdateActiveConfig();
	TextureCache::OnConfigChanged(g_ActiveConfig);

	// For testing zbuffer targets.
	// Renderer::SetZBufferRender();
	// SaveTexture("tex.png", GL_TEXTURE_2D, s_FakeZTarget,
	//	      GetTargetWidth(), GetTargetHeight());

	// Invalidate EFB cache
	ClearEFBCache();
}

// ALWAYS call RestoreAPIState for each ResetAPIState call you're doing
void Renderer::ResetAPIState()
{
	// Gets us to a reasonably sane state where it's possible to do things like
	// image copies with textured quads, etc.
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGL)
		glDisable(GL_COLOR_LOGIC_OP);
	glDepthMask(GL_FALSE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void Renderer::RestoreAPIState()
{
	// Gets us back into a more game-like state.
	glEnable(GL_SCISSOR_TEST);
	SetGenerationMode();
	BPFunctions::SetScissor();
	SetColorMask();
	SetDepthMode();
	SetBlendMode(true);
	SetLogicOpMode();
	SetViewport();

	const VertexManager* const vm = static_cast<VertexManager*>(g_vertex_manager.get());
	glBindBuffer(GL_ARRAY_BUFFER, vm->m_vertex_buffers);
	if (vm->m_last_vao)
		glBindVertexArray(vm->m_last_vao);

	TextureCache::SetStage();
}

void Renderer::SetGenerationMode()
{
	// none, ccw, cw, ccw
	if (bpmem.genMode.cullmode > 0)
	{
		// TODO: GX_CULL_ALL not supported, yet!
		glEnable(GL_CULL_FACE);
		glFrontFace(bpmem.genMode.cullmode == 2 ? GL_CCW : GL_CW);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}
}

void Renderer::SetDepthMode()
{
	const GLenum glCmpFuncs[8] =
	{
		GL_NEVER,
		GL_LESS,
		GL_EQUAL,
		GL_LEQUAL,
		GL_GREATER,
		GL_NOTEQUAL,
		GL_GEQUAL,
		GL_ALWAYS
	};

	if (bpmem.zmode.testenable)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthMask(bpmem.zmode.updateenable ? GL_TRUE : GL_FALSE);
		glDepthFunc(glCmpFuncs[bpmem.zmode.func]);
	}
	else
	{
		// if the test is disabled write is disabled too
		// TODO: When PE performance metrics are being emulated via occlusion queries, we should (probably?) enable depth test with depth function ALWAYS here
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
}

void Renderer::SetLogicOpMode()
{
	if (GLInterface->GetMode() != GLInterfaceMode::MODE_OPENGL)
		return;
	// Logic ops aren't available in GLES3/GLES2
	const GLenum glLogicOpCodes[16] =
	{
		GL_CLEAR,
		GL_AND,
		GL_AND_REVERSE,
		GL_COPY,
		GL_AND_INVERTED,
		GL_NOOP,
		GL_XOR,
		GL_OR,
		GL_NOR,
		GL_EQUIV,
		GL_INVERT,
		GL_OR_REVERSE,
		GL_COPY_INVERTED,
		GL_OR_INVERTED,
		GL_NAND,
		GL_SET
	};

	if (bpmem.blendmode.logicopenable && !bpmem.blendmode.blendenable)
	{
		glEnable(GL_COLOR_LOGIC_OP);
		glLogicOp(glLogicOpCodes[bpmem.blendmode.logicmode]);
	}
	else
	{
		glDisable(GL_COLOR_LOGIC_OP);
	}
}

void Renderer::SetDitherMode()
{
	if (bpmem.blendmode.dither)
		glEnable(GL_DITHER);
	else
		glDisable(GL_DITHER);
}

void Renderer::SetSamplerState(int stage, int texindex, bool custom_tex)
{
	auto const& tex = bpmem.tex[texindex];
	auto const& tm0 = tex.texMode0[stage];
	auto const& tm1 = tex.texMode1[stage];

	g_sampler_cache->SetSamplerState((texindex * 4) + stage, tm0, tm1, custom_tex);
}

void Renderer::SetInterlacingMode()
{
	// TODO
}

}

namespace OGL
{

bool Renderer::SaveScreenshot(const std::string &filename, const TargetRectangle &back_rc)
{
	u32 W = back_rc.GetWidth();
	u32 H = back_rc.GetHeight();
	std::unique_ptr<u8[]> data(new u8[W * 4 * H]);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	glReadPixels(back_rc.left, back_rc.bottom, W, H, GL_RGBA, GL_UNSIGNED_BYTE, data.get());

	// Turn image upside down
	FlipImageData(data.get(), W, H, 4);

	return TextureToPng(data.get(), W * 4, filename, W, H, false);

}

int Renderer::GetMaxTextureSize()
{
	int max_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
	return max_size;
}

}
