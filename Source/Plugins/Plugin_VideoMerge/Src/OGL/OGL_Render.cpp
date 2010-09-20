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


#include <vector>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
#include <mmsystem.h>
#endif

#ifdef _WIN32
//#include "OS/Win32.h"
//#include "AVIDump.h"

#include <wx/image.h>
#endif

#if defined(HAVE_WX) && HAVE_WX
#include <wx/image.h>
#endif

// Common
#include "Thread.h"
#include "Atomic.h"
#include "FileUtil.h"
#include "CommonPaths.h"
#include "Timer.h"
#include "StringUtil.h"

// VideoCommon
#include "VideoConfig.h"
#include "Profiler.h"
#include "Statistics.h"
#include "ImageWrite.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "VertexShaderGen.h"
#include "DLCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "VertexLoaderManager.h"
#include "VertexLoader.h"
#include "OnScreenDisplay.h"
#include "Fifo.h"

// OGL
#include "OGL_GLUtil.h"
#include "OGL_TextureCache.h"
#include "OGL_RasterFont.h"
#include "OGL_PixelShaderCache.h"
#include "OGL_VertexShaderCache.h"
#include "OGL_PostProcessing.h"
#include "OGL_TextureConverter.h"
#include "OGL_FramebufferManager.h"
#include "OGL_XFB.h"
#include "OGL_Render.h"

#include "../Main.h"

