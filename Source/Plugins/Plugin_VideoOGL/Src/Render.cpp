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


#include "Globals.h"
#include "Thread.h"

#include <vector>
#include <cmath>
#include <cstdio>

#include "GLUtil.h"

#include <Cg/cg.h>
#include <Cg/cgGL.h>

#ifdef _WIN32
#include <mmsystem.h>
#endif

//#include "GlobalControl.h"
#include "CommonPaths.h"
#include "VideoConfig.h"
#include "Profiler.h"
#include "Statistics.h"
#include "ImageWrite.h"
#include "Render.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "TextureMngr.h"
#include "RasterFont.h"
#include "VertexShaderGen.h"
#include "DLCache.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "VertexShaderManager.h"
#include "VertexLoaderManager.h"
#include "VertexLoader.h"
#include "PostProcessing.h"
#include "TextureConverter.h"
#include "XFB.h"
#include "OnScreenDisplay.h"
#include "Timer.h"
#include "StringUtil.h"
#include "FramebufferManager.h"
#include "Fifo.h"

#include "main.h" // Local
#ifdef _WIN32
#include "OS/Win32.h"
#include "AVIDump.h"
#endif

#if defined(HAVE_WX) && HAVE_WX
#include <wx/image.h>
#endif

// Declarations and definitions
// ----------------------------
CGcontext g_cgcontext;
CGprofile g_cgvProf;
CGprofile g_cgfProf;

RasterFont* s_pfont = NULL;

static bool s_bFullscreen = false;

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
static u32 s_blendMode;

static volatile bool s_bScreenshot = false;
static Common::Thread *scrshotThread = 0;
static Common::CriticalSection s_criticalScreenshot;
static std::string s_sScreenshotName;

int frameCount;

// The custom resolution
// TODO: Add functionality to reinit all the render targets when the window is resized.
static int m_CustomWidth;
static int m_CustomHeight;
// The framebuffer size
static int m_FrameBufferWidth;
static int m_FrameBufferHeight;

static GLuint s_tempScreenshotFramebuffer = 0;

static bool s_skipSwap = false;

#ifndef _WIN32
int OSDChoice = 0 , OSDTime = 0, OSDInternalW = 0, OSDInternalH = 0;
#endif

namespace {

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
	GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS
};

static const GLenum glLogicOpCodes[16] = {
    GL_CLEAR, GL_AND, GL_AND_REVERSE, GL_COPY, GL_AND_INVERTED, GL_NOOP, GL_XOR, 
	GL_OR, GL_NOR, GL_EQUIV, GL_INVERT, GL_OR_REVERSE, GL_COPY_INVERTED, GL_OR_INVERTED, GL_NAND, GL_SET
};

void SetDefaultRectTexParams()
{
	// Set some standard texture filter modes.
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (glGetError() != GL_NO_ERROR) {
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
        GL_REPORT_ERROR();
    }
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void HandleCgError(CGcontext ctx, CGerror err, void* appdata)
{
	DEBUG_LOG(VIDEO, "Cg error: %s", cgGetErrorString(err));
	const char* listing = cgGetLastListing(g_cgcontext);
	if (listing != NULL) {
		DEBUG_LOG(VIDEO, "    last listing: %s", listing);
	}
}

} // namespace
void VideoConfig::UpdateProjectionHack()
{
	::UpdateProjectionHack(g_Config.iPhackvalue);
}