namespace OGL
{

// Declarations and definitions
// ----------------------------

#if defined HAVE_CG && HAVE_CG
CGcontext g_cgcontext;
CGprofile g_cgvProf;
CGprofile g_cgfProf;
#endif

RasterFont* s_pfont = NULL;

static bool s_bLastFrameDumped = false;
#ifdef _WIN32
static bool s_bAVIDumping = false;
#else
static FILE* f_pFrameDump;
#endif

// 1 for no MSAA. Use s_MSAASamples > 1 to check for MSAA.
static int s_MSAASamples = 1;
static int s_MSAACoverageSamples = 0;

bool s_bHaveFramebufferBlit = false; // export to FramebufferManager.cpp
static bool s_bHaveCoverageMSAA = false;

// The custom resolution

static bool s_skipSwap = false;

// TODO: EmuWindow has these too, merge them
int OSDChoice = 0 , OSDTime = 0, OSDInternalW = 0, OSDInternalH = 0;

#if defined(HAVE_WX) && HAVE_WX
// Screenshot thread struct
typedef struct
{
	int W, H;
	std::string filename;
	wxImage *img;
} ScrStrct;
#endif

static const GLenum glSrcFactors[8] =
{
	GL_ZERO,
	GL_ONE,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA
};

static const GLenum glDestFactors[8] = {
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA
};

static const GLenum glCmpFuncs[8] = {
	GL_NEVER,
	GL_LESS,
	GL_EQUAL,
	GL_LEQUAL,
	GL_GREATER,
	GL_NOTEQUAL,
	GL_GEQUAL,
	GL_ALWAYS
};

static const GLenum glLogicOpCodes[16] = {
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

#if defined HAVE_CG && HAVE_CG
void HandleCgError(CGcontext ctx, CGerror err, void* appdata)
{
	DEBUG_LOG(VIDEO, "Cg error: %s", cgGetErrorString(err));
	const char* listing = cgGetLastListing(g_cgcontext);
	if (listing != NULL)
		DEBUG_LOG(VIDEO, "    last listing: %s", listing);
}
#endif

// Init functions
Renderer::Renderer()
{
	// hmm
	if (!OpenGL_Create(g_VideoInitialize, 640, 480))
	{
		g_VideoInitialize.pLog("Renderer::Create failed\n", TRUE);
		return;
	}

	OpenGL_MakeCurrent();

	//if (!Renderer::Init()) {
	//	g_VideoInitialize.pLog("Renderer::Create failed\n", TRUE);
	//	PanicAlert("Can't create opengl renderer. You might be missing some required opengl extensions, check the logs for more info");
	//	exit(1);
	//}

	bool bSuccess = true;
	s_MSAACoverageSamples = 0;
	GLint numvertexattribs = 0;

	switch (g_ActiveConfig.iMultisampleMode)
	{
	case MULTISAMPLE_OFF:
		s_MSAASamples = 1;
		break;

	case MULTISAMPLE_2X:
		s_MSAASamples = 2;
		break;

	case MULTISAMPLE_4X:
		s_MSAASamples = 4;
		break;

	case MULTISAMPLE_8X:
		s_MSAASamples = 8;
		break;

	case MULTISAMPLE_CSAA_8X:
		s_MSAASamples = 4; s_MSAACoverageSamples = 8;
		break;

	case MULTISAMPLE_CSAA_8XQ:
		s_MSAASamples = 8; s_MSAACoverageSamples = 8;
		break;

	case MULTISAMPLE_CSAA_16X:
		s_MSAASamples = 4; s_MSAACoverageSamples = 16;
		break;

	case MULTISAMPLE_CSAA_16XQ:
		s_MSAASamples = 8; s_MSAACoverageSamples = 16;
		break;

	default:
		s_MSAASamples = 1;
		break;
	}

#if defined HAVE_CG && HAVE_CG
	g_cgcontext = cgCreateContext();
	cgGetError();
	cgSetErrorHandler(HandleCgError, NULL);
#endif

	// Look for required extensions.
	const char *const ptoken = (const char*)glGetString(GL_EXTENSIONS);
	if (!ptoken)
	{
		PanicAlert("Your OpenGL Driver seems to be not working.\n"
				"Please make sure your drivers are up-to-date and\n"
				"that your video hardware is OpenGL 2.x compatible.");
		//return false;
		return;
	}

	INFO_LOG(VIDEO, "Supported OpenGL Extensions:");
	INFO_LOG(VIDEO, ptoken);  // write to the log file
	INFO_LOG(VIDEO, "");

	OSD::AddMessage(StringFromFormat("Video Info: %s, %s, %s",
		glGetString(GL_VENDOR),
		glGetString(GL_RENDERER),
		glGetString(GL_VERSION)).c_str(), 5000);

	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numvertexattribs);
	if (numvertexattribs < 11)
	{
		ERROR_LOG(VIDEO, "GPU: OGL ERROR: Number of attributes %d not enough.\n"
				"GPU: Does your video card support OpenGL 2.x?",
				numvertexattribs);
		bSuccess = false;
	}

	// Init extension support.
	if (glewInit() != GLEW_OK)
	{
		ERROR_LOG(VIDEO, "glewInit() failed! Does your video card support OpenGL 2.x?");
		//return false;

		return;
	}

	if (!GLEW_EXT_framebuffer_object)
	{
		ERROR_LOG(VIDEO, "GPU: ERROR: Need GL_EXT_framebufer_object for multiple render targets.\n"
				"GPU: Does your video card support OpenGL 2.x?");
		bSuccess = false;
	}

	if (!GLEW_EXT_secondary_color)
	{
		ERROR_LOG(VIDEO, "GPU: OGL ERROR: Need GL_EXT_secondary_color.\n"
				"GPU: Does your video card support OpenGL 2.x?");
		bSuccess = false;
	}

	s_bHaveFramebufferBlit = strstr(ptoken, "GL_EXT_framebuffer_blit") != NULL;
	if (!s_bHaveFramebufferBlit)
	{
		// MSAA ain't gonna work. turn it off if enabled.
		s_MSAASamples = 1;
	}

	s_bHaveCoverageMSAA = strstr(ptoken, "GL_NV_framebuffer_multisample_coverage") != NULL;
	if (!s_bHaveCoverageMSAA)
	{
		s_MSAACoverageSamples = 0;
	}

	if (!bSuccess)
		//return false;
		return;

	// Handle VSync on/off
#if defined USE_WX && USE_WX
	// TODO: FILL IN
#elif defined _WIN32
	if (WGLEW_EXT_swap_control)
		wglSwapIntervalEXT(g_ActiveConfig.bVSync ? 1 : 0);
	else
		ERROR_LOG(VIDEO, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
#elif defined(HAVE_X11) && HAVE_X11
	if (glXSwapIntervalSGI)
		glXSwapIntervalSGI(g_ActiveConfig.bVSync ? 1 : 0);
	else
		ERROR_LOG(VIDEO, "No support for SwapInterval (framerate clamped to monitor refresh rate).");
#endif

	// check the max texture width and height
	GLint max_texture_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&max_texture_size);
	if (max_texture_size < 1024)
		ERROR_LOG(VIDEO, "GL_MAX_TEXTURE_SIZE too small at %i - must be at least 1024.",
				max_texture_size);

	if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

	if (glDrawBuffers == NULL && !GLEW_ARB_draw_buffers)
		glDrawBuffers = glDrawBuffersARB;

	if (!GLEW_ARB_texture_non_power_of_two)
		WARN_LOG(VIDEO, "ARB_texture_non_power_of_two not supported.");

	// Decide frambuffer size
	FramebufferSize((int)OpenGL_GetBackbufferWidth(), (int)OpenGL_GetBackbufferHeight());

	// Because of the fixed framebuffer size we need to disable the resolution
	// options while running
	g_Config.bRunning = true;

	if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

	// Initialize the FramebufferManager
	g_framebuffer_manager = new FramebufferManager(s_backbuffer_width,
		s_backbuffer_height, s_MSAASamples, s_MSAACoverageSamples);

	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

	if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

	s_pfont = new RasterFont();

#if defined HAVE_CG && HAVE_CG
	// load the effect, find the best profiles (if any)
	if (cgGLIsProfileSupported(CG_PROFILE_ARBVP1) != CG_TRUE)
	{
		ERROR_LOG(VIDEO, "arbvp1 not supported");
		return false;
	}

	if (cgGLIsProfileSupported(CG_PROFILE_ARBFP1) != CG_TRUE)
	{
		ERROR_LOG(VIDEO, "arbfp1 not supported");
		return false;
	}

	g_cgvProf = cgGLGetLatestProfile(CG_GL_VERTEX);
	g_cgfProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);
#if CG_VERSION_NUM == 2100
	// A bug was introduced in Cg2.1's handling of very large profile option values
	// so this will not work on ATI. ATI returns MAXINT = 2147483647 (0x7fffffff)
	// which is correct in OpenGL but Cg fails to handle it properly. As a result
	// -1 is used by Cg resulting (signedness incorrect) and compilation fails.
	if (strstr((const char*)glGetString(GL_VENDOR), "ATI") == NULL)
#endif
	{
		cgGLSetOptimalOptions(g_cgvProf);
		cgGLSetOptimalOptions(g_cgfProf);
	}
#endif	// HAVE_CG