// Init functions
bool Renderer::Init()
{
	UpdateActiveConfig();
    bool bSuccess = true;
	s_blendMode = 0;
	s_MSAACoverageSamples = 0;
	switch (g_ActiveConfig.iMultisampleMode)
	{
	case MULTISAMPLE_OFF: s_MSAASamples = 1; break;
	case MULTISAMPLE_2X:  s_MSAASamples = 2; break;
	case MULTISAMPLE_4X:  s_MSAASamples = 4; break;
	case MULTISAMPLE_8X:  s_MSAASamples = 8; break;
	case MULTISAMPLE_CSAA_8X:   s_MSAASamples = 4; s_MSAACoverageSamples = 8; break;
	case MULTISAMPLE_CSAA_8XQ:  s_MSAASamples = 8; s_MSAACoverageSamples = 8; break;
	case MULTISAMPLE_CSAA_16X:  s_MSAASamples = 4; s_MSAACoverageSamples = 16; break;
	case MULTISAMPLE_CSAA_16XQ: s_MSAASamples = 8; s_MSAACoverageSamples = 16; break;
	default:
		s_MSAASamples = 1;
	}
	GLint numvertexattribs = 0;
    g_cgcontext = cgCreateContext();

    cgGetError();
	cgSetErrorHandler(HandleCgError, NULL);

    // Look for required extensions.
    const char *ptoken = (const char*)glGetString(GL_EXTENSIONS);
	if (!ptoken)
	{
		PanicAlert("Your OpenGL Driver seems to be not working.\n"
			       "Please make sure your drivers are up-to-date and\n"
				   "that your video hardware is OpenGL 2.x compatible "
					);
		return false;
	}

    INFO_LOG(VIDEO, "Supported OpenGL Extensions:");
    INFO_LOG(VIDEO, ptoken);  // write to the log file
    INFO_LOG(VIDEO, "");

	OSD::AddMessage(StringFromFormat("Video Info: %s, %s, %s", (const char*)glGetString(GL_VENDOR), 
															 (const char*)glGetString(GL_RENDERER), 
															 (const char*)glGetString(GL_VERSION)).c_str(), 5000);

    s_bFullscreen = g_ActiveConfig.bFullscreen;

	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numvertexattribs);
    if (numvertexattribs < 11) {
        ERROR_LOG(VIDEO, "*********\nGPU: OGL ERROR: Number of attributes %d not enough\nGPU: *********Does your video card support OpenGL 2.x?", numvertexattribs);
        bSuccess = false;
    }

	// Init extension support.
	if (glewInit() != GLEW_OK) {
        ERROR_LOG(VIDEO, "glewInit() failed!Does your video card support OpenGL 2.x?");
        return false;
    }
    if (!GLEW_EXT_framebuffer_object) {
        ERROR_LOG(VIDEO, "*********\nGPU: ERROR: Need GL_EXT_framebufer_object for multiple render targets\nGPU: *********Does your video card support OpenGL 2.x?");
        bSuccess = false;
    }
    if (!GLEW_EXT_secondary_color) {
        ERROR_LOG(VIDEO, "*********\nGPU: OGL ERROR: Need GL_EXT_secondary_color\nGPU: *********Does your video card support OpenGL 2.x?");
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
        return false;

	// Handle VSync on/off
#if defined USE_WX && USE_WX
	// TODO: FILL IN
#elif defined _WIN32
	if (WGLEW_EXT_swap_control)
		wglSwapIntervalEXT(g_ActiveConfig.bVSync ? 1 : 0);
	else
		ERROR_LOG(VIDEO, "no support for SwapInterval (framerate clamped to monitor refresh rate)Does your video card support OpenGL 2.x?");
#elif defined(HAVE_X11) && HAVE_X11
	if (glXSwapIntervalSGI)
		glXSwapIntervalSGI(g_ActiveConfig.bVSync ? 1 : 0);
	else
		ERROR_LOG(VIDEO, "no support for SwapInterval (framerate clamped to monitor refresh rate)");
#endif

    // check the max texture width and height
	GLint max_texture_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&max_texture_size);
	if (max_texture_size < 1024) {
		ERROR_LOG(VIDEO, "GL_MAX_TEXTURE_SIZE too small at %i - must be at least 1024", max_texture_size);
	}

	if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

    if (glDrawBuffers == NULL && !GLEW_ARB_draw_buffers)
        glDrawBuffers = glDrawBuffersARB;
	
	if (!GLEW_ARB_texture_non_power_of_two) {
		WARN_LOG(VIDEO, "ARB_texture_non_power_of_two not supported. This extension is not yet used, though.");
	}

	// Decide frambuffer size
	int W = (int)OpenGL_GetBackbufferWidth(), H = (int)OpenGL_GetBackbufferHeight();
	
	if (g_ActiveConfig.b2xResolution)
	{
		m_FrameBufferWidth = (2 * EFB_HEIGHT >= W) ? 2 * EFB_HEIGHT : W;
		m_FrameBufferHeight = (2 * EFB_HEIGHT >= H) ? 2 * EFB_HEIGHT : H;
	}
	else
	{
		// The size of the framebuffer targets should really NOT be the size of the OpenGL viewport.
		// The EFB is larger than 640x480 - in fact, it's 640x528, give or take a couple of lines.
		m_FrameBufferWidth = (EFB_WIDTH >= W) ? EFB_WIDTH : W;
		m_FrameBufferHeight = (480 >= H) ? 480 : H;

		// Adjust all heights with this ratio, the resulting height will be the same as H or EFB_HEIGHT. I.e.
		// 768 (-1) for 1024x768 etc.
		m_FrameBufferHeight *= (float)EFB_HEIGHT / 480.0;

		// Ensure a minimum target size so that the native res target always fits
		if (m_FrameBufferWidth < EFB_WIDTH) m_FrameBufferWidth = EFB_WIDTH;
		if (m_FrameBufferHeight < EFB_HEIGHT) m_FrameBufferHeight = EFB_HEIGHT;
	}

	// Save the custom resolution
	m_CustomWidth = (int)OpenGL_GetBackbufferWidth();
	m_CustomHeight = (int)OpenGL_GetBackbufferHeight();

	// Because of the fixed framebuffer size we need to disable the resolution options while running
	g_Config.bRunning = true;

    if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

	// Initialize the FramebufferManager
	g_framebufferManager.Init(m_FrameBufferWidth, m_FrameBufferHeight, s_MSAASamples, s_MSAACoverageSamples);

	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

    if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

    s_pfont = new RasterFont();

    // load the effect, find the best profiles (if any)
    if (cgGLIsProfileSupported(CG_PROFILE_ARBVP1) != CG_TRUE) {
        ERROR_LOG(VIDEO, "arbvp1 not supported");
        return false;
    }

    if (cgGLIsProfileSupported(CG_PROFILE_ARBFP1) != CG_TRUE) {
        ERROR_LOG(VIDEO, "arbfp1 not supported");
        return false;
    }

    g_cgvProf = cgGLGetLatestProfile(CG_GL_VERTEX);
    g_cgfProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);
    cgGLSetOptimalOptions(g_cgvProf);
    cgGLSetOptimalOptions(g_cgfProf);

    INFO_LOG(VIDEO, "Max buffer sizes: %d %d", cgGetProgramBufferMaxSize(g_cgvProf), cgGetProgramBufferMaxSize(g_cgfProf));
    int nenvvertparams, nenvfragparams, naddrregisters[2];
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB,   GL_MAX_PROGRAM_ENV_PARAMETERS_ARB,    (GLint *)&nenvvertparams);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB,    (GLint *)&nenvfragparams);
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB,   GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB, (GLint *)&naddrregisters[0]);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB, (GLint *)&naddrregisters[1]);
    DEBUG_LOG(VIDEO, "Max program env parameters: vert=%d, frag=%d", nenvvertparams, nenvfragparams);
    DEBUG_LOG(VIDEO, "Max program address register parameters: vert=%d, frag=%d", naddrregisters[0], naddrregisters[1]);

	if (nenvvertparams < 238)
        ERROR_LOG(VIDEO, "Not enough vertex shader environment constants!!");