	int nenvvertparams, nenvfragparams, naddrregisters[2];
	glGetProgramivARB(GL_VERTEX_PROGRAM_ARB,
			GL_MAX_PROGRAM_ENV_PARAMETERS_ARB,
			(GLint *)&nenvvertparams);
	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
			GL_MAX_PROGRAM_ENV_PARAMETERS_ARB,
			(GLint *)&nenvfragparams);
	glGetProgramivARB(GL_VERTEX_PROGRAM_ARB,
			GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB,
			(GLint *)&naddrregisters[0]);
	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB,
			GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB,
			(GLint *)&naddrregisters[1]);
	DEBUG_LOG(VIDEO, "Max program env parameters: vert=%d, frag=%d",
			nenvvertparams, nenvfragparams);
	DEBUG_LOG(VIDEO, "Max program address register parameters: vert=%d, frag=%d",
			naddrregisters[0], naddrregisters[1]);

	if (nenvvertparams < 238)
		ERROR_LOG(VIDEO, "Not enough vertex shader environment constants!!");

#if defined HAVE_CG && HAVE_CG
	INFO_LOG(VIDEO, "Max buffer sizes: %d %d",
		cgGetProgramBufferMaxSize(g_cgvProf),
		cgGetProgramBufferMaxSize(g_cgfProf));
#ifndef _DEBUG
	cgGLSetDebugMode(GL_FALSE);
#endif
#endif

	glStencilFunc(GL_ALWAYS, 0, 0);
	glBlendFunc(GL_ONE, GL_ONE);

	glViewport(0, 0, GetTargetWidth(), GetTargetHeight()); // Reset The Current Viewport

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glShadeModel(GL_SMOOTH);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDepthFunc(GL_LEQUAL);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment

	glDisable(GL_STENCIL_TEST);
	glEnable(GL_SCISSOR_TEST);

	glScissor(0, 0, GetTargetWidth(), GetTargetHeight());
	glBlendColorEXT(0, 0, 0, 0.5f);
	glClearDepth(1.0f);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// legacy multitexturing: select texture channel only.
	glActiveTexture(GL_TEXTURE0);
	glClientActiveTexture(GL_TEXTURE0);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	UpdateActiveConfig();
	//return GL_REPORT_ERROR() == GL_NO_ERROR && bSuccess;

	return;
}

Renderer::~Renderer()
{
	g_Config.bRunning = false;
	UpdateActiveConfig();
	delete s_pfont;
	s_pfont = 0;

#if defined HAVE_CG && HAVE_CG
	if (g_cgcontext)
	{
		cgDestroyContext(g_cgcontext);
		g_cgcontext = 0;
	}
#endif

	delete g_framebuffer_manager;

//#ifdef _WIN32
//	if(s_bAVIDumping)
//		AVIDump::Stop();
//#else
//	if(f_pFrameDump != NULL)
//		fclose(f_pFrameDump);
//#endif

	OpenGL_Shutdown();
}

// For the OSD menu's live resolution change
bool Renderer::Allow2x()
{
	if (GetFrameBufferWidth() >= 1280 && GetFrameBufferHeight() >= 960)
		return true;
	else
		return false;
}

bool Renderer::AllowCustom()
{
	//if (GetCustomWidth() <= GetFrameBufferWidth() && GetCustomHeight() <= GetFrameBufferHeight())
	//	return true;
	//else
	//	return false;

	return false;
}