#ifndef _DEBUG
    cgGLSetDebugMode(GL_FALSE);
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
    return glGetError() == GL_NO_ERROR && bSuccess;
}

void Renderer::Shutdown(void)
{    
	g_Config.bRunning = false;
	UpdateActiveConfig();
    delete s_pfont;
	s_pfont = 0;

    if (g_cgcontext) {
        cgDestroyContext(g_cgcontext);
        g_cgcontext = 0;
	}

	glDeleteFramebuffersEXT(1, &s_tempScreenshotFramebuffer);
	s_tempScreenshotFramebuffer = 0;

	g_framebufferManager.Shutdown();

#ifdef _WIN32
	if(s_bAVIDumping) {
		AVIDump::Stop();
	}
#else
	if(f_pFrameDump != NULL) {
		fclose(f_pFrameDump);
	}
#endif
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
	if (GetCustomWidth() <= GetFrameBufferWidth() && GetCustomHeight() <= GetFrameBufferHeight())
		return true;
	else
		return false;
}

// Return the framebuffer size
int Renderer::GetFrameBufferWidth()
{
	return m_FrameBufferWidth;
}
int Renderer::GetFrameBufferHeight()
{
	return m_FrameBufferHeight;
}
// Return the custom resolution
int Renderer::GetCustomWidth()
{
	return m_CustomWidth;
}
int Renderer::GetCustomHeight()
{
	return m_CustomHeight;
}
// Return the rendering target width and height
int Renderer::GetTargetWidth()
{
	return (g_ActiveConfig.bNativeResolution || g_ActiveConfig.b2xResolution) ?
		(g_ActiveConfig.bNativeResolution ? EFB_WIDTH : EFB_WIDTH * 2) : m_CustomWidth;
}
int Renderer::GetTargetHeight()
{
	return (g_ActiveConfig.bNativeResolution || g_ActiveConfig.b2xResolution) ?
		(g_ActiveConfig.bNativeResolution ? EFB_HEIGHT : EFB_HEIGHT * 2) : m_CustomHeight;
}
float Renderer::GetTargetScaleX()
{
    return (float)GetTargetWidth() / (float)EFB_WIDTH;
}

float Renderer::GetTargetScaleY()
{
    return (float)GetTargetHeight() / (float)EFB_HEIGHT;
}