void Renderer::ResetAPIState()
{
	// Gets us to a reasonably sane state where it's possible to do things like
	// image copies with textured quads, etc.
	VertexShaderCache::DisableShader();
	PixelShaderCache::DisableShader();
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDepthMask(GL_FALSE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void UpdateViewport();

void Renderer::RestoreAPIState()
{
	// Gets us back into a more game-like state.

	UpdateViewport();

	if (bpmem.genMode.cullmode > 0) glEnable(GL_CULL_FACE);
	if (bpmem.zmode.testenable)     glEnable(GL_DEPTH_TEST);
	if (bpmem.zmode.updateenable)   glDepthMask(GL_TRUE);

	glEnable(GL_SCISSOR_TEST);
	SetScissorRect();
	SetColorMask();
	SetBlendMode(true);

	VertexShaderCache::SetCurrentShader(0);
	PixelShaderCache::SetCurrentShader(0);
}

void Renderer::SetColorMask()
{
	GLenum ColorMask = (bpmem.blendmode.colorupdate) ? GL_TRUE : GL_FALSE;
	GLenum AlphaMask = (bpmem.blendmode.alphaupdate) ? GL_TRUE : GL_FALSE;
	glColorMask(ColorMask,  ColorMask,  ColorMask,  AlphaMask);
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	// blend mode bit mask
	// 0 - blend enable
	// 2 - reverse subtract enable (else add)
	// 3-5 - srcRGB function
	// 6-8 - dstRGB function

	u32 newval = bpmem.blendmode.subtract << 2;

	if (bpmem.blendmode.subtract)
		newval |= 0x0049;   // enable blending src 1 dst 1
	else if (bpmem.blendmode.blendenable)
	{
		newval |= 1;    // enable blending
		newval |= bpmem.blendmode.srcfactor << 3;
		newval |= bpmem.blendmode.dstfactor << 6;
	}

	u32 changes = forceUpdate ? 0xFFFFFFFF : newval ^ s_blendMode;

	if (changes & 1)
		// blend enable change
		(newval & 1) ? glEnable(GL_BLEND) : glDisable(GL_BLEND);

	if (changes & 4)
		// subtract enable change
		glBlendEquation(newval & 4 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);

	if (changes & 0x1F8)
		// blend RGB change
		glBlendFunc(glSrcFactors[(newval >> 3) & 7], glDestFactors[(newval >> 6) & 7]);

	s_blendMode = newval;
}

u32 Renderer::AccessEFB(EFBAccessType type, int x, int y)
{
	if(!g_ActiveConfig.bEFBAccessEnable)
		return 0;

	// Get the rectangular target region covered by the EFB pixel.
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;

	TargetRectangle targetPixelRc = ConvertEFBRectangle(efbPixelRc);

	// TODO (FIX) : currently, AA path is broken/offset and doesn't return the correct pixel
	switch (type)
	{

	case PEEK_Z:
	{
		if (s_MSAASamples > 1)
		{
			// Resolve our rectangle.
			FramebufferManager::GetEFBDepthTexture(efbPixelRc);
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, FramebufferManager::GetResolvedFramebuffer());
		}

		// Sample from the center of the target region.
		int srcX = (targetPixelRc.left + targetPixelRc.right) / 2;
		int srcY = (targetPixelRc.top + targetPixelRc.bottom) / 2;

		u32 z = 0;
		glReadPixels(srcX, srcY, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, &z);
		GL_REPORT_ERRORD();

		// Scale the 32-bit value returned by glReadPixels to a 24-bit
		// value (GC uses a 24-bit Z-buffer).
		// TODO: in RE0 this value is often off by one, which causes lighting to disappear
		return z >> 8;
	}

	case POKE_Z:
		// TODO: Implement
		break;

	case PEEK_COLOR: // GXPeekARGB
	{
		// Although it may sound strange, this really is A8R8G8B8 and not RGBA or 24-bit...

		// Tested in Killer 7, the first 8bits represent the alpha value which is used to
		// determine if we're aiming at an enemy (0x80 / 0x88) or not (0x70)
		// Wind Waker is also using it for the pictograph to determine the color of each pixel

		if (s_MSAASamples > 1)
		{
			// Resolve our rectangle.
			FramebufferManager::GetEFBColorTexture(efbPixelRc);
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, FramebufferManager::GetResolvedFramebuffer());
		}

		// Sample from the center of the target region.
		int srcX = (targetPixelRc.left + targetPixelRc.right) / 2;
		int srcY = (targetPixelRc.top + targetPixelRc.bottom) / 2;

		// Read back pixel in BGRA format, then byteswap to get GameCube's ARGB Format.
		u32 color = 0;
		glReadPixels(srcX, srcY, 1, 1, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, &color);
		GL_REPORT_ERRORD();

		return color;
	}

	case POKE_COLOR:
		// TODO: Implement. One way is to draw a tiny pixel-sized rectangle at
		// the exact location. Note: EFB pokes are susceptible to Z-buffering
		// and perhaps blending.
		//WARN_LOG(VIDEOINTERFACE, "This is probably some kind of software rendering");
		break;

	}

	return 0;
}

// Function: This function handles the OpenGL glScissor() function
// ----------------------------
// Call browser: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg()
//		case 0x52 > SetScissorRect()
// ----------------------------
// bpmem.scissorTL.x, y = 342x342
// bpmem.scissorBR.x, y = 981x821
// GetTargetHeight() = the fixed ini file setting
// donkopunchstania - it appears scissorBR is the bottom right pixel inside the scissor box
// therefore the width and height are (scissorBR + 1) - scissorTL
bool Renderer::SetScissorRect()
{
	EFBRectangle rc;
	if (g_renderer->SetScissorRect(rc))
	{
		glScissor(
			(int)(rc.left * EFBxScale), // x = 0 for example
			(int)((EFB_HEIGHT - rc.bottom) * EFByScale), // y = 0 for example
			(int)((rc.right - rc.left) * EFBxScale), // width = 640 for example
			(int)((rc.bottom - rc.top) * EFByScale) // height = 480 for example
			);
		return true;
	}
	else
	{
		glScissor(0, 0, GetTargetWidth(), GetTargetHeight());
	}
	return false;
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable,
		bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	// Update the view port for clearing the picture
	TargetRectangle targetRc = ConvertEFBRectangle(rc);
	glViewport(targetRc.left, targetRc.bottom, targetRc.GetWidth(), targetRc.GetHeight());
	glScissor(targetRc.left, targetRc.bottom, targetRc.GetWidth(), targetRc.GetHeight());

	// Always set the scissor in case it was set by the game and has not been reset

	VertexShaderManager::SetViewportChanged();

	GLbitfield bits = 0;
	if (colorEnable)
	{
		bits |= GL_COLOR_BUFFER_BIT;
		glClearColor(
			((color >> 16) & 0xFF) / 255.0f,
			((color >> 8) & 0xFF) / 255.0f,
			(color & 0xFF) / 255.0f,
			((color >> 24) & 0xFF) / 255.0f
			);
	}
	if (zEnable)
	{
		bits |= GL_DEPTH_BUFFER_BIT;
		glClearDepth((z & 0xFFFFFF) / float(0xFFFFFF));
	}

	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glClear(bits);
	SetScissorRect();
}

void Renderer::PrepareXFBCopy(const TargetRectangle &dst_rect)
{
	// Update GLViewPort
	glViewport(dst_rect.left, dst_rect.bottom, dst_rect.GetWidth(), dst_rect.GetHeight());
	
	GL_REPORT_ERRORD();

	// Copy the framebuffer to screen.

	// Texture map s_xfbTexture onto the main buffer
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	// Use linear filtering.
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void Renderer::Draw(const XFBSourceBase* xfbSource, const TargetRectangle& sourceRc,
		const MathUtil::Rectangle<float>& drawRc, const EFBRectangle& rc)
{
	// testing
	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if (xfbSource)
	{
		// Texture map xfbSource->texture onto the main buffer
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, ((XFBSource*)xfbSource)->texture);
	}
	else
	{
		// Render to the real buffer now.
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); // switch to the window backbuffer
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, FramebufferManager::ResolveAndGetRenderTarget(rc));
	}

	// We must call ApplyShader here even if no post proc is selected - it takes
	// care of disabling it in that case. It returns false in case of no post processing.
	bool applyShader = PostProcessing::ApplyShader();
	if (applyShader)
	{
		glBegin(GL_QUADS);
			glTexCoord2f(sourceRc.left, sourceRc.bottom);
			glMultiTexCoord2fARB(GL_TEXTURE1, 0, 0);
			glVertex2f(drawRc.left, drawRc.bottom);

			glTexCoord2f(sourceRc.left, sourceRc.top);
			glMultiTexCoord2fARB(GL_TEXTURE1, 0, 1);
			glVertex2f(drawRc.left, drawRc.top);

			glTexCoord2f(sourceRc.right, sourceRc.top);
			glMultiTexCoord2fARB(GL_TEXTURE1, 1, 1);
			glVertex2f(drawRc.right, drawRc.top);

			glTexCoord2f(sourceRc.right, sourceRc.bottom);
			glMultiTexCoord2fARB(GL_TEXTURE1, 1, 0);
			glVertex2f(drawRc.right, drawRc.bottom);
		glEnd();
		PixelShaderCache::DisableShader();
	}
	else
	{
		glBegin(GL_QUADS);
			glTexCoord2f(sourceRc.left, sourceRc.bottom);
			glVertex2f(drawRc.left, drawRc.bottom);

			glTexCoord2f(sourceRc.left, sourceRc.top);
			glVertex2f(drawRc.left, drawRc.top);

			glTexCoord2f(sourceRc.right, sourceRc.top);
			glVertex2f(drawRc.right, drawRc.top);

			glTexCoord2f(sourceRc.right, sourceRc.bottom);
			glVertex2f(drawRc.right, drawRc.bottom);
		glEnd();
	}

	GL_REPORT_ERRORD();

	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	TextureCache::DisableStage(0);

	// TODO: silly place for this
	// Wireframe
	if (g_ActiveConfig.bWireFrame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void Renderer::EndFrame()
{
	// Copy the rendered frame to the real window
	OpenGL_SwapBuffers();

	GL_REPORT_ERRORD();

	// Clear framebuffer
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	GL_REPORT_ERRORD();
}

void Renderer::Present()
{
	// Render to the framebuffer.
	FramebufferManager::SetFramebuffer(0);

	GL_REPORT_ERRORD();
}

bool Renderer::CheckForResize()
{
	// TODO: temp

	OpenGL_Update(); // just updates the render window position and the backbuffer size
	return true;
}

void Renderer::GetBackBufferSize(int* w, int* h)
{
	*w = (int)OpenGL_GetBackbufferWidth();
	*h = (int)OpenGL_GetBackbufferHeight();
}

void Renderer::RecreateFramebufferManger()
{
	delete g_framebuffer_manager;
	g_framebuffer_manager = new FramebufferManager(s_backbuffer_width,
		s_backbuffer_height, s_MSAASamples, s_MSAACoverageSamples);

	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
}

void Renderer::BeginFrame()
{
	// TODO: silly place for this
	g_Config.iSaveTargetId = 0;

	bool last_copy_efb_to_Texture = g_ActiveConfig.bCopyEFBToTexture;
	UpdateActiveConfig();
	if (last_copy_efb_to_Texture != g_ActiveConfig.bCopyEFBToTexture)
		TextureCache::ClearRenderTargets();
}

// Called from VertexShaderManager
void Renderer::UpdateViewport()
{
	// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
	// [0] = width/2
	// [1] = height/2
	// [2] = 16777215 * (farz - nearz)
	// [3] = xorig + width/2 + 342
	// [4] = yorig + height/2 + 342
	// [5] = 16777215 * farz
	float scissorXOff = float(bpmem.scissorOffset.x) * 2.0f;   // 342
	float scissorYOff = float(bpmem.scissorOffset.y) * 2.0f;   // 342

	// Stretch picture with increased internal resolution
	int GLx = (int)ceil((xfregs.rawViewport[3] - xfregs.rawViewport[0] - scissorXOff) *
			EFBxScale);
	int GLy = (int)ceil(
			(float(EFB_HEIGHT) - xfregs.rawViewport[4] + xfregs.rawViewport[1] + scissorYOff) *
			EFByScale);
	int GLWidth = (int)ceil(2.0f * xfregs.rawViewport[0] * EFBxScale);
	int GLHeight = (int)ceil(-2.0f * xfregs.rawViewport[1] * EFByScale);
	double GLNear = (xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777216.0f;
	double GLFar = xfregs.rawViewport[5] / 16777216.0f;
	if(GLWidth < 0)
	{
		GLx += GLWidth;
		GLWidth*=-1;
	}
	if(GLHeight < 0)
	{
		GLy += GLHeight;
		GLHeight *= -1;
	}
	// Update the view port
	glViewport(GLx, GLy, GLWidth, GLHeight);
	glDepthRange(GLNear, GLFar);
}

void Renderer::SetGenerationMode()
{
	// none, ccw, cw, ccw
	if (bpmem.genMode.cullmode > 0)
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(bpmem.genMode.cullmode == 2 ? GL_CCW : GL_CW);
	}
	else
		glDisable(GL_CULL_FACE);
}

void Renderer::SetDepthMode()
{
	if (bpmem.zmode.testenable)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthMask(bpmem.zmode.updateenable ? GL_TRUE : GL_FALSE);
		glDepthFunc(glCmpFuncs[bpmem.zmode.func]);
	}
	else
	{
		// if the test is disabled write is disabled too
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
}

void Renderer::SetLogicOpMode()
{
	if (bpmem.blendmode.logicopenable && bpmem.blendmode.logicmode != 3)
	{
		glEnable(GL_COLOR_LOGIC_OP);
		glLogicOp(glLogicOpCodes[bpmem.blendmode.logicmode]);
	}
	else
		glDisable(GL_COLOR_LOGIC_OP);
}

void Renderer::SetDitherMode()
{
	if (bpmem.blendmode.dither)
		glEnable(GL_DITHER);
	else
		glDisable(GL_DITHER);
}

void Renderer::SetLineWidth()
{
	float fratio = xfregs.rawViewport[0] != 0 ?
		((float)GetTargetWidth() / EFB_WIDTH) : 1.0f;
	if (bpmem.lineptwidth.linesize > 0)
		// scale by ratio of widths
		glLineWidth((float)bpmem.lineptwidth.linesize * fratio / 6.0f);
	if (bpmem.lineptwidth.pointsize > 0)
		glPointSize((float)bpmem.lineptwidth.pointsize * fratio / 6.0f);
}

}