TargetRectangle Renderer::ConvertEFBRectangle(const EFBRectangle& rc)
{
	return g_framebufferManager.ConvertEFBRectangle(rc);
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

void Renderer::ReinitView()
{

}

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

	VertexShaderCache::EnableShader(0);
	PixelShaderCache::EnableShader(0);	
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

    if (bpmem.blendmode.subtract) {
        newval |= 0x0049;   // enable blending src 1 dst 1
    } else if (bpmem.blendmode.blendenable) {		
		newval |= 1;    // enable blending
		newval |= bpmem.blendmode.srcfactor << 3;
		newval |= bpmem.blendmode.dstfactor << 6;
	}
	
	u32 changes = forceUpdate ? 0xFFFFFFFF : newval ^ s_blendMode;

	if (changes & 1) {
        // blend enable change
		(newval & 1) ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
	}

	if (changes & 4) {		
		// subtract enable change
		glBlendEquation(newval & 4 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
	}

	if (changes & 0x1F8) {
		// blend RGB change
		glBlendFunc(glSrcFactors[(newval >> 3) & 7], glDestFactors[(newval >> 6) & 7]);
	}

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

	TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

	// TODO (FIX) : currently, AA path is broken/offset and doesn't return the correct pixel
	switch (type)
	{

	case PEEK_Z:
	{
		if (s_MSAASamples > 1)
		{
			// Resolve our rectangle.
			g_framebufferManager.GetEFBDepthTexture(efbPixelRc);
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, g_framebufferManager.GetResolvedFramebuffer());
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
			g_framebufferManager.GetEFBColorTexture(efbPixelRc);
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, g_framebufferManager.GetResolvedFramebuffer());
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
// Renderer::GetTargetHeight() = the fixed ini file setting
// donkopunchstania - it appears scissorBR is the bottom right pixel inside the scissor box
// therefore the width and height are (scissorBR + 1) - scissorTL
bool Renderer::SetScissorRect()
{
    int xoff = bpmem.scissorOffset.x * 2 - 342;
    int yoff = bpmem.scissorOffset.y * 2 - 342;
    float MValueX = GetTargetScaleX();
    float MValueY = GetTargetScaleY();
	float rc_left = (float)bpmem.scissorTL.x - xoff - 342; // left = 0
	rc_left *= MValueX;
	if (rc_left < 0) rc_left = 0;

	float rc_top = (float)bpmem.scissorTL.y - yoff - 342; // right = 0
	rc_top *= MValueY;
	if (rc_top < 0) rc_top = 0;
    
	float rc_right = (float)bpmem.scissorBR.x - xoff - 341; // right = 640
	rc_right *= MValueX;
	if (rc_right > EFB_WIDTH * MValueX) rc_right = EFB_WIDTH * MValueX;

	float rc_bottom = (float)bpmem.scissorBR.y - yoff - 341; // bottom = 480
	rc_bottom *= MValueY;
	if (rc_bottom > EFB_HEIGHT * MValueY) rc_bottom = EFB_HEIGHT * MValueY;

   if(rc_left > rc_right)
	{
		int temp = rc_right;
		rc_right = rc_left;
		rc_left = temp;
	}
	if(rc_top > rc_bottom)
	{
		int temp = rc_bottom;
		rc_bottom = rc_top;
		rc_top = temp;
	}

	// Check that the coordinates are good
    if (rc_right >= rc_left && rc_bottom >= rc_top)
	{
        glScissor(
			(int)rc_left, // x = 0 for example
			Renderer::GetTargetHeight() - (int)(rc_bottom), // y = 0 for example
			(int)(rc_right - rc_left), // width = 640 for example
			(int)(rc_bottom - rc_top) // height = 480 for example
			); 
        return true;
    }

    return false;
}

void Renderer::ClearScreen(const EFBRectangle& rc, bool colorEnable, bool alphaEnable, bool zEnable, u32 color, u32 z)
{
	// Update the view port for clearing the picture
	glViewport(0, 0, Renderer::GetTargetWidth(), Renderer::GetTargetHeight());

	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(rc);

    // Always set the scissor in case it was set by the game and has not been reset
	glScissor(targetRc.left, targetRc.bottom, targetRc.GetWidth(), targetRc.GetHeight());

    VertexShaderManager::SetViewportChanged();

	GLbitfield bits = 0;
	if (colorEnable)
	{
		bits |= GL_COLOR_BUFFER_BIT;
		glClearColor(
			((color >> 16) & 0xFF) / 255.0f,
			((color >> 8) & 0xFF) / 255.0f,
			(color & 0xFF) / 255.0f,
			(alphaEnable ? ((color >> 24) & 0xFF) / 255.0f : 1.0f)
			);
	}
	if (zEnable)
	{
		bits |= GL_DEPTH_BUFFER_BIT;
		glClearDepth((z & 0xFFFFFF) / float(0xFFFFFF));
	}

	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glClear(bits);
}

void Renderer::RenderToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	s_skipSwap = g_bSkipCurrentFrame;

	// If we're about to write to a requested XFB, make sure the previous
	// contents make it to the screen first.
	VideoFifo_CheckSwapRequestAt(xfbAddr, fbWidth, fbHeight);
	g_framebufferManager.CopyToXFB(xfbAddr, fbWidth, fbHeight, sourceRc);

	// XXX: Without the VI, how would we know what kind of field this is? So
	// just use progressive.
	if (!g_ActiveConfig.bUseXFB)
	{
		// TODO: Find better name for this because I don't know if it means what it says.
		g_VideoInitialize.pCopiedToXFB(false);

		Renderer::Swap(xfbAddr, FIELD_PROGRESSIVE, fbWidth, fbHeight);
	}
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{
	if (s_skipSwap)
		return;
	const XFBSource* xfbSource = g_framebufferManager.GetXFBSource(xfbAddr, fbWidth, fbHeight);
	if (!xfbSource)
	{
		WARN_LOG(VIDEO, "Failed to get video for this frame");
		return;
	}

    OpenGL_Update(); // just updates the render window position and the backbuffer size
    DVSTARTPROFILE();

    ResetAPIState();

	TargetRectangle back_rc;
	ComputeDrawRectangle(OpenGL_GetBackbufferWidth(), OpenGL_GetBackbufferHeight(), true, &back_rc);

	TargetRectangle sourceRc;

	if (g_ActiveConfig.bAutoScale || g_ActiveConfig.bUseXFB)
	{
		sourceRc = xfbSource->sourceRc;
	}
	else
	{
		sourceRc.left = 0;
		sourceRc.top = xfbSource->texHeight;
		sourceRc.right = xfbSource->texWidth;
		sourceRc.bottom = 0;
	}

	int yOffset = (g_ActiveConfig.bUseXFB && field == FIELD_LOWER) ? -1 : 0;
	sourceRc.top -= yOffset;
	sourceRc.bottom -= yOffset;

	// Tell the OSD Menu about the current internal resolution
	OSDInternalW = xfbSource->sourceRc.GetWidth(); OSDInternalH = xfbSource->sourceRc.GetHeight();

	// Make sure that the wireframe setting doesn't screw up the screen copy.
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// Textured triangles are necessary because of post-processing shaders

	// Disable all other stages
	for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);

	// Update GLViewPort
	glViewport(back_rc.left, back_rc.bottom, back_rc.GetWidth(), back_rc.GetHeight());

	GL_REPORT_ERRORD();

	// Copy the framebuffer to screen.

	// Render to the real buffer now.
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); // switch to the window backbuffer

	// Texture map s_xfbTexture onto the main buffer
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, xfbSource->texture);
	// Use linear filtering.
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	// We must call ApplyShader here even if no post proc is selected - it takes 
	// care of disabling it in that case. It returns false in case of no post processing.
	if (PostProcessing::ApplyShader())
	{
		glBegin(GL_QUADS);
			glTexCoord2f(sourceRc.left, sourceRc.bottom);  glMultiTexCoord2fARB(GL_TEXTURE1, 0, 0); glVertex2f(-1, -1);
			glTexCoord2f(sourceRc.left, sourceRc.top);     glMultiTexCoord2fARB(GL_TEXTURE1, 0, 1); glVertex2f(-1,  1);
			glTexCoord2f(sourceRc.right, sourceRc.top);    glMultiTexCoord2fARB(GL_TEXTURE1, 1, 1); glVertex2f( 1,  1);
			glTexCoord2f(sourceRc.right, sourceRc.bottom); glMultiTexCoord2fARB(GL_TEXTURE1, 1, 0); glVertex2f( 1, -1);
		glEnd();
		PixelShaderCache::DisableShader();;
	}
	else
	{
		glBegin(GL_QUADS);
			glTexCoord2f(sourceRc.left, sourceRc.bottom);  glVertex2f(-1, -1);
			glTexCoord2f(sourceRc.left, sourceRc.top);     glVertex2f(-1,  1);
			glTexCoord2f(sourceRc.right, sourceRc.top);    glVertex2f( 1,  1);
			glTexCoord2f(sourceRc.right, sourceRc.bottom); glVertex2f( 1, -1);
		glEnd();
	}

	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	TextureMngr::DisableStage(0);

	// Wireframe
	if (g_ActiveConfig.bWireFrame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// Save screenshot
	if (s_bScreenshot)
	{
		if (!s_tempScreenshotFramebuffer)
			glGenFramebuffersEXT(1, &s_tempScreenshotFramebuffer);

		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_tempScreenshotFramebuffer);
		glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, xfbSource->texture, 0);

		s_criticalScreenshot.Enter();
		// Save screenshot
		SaveRenderTarget(s_sScreenshotName.c_str(), xfbSource->sourceRc.GetWidth(), xfbSource->sourceRc.GetHeight(), yOffset);
		// Reset settings
		s_sScreenshotName = "";
		s_bScreenshot = false;
		s_criticalScreenshot.Leave();

		glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, g_framebufferManager.GetEFBFramebuffer());
	}

	// Frame dumps are handled a little differently in Windows
#ifdef _WIN32
	if (g_ActiveConfig.bDumpFrames)
	{
		if (!s_tempScreenshotFramebuffer)
			glGenFramebuffersEXT(1, &s_tempScreenshotFramebuffer);

		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_tempScreenshotFramebuffer);
		glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, xfbSource->texture, 0);

		s_criticalScreenshot.Enter();
		int w = xfbSource->sourceRc.GetWidth();
		int h = xfbSource->sourceRc.GetHeight();

		u8 *data = (u8 *) malloc(3 * w * h);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, Renderer::GetTargetHeight() - h + yOffset, w, h, GL_BGR, GL_UNSIGNED_BYTE, data);
		if (glGetError() == GL_NO_ERROR) 
		{
			if (!s_bLastFrameDumped) 
			{
				s_bAVIDumping = AVIDump::Start(EmuWindow::GetParentWnd(), w, h);
				if (!s_bAVIDumping) 
					OSD::AddMessage("AVIDump Start failed", 2000);
				else 
				{
					OSD::AddMessage(StringFromFormat(
						"Dumping Frames to \"%s/framedump0.avi\" (%dx%d RGB24)", FULL_FRAMES_DIR, w, h).c_str(), 2000);
				}
			}
			if (s_bAVIDumping) 
				AVIDump::AddFrame((char *) data);

			s_bLastFrameDumped = true;
		}
		else
		{
			NOTICE_LOG(VIDEO, "Error reading framebuffer");
		}
		free(data);
		s_criticalScreenshot.Leave();

		glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, g_framebufferManager.GetEFBFramebuffer());
	} 
	else 
	{
		if(s_bLastFrameDumped && s_bAVIDumping) 
		{
			AVIDump::Stop();
			s_bAVIDumping = false;
			OSD::AddMessage("Stop dumping frames to AVI", 2000);
		}
		s_bLastFrameDumped = false;
	}
#else
	if (g_ActiveConfig.bDumpFrames) {
		s_criticalScreenshot.Enter();
		char movie_file_name[255];
		int w = OpenGL_GetBackbufferWidth();
		int h = OpenGL_GetBackbufferHeight();
		u8 *data = (u8 *) malloc(3 * w * h);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, Renderer::GetTargetHeight() - h, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
		if (glGetError() == GL_NO_ERROR) {
			if (!s_bLastFrameDumped) {
				sprintf(movie_file_name, "%s/framedump.raw", FULL_FRAMES_DIR);
				f_pFrameDump = fopen(movie_file_name, "wb");
				if (f_pFrameDump == NULL) {
					PanicAlert("Error opening framedump.raw for writing.");
				} else {
					char msg [255];
					sprintf(msg, "Dumping Frames to \"%s\" (%dx%d RGB24)", movie_file_name, w, h);
					OSD::AddMessage(msg, 2000);
				}
			}
			if (f_pFrameDump != NULL) {
				FlipImageData(data, w, h);
				fwrite(data, w * 3, h, f_pFrameDump);
				fflush(f_pFrameDump);
			}
			s_bLastFrameDumped = true;
		}
		free(data);
		s_criticalScreenshot.Leave();
	} else {
		if (s_bLastFrameDumped && f_pFrameDump != NULL) {
			fclose(f_pFrameDump);
			f_pFrameDump = NULL;
		}

		s_bLastFrameDumped = false;
	}
#endif

	// Place messages on the picture, then copy it to the screen
	// ---------------------------------------------------------------------
	// Count FPS.
	// -------------
	static int fpscount = 0;
    static unsigned long lasttime;
    ++fpscount;
    if (timeGetTime() - lasttime > 1000) 
    {
        lasttime = timeGetTime();
        s_fps = fpscount - 1;
        fpscount = 0;
    }
	// ---------------------------------------------------------------------
    GL_REPORT_ERRORD();
	
	DrawDebugText();

    GL_REPORT_ERRORD();

	// Get the status of the Blend mode
	GLboolean blend_enabled = glIsEnabled(GL_BLEND);
	glDisable(GL_BLEND);
	OSD::DrawMessages();
	if (blend_enabled)
		glEnable(GL_BLEND);
	GL_REPORT_ERRORD();	
#if defined(DVPROFILE)
    if (g_bWriteProfile) { 
        //g_bWriteProfile = 0;
        static int framenum = 0;
        const int UPDATE_FRAMES = 8;
        if (++framenum >= UPDATE_FRAMES) {
            DVProfWrite("prof.txt", UPDATE_FRAMES);
            DVProfClear();
            framenum = 0;
        }
    }
#endif
   	// Copy the rendered frame to the real window
	OpenGL_SwapBuffers();
	    
	GL_REPORT_ERRORD();

	// Clear framebuffer
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	GL_REPORT_ERRORD();

	// Clean out old stuff from caches
	DLCache::ProgressiveCleanup();
    VertexShaderCache::ProgressiveCleanup();
    PixelShaderCache::ProgressiveCleanup();
    TextureMngr::ProgressiveCleanup();

    frameCount++;

    // New frame
    stats.ResetFrame();

	// Render to the framebuffer.
	g_framebufferManager.SetFramebuffer(0);

	GL_REPORT_ERRORD();

    RestoreAPIState();

	GL_REPORT_ERRORD();
    g_Config.iSaveTargetId = 0;

	bool last_copy_efb_to_ram = g_ActiveConfig.bCopyEFBToRAM;
	UpdateActiveConfig();
	if (last_copy_efb_to_ram != g_ActiveConfig.bCopyEFBToRAM)
		TextureMngr::ClearRenderTargets();

	// For testing zbuffer targets.
    // Renderer::SetZBufferRender();
    // SaveTexture("tex.tga", GL_TEXTURE_RECTANGLE_ARB, s_FakeZTarget, GetTargetWidth(), GetTargetHeight());
}

// Create On-Screen-Messages
void Renderer::DrawDebugText()
{
	// Reset viewport for drawing text
	glViewport(0, 0, OpenGL_GetBackbufferWidth(), OpenGL_GetBackbufferHeight());
	// Draw various messages on the screen, like FPS, statistics, etc.
	char debugtext_buffer[8192];
	char *p = debugtext_buffer;
	p[0] = 0;

	if (g_ActiveConfig.bShowFPS)
		p+=sprintf(p, "FPS: %d\n", s_fps);

	if (g_ActiveConfig.bShowEFBCopyRegions)
	{
		// Store Line Size
        GLfloat lSize;
		glGetFloatv(GL_LINE_WIDTH, &lSize);

		// Set Line Size
		glLineWidth(3.0f); 

		glBegin(GL_LINES);

		// Draw EFB copy regions rectangles
		for (std::vector<EFBRectangle>::const_iterator it = stats.efb_regions.begin(); it != stats.efb_regions.end(); ++it)
		{
			GLfloat halfWidth = EFB_WIDTH / 2.0f;
            GLfloat halfHeight = EFB_HEIGHT / 2.0f;
            GLfloat x =  (GLfloat) -1.0f + ((GLfloat)it->left / halfWidth);
			GLfloat y =  (GLfloat) 1.0f - ((GLfloat)it->top / halfHeight);
			GLfloat x2 = (GLfloat) -1.0f + ((GLfloat)it->right / halfWidth);
			GLfloat y2 = (GLfloat) 1.0f - ((GLfloat)it->bottom / halfHeight);

			// Draw shadow of rect
			glColor3f(0.0f, 0.0f, 0.0f);
			glVertex2f(x, y - 0.01);  glVertex2f(x2, y - 0.01);
			glVertex2f(x, y2 - 0.01); glVertex2f(x2, y2 - 0.01);
			glVertex2f(x + 0.005, y);  glVertex2f(x + 0.005, y2);
			glVertex2f(x2 + 0.005, y); glVertex2f(x2 + 0.005, y2);

			// Draw rect
			glColor3f(0.0f, 1.0f, 1.0f);
			glVertex2f(x, y);  glVertex2f(x2, y);
			glVertex2f(x, y2); glVertex2f(x2, y2);
			glVertex2f(x, y);  glVertex2f(x, y2);
			glVertex2f(x2, y); glVertex2f(x2, y2);
		}

		glEnd();

		// Restore Line Size
		glLineWidth(lSize);

		// Clear stored regions
		stats.efb_regions.clear();
	}

    if (g_ActiveConfig.bOverlayStats) 
	{
		p = Statistics::ToString(p);
    }

	if (g_ActiveConfig.bOverlayProjStats)
	{
		p = Statistics::ToStringProj(p);
	}

	// Render a shadow, and then the text.
	if (p != debugtext_buffer)
	{
		Renderer::RenderText(debugtext_buffer, 21, 21, 0xDD000000);
		Renderer::RenderText(debugtext_buffer, 20, 20, 0xFF00FFFF);
	}

	// OSD Menu messages
	if (g_ActiveConfig.bOSDHotKey)
	{
		if (OSDChoice > 0)
		{
			OSDTime = timeGetTime() + 3000;
			OSDChoice = -OSDChoice;
		}
		if ((u32)OSDTime > timeGetTime())
		{
			std::string T1 = "", T2 = "";
			std::vector<std::string> T0;

			int W, H;
			sscanf(g_ActiveConfig.cInternalRes, "%dx%d", &W, &H);

			std::string OSDM1 =
				g_ActiveConfig.bNativeResolution || g_ActiveConfig.b2xResolution ?
				(g_ActiveConfig.bNativeResolution ? 
				StringFromFormat("%i x %i (native)", OSDInternalW, OSDInternalH)
				: StringFromFormat("%i x %i (2x)", OSDInternalW, OSDInternalH))
				: StringFromFormat("%i x %i (custom)", W, H);
			std::string OSDM21 =
				!(g_ActiveConfig.bKeepAR43 || g_ActiveConfig.bKeepAR169) ? "-": (g_ActiveConfig.bKeepAR43 ? "4:3" : "16:9");
			std::string OSDM22 =
				g_ActiveConfig.bCrop ? " (crop)" : "";			
			std::string OSDM31 =
				g_ActiveConfig.bCopyEFBToRAM ? "RAM" : "Texture";
			std::string OSDM32 =
				g_ActiveConfig.bEFBCopyDisable ? "No" : "Yes";

			// If there is more text than this we will have a collission
			if (g_ActiveConfig.bShowFPS)
				{ T1 += "\n\n"; T2 += "\n\n"; }

			// The rows
			T0.push_back(StringFromFormat("3: Internal Resolution: %s\n", OSDM1.c_str()));
			T0.push_back(StringFromFormat("4: Lock Aspect Ratio: %s%s\n", OSDM21.c_str(), OSDM22.c_str()));
			T0.push_back(StringFromFormat("5: Copy Embedded Framebuffer to %s: %s\n", OSDM31.c_str(), OSDM32.c_str()));
			T0.push_back(StringFromFormat("6: Fog: %s\n", g_ActiveConfig.bDisableFog ? "Disabled" : "Enabled"));
			T0.push_back(StringFromFormat("7: Material Lighting: %s\n", g_ActiveConfig.bDisableLighting ? "Disabled" : "Enabled"));	

			// The latest changed setting in yellow
			T1 += (OSDChoice == -1) ? T0.at(0) : "\n";
			T1 += (OSDChoice == -2) ? T0.at(1) : "\n";
			T1 += (OSDChoice == -3) ? T0.at(2) : "\n";
			T1 += (OSDChoice == -4) ? T0.at(3) : "\n";
			T1 += (OSDChoice == -5) ? T0.at(4) : "\n";		

			// The other settings in cyan
			T2 += (OSDChoice != -1) ? T0.at(0) : "\n";
			T2 += (OSDChoice != -2) ? T0.at(1) : "\n";
			T2 += (OSDChoice != -3) ? T0.at(2) : "\n";
			T2 += (OSDChoice != -4) ? T0.at(3) : "\n";
			T2 += (OSDChoice != -5) ? T0.at(4) : "\n";		

			// Render a shadow, and then the text
			Renderer::RenderText(T1.c_str(), 21, 21, 0xDD000000);
			Renderer::RenderText(T1.c_str(), 20, 20, 0xFFffff00);
			Renderer::RenderText(T2.c_str(), 21, 21, 0xDD000000);
			Renderer::RenderText(T2.c_str(), 20, 20, 0xFF00FFFF);
		}
	}
}
void Renderer::RenderText(const char* pstr, int left, int top, u32 color)
{
	int nBackbufferWidth = (int)OpenGL_GetBackbufferWidth();
	int nBackbufferHeight = (int)OpenGL_GetBackbufferHeight();
	glColor4f(((color>>16) & 0xff)/255.0f, ((color>> 8) & 0xff)/255.0f,
	          ((color>> 0) & 0xff)/255.0f, ((color>>24) & 0xFF)/255.0f);
	s_pfont->printMultilineText(pstr,
		left * 2.0f / (float)nBackbufferWidth - 1,
		1 - top * 2.0f / (float)nBackbufferHeight,
		0, nBackbufferWidth, nBackbufferHeight);
	GL_REPORT_ERRORD();
}

// Save screenshot
void Renderer::SetScreenshot(const char *filename)
{
	s_criticalScreenshot.Enter();
	s_sScreenshotName = filename;
	s_bScreenshot = true;
	s_criticalScreenshot.Leave();
}

#if defined(HAVE_WX) && HAVE_WX
THREAD_RETURN TakeScreenshot(void *pArgs)
{
	ScrStrct *threadStruct = (ScrStrct *)pArgs;
	
	// These will contain the final image size
	float FloatW = (float)threadStruct->W;
	float FloatH = (float)threadStruct->H;

	// Handle aspect ratio for the final ScrStrct to look exactly like what's on screen.
	if (g_ActiveConfig.bKeepAR43 || g_ActiveConfig.bKeepAR169)
	{
		float Ratio = (FloatW / FloatH) / (g_ActiveConfig.bKeepAR43 ? (4.0f / 3.0f) : (16.0f / 9.0f));
		
		// If ratio > 1 the picture is too wide and we have to limit the width.
		if (Ratio > 1)
			FloatW /= Ratio;
		// ratio == 1 or the image is too high, we have to limit the height.
		else
			FloatH *= Ratio;

		// This is a bit expensive on high resolutions
		threadStruct->img->Rescale((int)FloatW, (int)FloatH, wxIMAGE_QUALITY_HIGH);
	}

	// Save the screenshot and finally kill the wxImage object
	// This is really expensive when saving to PNG, but not at all when using BMP
	threadStruct->img->SaveFile(wxString::FromAscii(threadStruct->filename.c_str()), wxBITMAP_TYPE_PNG);
	threadStruct->img->Destroy();
	
	// Show success messages
	OSD::AddMessage(StringFromFormat("Saved %i x %i %s", (int)FloatW, (int)FloatH, threadStruct->filename.c_str()).c_str(), 2000);
	delete threadStruct;

	return 0;
}
#endif

bool Renderer::SaveRenderTarget(const char *filename, int W, int H, int YOffset)
{
	u8 *data = (u8 *)malloc(3 * W * H);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	glReadPixels(0, Renderer::GetTargetHeight() - H + YOffset, W, H, GL_RGB, GL_UNSIGNED_BYTE, data);

	// Show failure message
	if (glGetError() != GL_NO_ERROR)
	{
		OSD::AddMessage("Error capturing or saving screenshot.", 2000);
		return false;
	}

	// Turn image upside down
	FlipImageData(data, W, H);

#if defined(HAVE_WX) && HAVE_WX
	// Create wxImage
	wxImage *a = new wxImage(W, H, data);

	if (scrshotThread)
	{
		delete scrshotThread;
		scrshotThread = NULL;
	}

	ScrStrct *threadStruct = new ScrStrct;
	threadStruct->filename = std::string(filename);
	threadStruct->img = a;
	threadStruct->H = H; threadStruct->W = W;

	scrshotThread = new Common::Thread(TakeScreenshot, threadStruct);
#ifdef _WIN32
	scrshotThread->SetPriority(THREAD_PRIORITY_BELOW_NORMAL);
#endif
	bool result = true;

	OSD::AddMessage("Saving Screenshot... ", 2000);

#else
	bool result = SaveTGA(filename, W, H, data);
	free(data);
#endif

	return result;
}


void Renderer::FlipImageData(u8 *data, int w, int h)
{
	// Flip image upside down. Damn OpenGL.
	for (int y = 0; y < h / 2; y++)
	{
		for(int x = 0; x < w; x++)
		{
			std::swap(data[(y * w + x) * 3],     data[((h - 1 - y) * w + x) * 3]);
			std::swap(data[(y * w + x) * 3 + 1], data[((h - 1 - y) * w + x) * 3 + 1]);
			std::swap(data[(y * w + x) * 3 + 2], data[((h - 1 - y) * w + x) * 3 + 2]);
		}
	}
}

// Called from VertexShaderManager
void UpdateViewport()
{
	// reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
    // [0] = width/2
    // [1] = height/2
    // [2] = 16777215 * (farz - nearz)
    // [3] = xorig + width/2 + 342
    // [4] = yorig + height/2 + 342
    // [5] = 16777215 * farz
	int scissorXOff = bpmem.scissorOffset.x * 2;   // 342
	int scissorYOff = bpmem.scissorOffset.y * 2;   // 342

	float MValueX = Renderer::GetTargetScaleX();
	float MValueY = Renderer::GetTargetScaleY();

	// Stretch picture with increased internal resolution
	int GLx = (int)ceil((xfregs.rawViewport[3] - xfregs.rawViewport[0] - scissorXOff) * MValueX);
	int GLy = (int)ceil(Renderer::GetTargetHeight() - ((int)(xfregs.rawViewport[4] - xfregs.rawViewport[1] - scissorYOff)) * MValueY);
	int GLWidth = (int)ceil((int)(2 * xfregs.rawViewport[0]) * MValueX);
	int GLHeight = (int)ceil((int)(-2 * xfregs.rawViewport[1]) * MValueY);
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
	float fratio = xfregs.rawViewport[0] != 0 ? ((float)Renderer::GetTargetWidth() / EFB_WIDTH) : 1.0f;
	if (bpmem.lineptwidth.linesize > 0)
		glLineWidth((float)bpmem.lineptwidth.linesize * fratio / 6.0f); // scale by ratio of widths
	if (bpmem.lineptwidth.pointsize > 0)
		glPointSize((float)bpmem.lineptwidth.pointsize * fratio / 6.0f);
}

void Renderer::SetSamplerState(int stage,int texindex)
{
	// TODO
}

void Renderer::SetInterlacingMode()
{
	// TODO
}
