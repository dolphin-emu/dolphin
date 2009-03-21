// Copyright (C) 2003-2008 Dolphin Project.

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

#include <vector>
#include <cmath>

#include "GLUtil.h"

#include <Cg/cg.h>
#include <Cg/cgGL.h>

#ifdef _WIN32
#include <mmsystem.h>
#endif

#include "Config.h"
#include "Profiler.h"
#include "Statistics.h"
#include "ImageWrite.h"
#include "Render.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "TextureMngr.h"
#include "rasterfont.h"
#include "VertexShaderGen.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderCache.h"
#include "VertexShaderManager.h"
#include "VertexLoaderManager.h"
#include "VertexLoader.h"
#include "XFB.h"
#include "OnScreenDisplay.h"
#include "Timer.h"

#include "main.h" // Local
#ifdef _WIN32
#include "OS/Win32.h"
#endif

#if defined(HAVE_WX) && HAVE_WX
#include <wx/image.h>
#endif

#ifdef _WIN32
	#include "Win32Window.h" // warning: crapcode
#else
#endif

CGcontext g_cgcontext;
CGprofile g_cgvProf;
CGprofile g_cgfProf;

RasterFont* s_pfont = NULL;

static bool s_bFullscreen = false;

static int nZBufferRender = 0;  // if > 0, then use zbuffer render, and count down.

// 1 for no MSAA. Use s_MSAASamples > 1 to check for MSAA.
static int s_MSAASamples = 1;
static int s_MSAACoverageSamples = 0;

// Normal Mode
// s_RenderTarget is a texture_rect
// s_DepthTarget is a Z renderbuffer
// s_FakeZTarget is a texture_rect

// MSAA mode
// s_uFramebuffer is a FBO
// s_RenderTarget is a MSAA renderbuffer
// s_FakeZBufferTarget is a MSAA renderbuffer
// s_DepthTarget is a real MSAA z/stencilbuffer
// 
// s_ResolvedFramebuffer is a FBO
// s_ResolvedColorTarget is a texture
// s_ResolvedFakeZTarget is a texture
// s_ResolvedDepthTarget is a Z renderbuffer

// A framebuffer is a set of render targets: a color and a z buffer. They can be either RenderBuffers or Textures.
static GLuint s_uFramebuffer = 0;
static GLuint s_uResolvedFramebuffer = 0;

// The size of these should be a (not necessarily even) multiple of the EFB size, 640x528, but isn't.
// These are all texture IDs. Bind them as rect arb textures.
static GLuint s_RenderTarget = 0;
static GLuint s_FakeZTarget  = 0;
static GLuint s_DepthTarget  = 0;

static GLuint s_ResolvedRenderTarget = 0;
static GLuint s_ResolvedFakeZTarget  = 0;
static GLuint s_ResolvedDepthTarget  = 0;

static bool s_bATIDrawBuffers = false;
static bool s_bHaveStencilBuffer = false;
static bool s_bHaveFramebufferBlit = false;
static bool s_bHaveCoverageMSAA = false;
static u32 s_blendMode;
static bool s_bNativeResolution = false;

static volatile bool s_bScreenshot = false;
static Common::CriticalSection s_criticalScreenshot;
static std::string s_sScreenshotName;

static Renderer::RenderMode s_RenderMode = Renderer::RM_Normal;

int frameCount;
static int s_fps = 0;

// These STAY CONSTANT during execution, no matter how much you resize the game window.
// TODO: Add functionality to reinit all the render targets when the window is resized.
static int s_targetwidth;   // Size of render buffer FBO.
static int s_targetheight;


extern void HandleCgError(CGcontext ctx, CGerror err, void *appdata);

namespace {

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
    GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
    GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,  GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA
};

void SetDefaultRectTexParams()
{
	// Set some standard texture filter modes.
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (glGetError() != GL_NO_ERROR) {
		GLenum err;
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
        GL_REPORT_ERROR();
    }
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

}  // namespace


bool Renderer::Init()
{
    bool bSuccess = true;
	s_blendMode = 0;
	s_MSAACoverageSamples = 0;
	s_bNativeResolution = g_Config.bNativeResolution;
	switch (g_Config.iMultisampleMode)
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
    GLenum err = GL_NO_ERROR;
    g_cgcontext = cgCreateContext();

    cgGetError();
	cgSetErrorHandler(HandleCgError, NULL);

    // Look for required extensions.
    const char *ptoken = (const char*)glGetString(GL_EXTENSIONS);
	if (!ptoken)
	{
		PanicAlert("Failed to get OpenGL extension string.\nDo you have the latest video drivers installed?\nIs your video card OpenGL 2.x compatible?");
		return false;
	}
    INFO_LOG(VIDEO, "Supported OpenGL Extensions:");
    INFO_LOG(VIDEO, ptoken);  // write to the log file
    INFO_LOG(VIDEO, "");

	// Checks if it ONLY has the ATI_draw_buffers extension, some have both
    if (GLEW_ATI_draw_buffers && !GLEW_ARB_draw_buffers) 
        s_bATIDrawBuffers = true;

    s_bFullscreen = g_Config.bFullscreen;

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
#ifdef _WIN32
	if (WGLEW_EXT_swap_control)
		wglSwapIntervalEXT(g_Config.bVSync ? 1 : 0);
	else
		ERROR_LOG(VIDEO, "no support for SwapInterval (framerate clamped to monitor refresh rate)Does your video card support OpenGL 2.x?");
#elif defined(HAVE_X11) && HAVE_X11
	if (glXSwapIntervalSGI)
		glXSwapIntervalSGI(g_Config.bVSync ? 1 : 0);
	else
		ERROR_LOG(VIDEO, "no support for SwapInterval (framerate clamped to monitor refresh rate)");
#endif

    // check the max texture width and height
	GLint max_texture_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&max_texture_size);
	if (max_texture_size < 1024) {
		ERROR_LOG(VIDEO, "GL_MAX_TEXTURE_SIZE too small at %i - must be at least 1024", max_texture_size);
	}
    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR) bSuccess = false;

    if (glDrawBuffers == NULL && !GLEW_ARB_draw_buffers)
        glDrawBuffers = glDrawBuffersARB;

	// The size of the framebuffer targets should really NOT be the size of the OpenGL viewport.
	// The EFB is larger than 640x480 - in fact, it's 640x528, give or take a couple of lines.
	// So the below is wrong.
	// This should really be grabbed from config rather than from OpenGL.
	s_targetwidth = (int)OpenGL_GetBackbufferWidth();
    s_targetheight = (int)OpenGL_GetBackbufferHeight();
	
	// Compensate height of render target for scaling, so that we get something close to the correct number of
	// vertical pixels.
	s_targetheight *= 528.0 / 480.0;

	// Ensure a minimum target size so that the native res target always fits.
	if (s_targetwidth < EFB_WIDTH)
		s_targetwidth = EFB_WIDTH;
	if (s_targetheight < EFB_HEIGHT)
		s_targetheight = EFB_HEIGHT;

	glGenFramebuffersEXT(1, (GLuint *)&s_uFramebuffer);
	if (s_uFramebuffer == 0) {
		ERROR_LOG(VIDEO, "failed to create the renderbufferDoes your video card support OpenGL 2.x?");
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_uFramebuffer);

	if (s_MSAASamples <= 1) {
		// Create the framebuffer target texture
		glGenTextures(1, (GLuint *)&s_RenderTarget);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_RenderTarget);
		// Create our main color render target as a texture rectangle of the desired size.
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, s_targetwidth, s_targetheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		SetDefaultRectTexParams();
		GL_REPORT_ERROR();

		GLint nMaxMRT = 0;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &nMaxMRT);
		if (nMaxMRT > 1) 
		{
			// There's MRT support. Create a color texture image to use as secondary render target.
			// We use MRT to render Z into this one, for various purposes (mostly copy Z to texture).
			glGenTextures(1, (GLuint *)&s_FakeZTarget);
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_FakeZTarget);
			glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, s_targetwidth, s_targetheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			SetDefaultRectTexParams();
		}

		// Create the real depth/stencil buffer. It's a renderbuffer, not a texture.
		glGenRenderbuffersEXT(1, &s_DepthTarget);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_DepthTarget);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, s_targetwidth, s_targetheight);
		GL_REPORT_ERROR();
	
		// Our framebuffer object is still bound here. Attach the two render targets, color and Z/stencil, to the framebuffer object.
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, s_RenderTarget, 0);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, s_DepthTarget);
    	GL_REPORT_ERROR();

		if (s_FakeZTarget != 0) {
			// We do a simple test to make sure that MRT works. I don't really know why - this is probably a workaround for
			// some terribly buggy ancient driver.
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, s_FakeZTarget, 0);
			bool bFailed = glGetError() != GL_NO_ERROR || glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT;
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);
			if (bFailed) {
				glDeleteTextures(1, (GLuint *)&s_FakeZTarget);
				s_FakeZTarget = 0;
			}
		}

		if (s_FakeZTarget == 0)
			ERROR_LOG(VIDEO, "Disabling ztarget MRT feature (max MRT = %d)", nMaxMRT);
	}
	else
	{
		// MSAA rendertarget init.
		// First set up the boring multisampled rendertarget.
		glGenRenderbuffersEXT(1, &s_RenderTarget);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_RenderTarget);
		if (s_MSAACoverageSamples) {
			glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER_EXT, s_MSAACoverageSamples, s_MSAASamples, GL_RGBA, s_targetwidth, s_targetheight);
		} else {
			glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, s_MSAASamples, GL_RGBA, s_targetwidth, s_targetheight);
		}
		glGenRenderbuffersEXT(1, &s_FakeZTarget);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_FakeZTarget);
		if (s_MSAACoverageSamples) {
			glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER_EXT, s_MSAACoverageSamples, s_MSAASamples, GL_RGBA, s_targetwidth, s_targetheight);
		} else {
			glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, s_MSAASamples, GL_RGBA, s_targetwidth, s_targetheight);
		}
		glGenRenderbuffersEXT(1, &s_DepthTarget);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_DepthTarget);
		if (s_MSAACoverageSamples) {
			glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER_EXT, s_MSAACoverageSamples, s_MSAASamples, GL_DEPTH24_STENCIL8_EXT, s_targetwidth, s_targetheight);
		} else {
			glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, s_MSAASamples, GL_DEPTH24_STENCIL8_EXT, s_targetwidth, s_targetheight);
		}
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);

		// Attach them to our multisampled FBO. The multisampled FBO is still bound here.
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, s_RenderTarget);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_RENDERBUFFER_EXT, s_FakeZTarget);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT,  GL_RENDERBUFFER_EXT, s_DepthTarget);
		OpenGL_CheckFBOStatus();

		bool bFailed = glGetError() != GL_NO_ERROR || glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT;
		if (bFailed) PanicAlert("Incomplete rt");

		// Create our resolve FBO, and bind it.
		glGenFramebuffersEXT(1, (GLuint *)&s_uResolvedFramebuffer);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_uResolvedFramebuffer);

		// Generate the resolve targets.
		glGenTextures(1, (GLuint *)&s_ResolvedRenderTarget);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_ResolvedRenderTarget);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, s_targetwidth, s_targetheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		SetDefaultRectTexParams();
		// Generate the resolve targets.
		glGenTextures(1, (GLuint *)&s_ResolvedFakeZTarget);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_ResolvedFakeZTarget);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, s_targetwidth, s_targetheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		SetDefaultRectTexParams();
		// Create the real depth/stencil buffer. It's a renderbuffer, not a texture.
		glGenRenderbuffersEXT(1, &s_ResolvedDepthTarget);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_ResolvedDepthTarget);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, s_targetwidth, s_targetheight);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
		
		// Attach our resolve targets to our resolved FBO.
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, s_ResolvedRenderTarget, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, s_ResolvedFakeZTarget, 0);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, s_ResolvedDepthTarget);

		OpenGL_CheckFBOStatus();

		bFailed = glGetError() != GL_NO_ERROR || glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT;
		if (bFailed) PanicAlert("Incomplete rt2");
	}

	// glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_uFramebuffer);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);

    nZBufferRender = 0;  // Initialize the Z render shutoff countdown. We only render Z if it's desired, to save GPU power.

    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR)
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
    
    s_RenderMode = Renderer::RM_Normal;
    if (!InitializeGL())
        return false;

	XFB_Init();
    return glGetError() == GL_NO_ERROR && bSuccess;
}

void Renderer::Shutdown(void)
{    
    delete s_pfont;
	s_pfont = 0;

	XFB_Shutdown();

    if (g_cgcontext) {
        cgDestroyContext(g_cgcontext);
        g_cgcontext = 0;
    }
    if (s_RenderTarget) {
        glDeleteTextures(1, &s_RenderTarget);
        s_RenderTarget = 0;
    }
    if (s_DepthTarget) {
        glDeleteRenderbuffersEXT(1, &s_DepthTarget);
		s_DepthTarget = 0;
    }
    if (s_uFramebuffer) {
        glDeleteFramebuffersEXT(1, &s_uFramebuffer);
        s_uFramebuffer = 0;
    }
}

bool Renderer::InitializeGL()
{
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
    
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // perspective correct interpolation of colors and tex coords
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);    
    glHint(GL_POLYGON_SMOOTH_HINT, GL_DONT_CARE);   // Polygon smoothing is ancient junk that doesn't work anymore. MSAA is modern AA.
    
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

    GLenum err = GL_NO_ERROR;
    GL_REPORT_ERROR();

    return err == GL_NO_ERROR;
}


//////////////////////////////////////////////////////////////////////////////////////
// Return the rendering window width and height
// ------------------------
int Renderer::GetTargetWidth()
{
	return s_bNativeResolution ? EFB_WIDTH : s_targetwidth;
}

int Renderer::GetTargetHeight()
{
    return s_bNativeResolution ? EFB_HEIGHT : s_targetheight;
}

float Renderer::GetTargetScaleX()
{
    return (float)GetTargetWidth() / (float)EFB_WIDTH;
}

float Renderer::GetTargetScaleY()
{
    return (float)GetTargetHeight() / (float)EFB_HEIGHT;
}

/////////////////////////////

void Renderer::SetRenderTarget(GLuint targ)
{
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, 
		targ != 0 ? targ : s_RenderTarget, 0);
}

void Renderer::SetDepthTarget(GLuint targ)
{
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT,
		targ != 0 ? targ : s_DepthTarget);
}

void Renderer::SetFramebuffer(GLuint fb)
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb != 0 ? fb : s_uFramebuffer);
}

GLuint Renderer::ResolveAndGetRenderTarget(const TRectangle &source_rect)
{
	if (s_MSAASamples > 1)
	{
		// Flip the rectangle
		TRectangle flipped_rect;
		source_rect.FlipYPosition(GetTargetHeight(), &flipped_rect);

		flipped_rect.Clamp(0, 0, GetTargetWidth(), GetTargetHeight());
		// Do the resolve.
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_uFramebuffer);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, s_uResolvedFramebuffer);
		glBlitFramebufferEXT(flipped_rect.left, flipped_rect.top, flipped_rect.right, flipped_rect.bottom,
			                 flipped_rect.left, flipped_rect.top, flipped_rect.right, flipped_rect.bottom,
							 GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// Return the resolved target.
		return s_ResolvedRenderTarget;
	}
	else 
	{
		return s_RenderTarget;
	}
}

GLuint Renderer::ResolveAndGetFakeZTarget(const TRectangle &source_rect)
{
	// This logic should be moved elsewhere.
	if (s_MSAASamples > 1)
	{
		// Flip the rectangle	
		TRectangle flipped_rect;
		source_rect.FlipYPosition(GetTargetHeight(), &flipped_rect);

		flipped_rect.Clamp(0, 0, GetTargetWidth(), GetTargetHeight());
		// Do the resolve. We resolve both color channels, not very necessary.
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_uFramebuffer);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, s_uResolvedFramebuffer);
		glBlitFramebufferEXT(flipped_rect.left, flipped_rect.top, flipped_rect.right, flipped_rect.bottom,
			                 flipped_rect.left, flipped_rect.top, flipped_rect.right, flipped_rect.bottom,
							 GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// Return the resolved target.
		return s_ResolvedFakeZTarget;
	}
	else 
	{
		return s_FakeZTarget;
	}
}

bool Renderer::UseFakeZTarget()
{
	// This logic should be moved elsewhere.
	return nZBufferRender > 0;
}

void Renderer::ResetGLState()
{
	// Gets us to a reasonably sane state where it's possible to do things like
	// image copies with textured quads, etc.
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glDisable(GL_VERTEX_PROGRAM_ARB);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
}

void Renderer::RestoreGLState()
{
	// Gets us back into a more game-like state.
    if (bpmem.genMode.cullmode > 0) glEnable(GL_CULL_FACE);
    if (bpmem.zmode.testenable)     glEnable(GL_DEPTH_TEST);
    if (bpmem.zmode.updateenable)   glDepthMask(GL_TRUE);

    glEnable(GL_SCISSOR_TEST);
	SetScissorRect();
    SetColorMask();
	SetBlendMode(true);

	glEnable(GL_VERTEX_PROGRAM_ARB);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
}

void Renderer::SetColorMask()
{
    if (bpmem.blendmode.alphaupdate && bpmem.blendmode.colorupdate)
        glColorMask(GL_TRUE,  GL_TRUE,  GL_TRUE,  GL_TRUE);
    else if (bpmem.blendmode.alphaupdate)
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    else if (bpmem.blendmode.colorupdate) 
        glColorMask(GL_TRUE,  GL_TRUE,  GL_TRUE,  GL_FALSE);
	else
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
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

////////////////////////////////////////////////////////////////////////////////////////////
// Call browser: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg()
//		case 0x52 > SetScissorRect()
// ---------------
// This function handles the OpenGL glScissor() function
// ---------------
// bpmem.scissorTL.x, y = 342x342
// bpmem.scissorBR.x, y = 981x821
// Renderer::GetTargetHeight() = the fixed ini file setting
// donkopunchstania - it appears scissorBR is the bottom right pixel inside the scissor box
// therefore the width and height are (scissorBR + 1) - scissorTL
// ---------------
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

   /*LOG(VIDEO, "Scissor: lt=(%d,%d), rb=(%d,%d,%i), off=(%d,%d)\n",
		rc_left, rc_top,
		rc_right, rc_bottom, Renderer::GetTargetHeight(),
		xoff, yoff
		);*/

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

bool Renderer::IsUsingATIDrawBuffers()
{
    return s_bATIDrawBuffers;
}

bool Renderer::HaveStencilBuffer()
{
    return s_bHaveStencilBuffer;
}

void Renderer::SetZBufferRender()
{
    nZBufferRender = 10;  // The game asked for Z. Give it 10 frames, then turn it off for speed.
    GLenum s_drawbuffers[2] = {
		GL_COLOR_ATTACHMENT0_EXT,
		GL_COLOR_ATTACHMENT1_EXT
	};
    glDrawBuffers(2, s_drawbuffers);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, s_FakeZTarget, 0);
    _assert_(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
}


// Does this function even work correctly???
void Renderer::FlushZBufferAlphaToTarget()
{
    ResetGLState();

    SetRenderTarget(0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glViewport(0, 0, GetTargetWidth(), GetTargetHeight());

    // texture map s_RenderTargets[s_curtarget] onto the main buffer
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_FakeZTarget);
    TextureMngr::EnableTexRECT(0);
    // disable all other stages
    for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);
    GL_REPORT_ERRORD();

	// setup the stencil to only accept pixels that have been written
	glStencilFunc(GL_EQUAL, 1, 0xff);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	// TODO: This code should not have to bother with stretchtofit checking - 
	// all necessary scale initialization should be done elsewhere.
	if (s_bNativeResolution)
	{
		//TODO: Do Correctly in a bit
        float FactorW = 640.f / (float)OpenGL_GetBackbufferWidth();
		float FactorH = 480.f / (float)OpenGL_GetBackbufferHeight();

		float Max = (FactorW < FactorH) ? FactorH : FactorW;
		float Temp = 1.0f / Max;
		FactorW *= Temp;
		FactorH *= Temp;

		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2f(-FactorW,-FactorH);
		glTexCoord2f(0, (float)GetTargetHeight()); glVertex2f(-FactorW,FactorH);
		glTexCoord2f((float)GetTargetWidth(), (float)GetTargetHeight()); glVertex2f(FactorW,FactorH);
		glTexCoord2f((float)GetTargetWidth(), 0); glVertex2f(FactorW,-FactorH);
	    glEnd();
	}
	else
	{		
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2f(-1,-1);
		glTexCoord2f(0, (float)(GetTargetHeight())); glVertex2f(-1,1);
		glTexCoord2f((float)(GetTargetWidth()), (float)(GetTargetHeight())); glVertex2f(1,1);
		glTexCoord2f((float)(GetTargetWidth()), 0); glVertex2f(1,-1);
	    glEnd();
	}
    
    GL_REPORT_ERRORD();

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    RestoreGLState();
}

void Renderer::SetRenderMode(RenderMode mode)
{
    if (!s_bHaveStencilBuffer && mode == RM_ZBufferAlpha)
        mode = RM_ZBufferOnly;

    if (s_RenderMode == mode)
        return;

    if (mode == RM_Normal) {
        // flush buffers
        if (s_RenderMode == RM_ZBufferAlpha) {
            FlushZBufferAlphaToTarget();
            glDisable(GL_STENCIL_TEST);
        }
        SetColorMask();
        SetRenderTarget(0);
        SetZBufferRender();
        GL_REPORT_ERRORD();
    }
    else if (s_RenderMode == RM_Normal) {
        // setup buffers
        _assert_(UseFakeZTarget() && bpmem.zmode.updateenable);
        if (mode == RM_ZBufferAlpha) {
            glEnable(GL_STENCIL_TEST);
            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);
            glStencilFunc(GL_ALWAYS, 1, 0xff);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        }

        glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        GL_REPORT_ERRORD();
    }
    else {
        _assert_(UseFakeZTarget());
        _assert_(s_bHaveStencilBuffer);

        if (mode == RM_ZBufferOnly) {
            // flush and remove stencil
            _assert_(s_RenderMode == RM_ZBufferAlpha);
            FlushZBufferAlphaToTarget();
            glDisable(GL_STENCIL_TEST);

            SetRenderTarget(s_FakeZTarget);
            glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
            GL_REPORT_ERRORD();
        }
        else {
            _assert_(mode == RM_ZBufferAlpha && s_RenderMode == RM_ZBufferOnly);
            
            // setup stencil
            glEnable(GL_STENCIL_TEST);
            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);
            glStencilFunc(GL_ALWAYS, 1, 0xff);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        }
    }

    s_RenderMode = mode;
}

Renderer::RenderMode Renderer::GetRenderMode()
{
    return s_RenderMode;
}

void ComputeBackbufferRectangle(TRectangle *rc)
{
	float FloatGLWidth = (float)OpenGL_GetBackbufferWidth();
	float FloatGLHeight = (float)OpenGL_GetBackbufferHeight();
	float FloatXOffset = 0;
	float FloatYOffset = 0;

	// The rendering window size
	const float WinWidth = FloatGLWidth;
	const float WinHeight = FloatGLHeight;

	// Handle aspect ratio.
	if (g_Config.bKeepAR43 || g_Config.bKeepAR169)
	{
		// The rendering window aspect ratio as a proportion of the 4:3 or 16:9 ratio
		float Ratio = (WinWidth / WinHeight) / (g_Config.bKeepAR43 ? (4.0f / 3.0f) : (16.0f / 9.0f));
		// Check if height or width is the limiting factor. If ratio > 1 the picture is to wide and have to limit the width.
		if (Ratio > 1)
		{
			// Scale down and center in the X direction.
			FloatGLWidth /= Ratio;
			FloatXOffset = (WinWidth - FloatGLWidth) / 2.0f;
		}
		// The window is too high, we have to limit the height
		else
		{
			// Scale down and center in the Y direction.
			FloatGLHeight *= Ratio;
			FloatYOffset = FloatYOffset + (WinHeight - FloatGLHeight) / 2.0f;
		}
	}

	// -----------------------------------------------------------------------
	// Crop the picture from 4:3 to 5:4 or from 16:9 to 16:10.
	//		Output: FloatGLWidth, FloatGLHeight, FloatXOffset, FloatYOffset
	// ------------------
	if ((g_Config.bKeepAR43 || g_Config.bKeepAR169) && g_Config.bCrop)
	{
		float Ratio = g_Config.bKeepAR43 ? ((4.0 / 3.0) / (5.0 / 4.0)) : (((16.0 / 9.0) / (16.0 / 10.0)));
		// The width and height we will add (calculate this before FloatGLWidth and FloatGLHeight is adjusted)
		float IncreasedWidth = (Ratio - 1.0) * FloatGLWidth;
		float IncreasedHeight = (Ratio - 1.0) * FloatGLHeight;
		// The new width and height
		FloatGLWidth = FloatGLWidth * Ratio;
		FloatGLHeight = FloatGLHeight * Ratio;
		// We need this adjustment too, the -6 adjustment was needed to never show any pixels outside the actual picture
		// The result in offset in actual pixels is only around 1 or 2 pixels in a 1280 x 1024 resolution. In 1280 x 1024 the
		// picture is only about 2 to 4 pixels (1 up and 1 down for example) to big to produce a minimal margin
		// of error, while rather having a full screen and hiding one pixel, than having a one pixel black border. But it seems
		// to be just enough in all games I tried.
		float WidthRatio = ((float)FloatGLWidth - 6.0) / 640.0;
		float HeightRatio = ((float)FloatGLHeight - 6.0) / 480.0;
		// Adjust the X and Y offset
		FloatXOffset = FloatXOffset - (IncreasedWidth / 2.0 / WidthRatio / Ratio);
		FloatYOffset = FloatYOffset - (IncreasedHeight / 2.0 / HeightRatio / Ratio);
		//DEBUG_LOG(CONSOLE, "Crop       Ratio:%1.2f IncreasedHeight:%3.0f YOffset:%3.0f", Ratio, IncreasedHeight, FloatYOffset);
	}

	// round(float) = floor(float + 0.5)
	int XOffset = floor(FloatXOffset + 0.5);
	int YOffset = floor(FloatYOffset + 0.5);
	rc->left = XOffset;
	rc->top = YOffset;
	rc->right = XOffset + ceil(FloatGLWidth);
	rc->bottom = YOffset + ceil(FloatGLHeight);
}

//////////////////////////////////////////////////////////////////////////////////////
// This function has the final picture if the XFB functions are not used. We adjust the aspect ratio here.
// ----------------------
void Renderer::Swap(const TRectangle& rc)
{
    OpenGL_Update(); // just updates the render window position and the backbuffer size
    DVSTARTPROFILE();

    Renderer::SetRenderMode(Renderer::RM_Normal);

    ResetGLState();

	TRectangle back_rc;
	ComputeBackbufferRectangle(&back_rc);

	float u_max;
	float v_min = 0.f;
	float v_max;
	if (g_Config.bAutoScale)
	{
		u_max = (rc.right - rc.left);
		v_min = (float)GetTargetHeight() - (rc.bottom - rc.top);
		v_max = (float)GetTargetHeight();
	}
	else
	{
		u_max = (float)GetTargetWidth();
		v_max = (float)GetTargetHeight();
	}

	if (/*s_bHaveFramebufferBlit*/ s_MSAASamples > 1)
	{
		// Use framebuffer blit to stretch screen.
		// No messing around with annoying glBegin and viewports, plus can support multisampling.
		if (s_MSAASamples > 1)
		{
			ResolveAndGetRenderTarget(rc);
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_uResolvedFramebuffer);
		}
		else
		{
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_uFramebuffer); 
		}
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
		glBlitFramebufferEXT(0, v_min, u_max, v_max,
							 back_rc.left, back_rc.top, back_rc.right, back_rc.bottom,
							 GL_COLOR_BUFFER_BIT, GL_LINEAR);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);  // switch to the window backbuffer, we'll draw debug text on top
	}
	else
	{
		// No framebuffer_blit extension - crappy gfx card! Fall back to plain texturing solution.
		// Disable all other stages.
		for (int i = 1; i < 8; ++i)
			TextureMngr::DisableStage(i);

		// Update GLViewPort
		glViewport(back_rc.left, back_rc.top,
				   back_rc.right - back_rc.left, back_rc.bottom - back_rc.top);

		GL_REPORT_ERRORD();

		// Copy the framebuffer to screen.
		// TODO: Use glBlitFramebufferEXT.
			// Render to the real buffer now.
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); // switch to the window backbuffer

		// Texture map s_RenderTargets[s_curtarget] onto the main buffer
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_RenderTarget);
		// Use linear filtering.
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		TextureMngr::EnableTexRECT(0);
	    
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glBegin(GL_QUADS);
			glTexCoord2f(0,     v_min); glVertex2f(-1, -1);
			glTexCoord2f(0,     v_max); glVertex2f(-1,  1);
			glTexCoord2f(u_max, v_max); glVertex2f( 1,  1);
			glTexCoord2f(u_max, v_min); glVertex2f( 1, -1);
		glEnd();

		// Restore filtering.
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		// Wireframe
		if (g_Config.bWireFrame)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
		TextureMngr::DisableStage(0);
		// End of non-framebuffer_blit workaround.
	}

	// Take screenshot, if requested
	if (s_bScreenshot) {
		s_criticalScreenshot.Enter();
		if (SaveRenderTarget(s_sScreenshotName.c_str(), OpenGL_GetBackbufferWidth(), OpenGL_GetBackbufferHeight())) {
			char msg[255];
			sprintf(msg, "Saved %s\n", s_sScreenshotName.c_str());
			OSD::AddMessage(msg, 2000);
		} else {
			PanicAlert("Error capturing or saving screenshot.");
		}
		s_sScreenshotName = "";
		s_bScreenshot = false;
		s_criticalScreenshot.Leave();
	}

	// Place messages on the picture, then copy it to the screen
    SwapBuffers();
	s_bNativeResolution = g_Config.bNativeResolution;

    RestoreGLState();

	GL_REPORT_ERRORD();
    g_Config.iSaveTargetId = 0;

	// For testing zbuffer targets.
    // Renderer::SetZBufferRender();
    // SaveTexture("tex.tga", GL_TEXTURE_RECTANGLE_ARB, s_FakeZTarget, GetTargetWidth(), GetTargetHeight());
}
////////////////////////////////////////////////


void Renderer::DrawDebugText()
{
	// Reset viewport for drawing text
	glViewport(0, 0, OpenGL_GetBackbufferWidth(), OpenGL_GetBackbufferHeight());
	// Draw various messages on the screen, like FPS, statistics, etc.
	char debugtext_buffer[8192];
	char *p = debugtext_buffer;
	p[0] = 0;
	if (g_Config.bShowFPS)
    {
		p+=sprintf(p, "FPS: %d\n", s_fps);
    }

	if (g_Config.bShowEFBCopyRegions)
	{
		// Store Line Size
        GLfloat lSize;
		glGetFloatv(GL_LINE_WIDTH, &lSize);

		// Set Line Size
		glLineWidth(8.0f); 

		glBegin(GL_LINES);

		// Draw EFB copy regions rectangles
		for (std::vector<TRectangle>::const_iterator it = stats.efb_regions.begin(); it != stats.efb_regions.end(); ++it)
		{
			GLfloat halfWidth = Renderer::GetTargetWidth() / 2.0f;
            GLfloat halfHeight = Renderer::GetTargetHeight() / 2.0f;
            GLfloat x =  (GLfloat) -1.0f + ((GLfloat)it->left / halfWidth);
			GLfloat y =  (GLfloat) 1.0f - ((GLfloat)it->top / halfHeight);
			GLfloat x2 = (GLfloat) -1.0f + ((GLfloat)it->right / halfWidth);
			GLfloat y2 = (GLfloat) 1.0f - ((GLfloat)it->bottom / halfHeight);

			// Draw shadow of rect
			glColor3f(0.0f, 0.0f, 0.0f);
			glVertex2f(x, y - 0.01f); glVertex2f(x2 + 0.008f, y - 0.01f);
			glVertex2f(x, y2 - 0.01f); glVertex2f(x2 + 0.008f, y2 - 0.01f);
			glVertex2f(x - 0.01f, y); glVertex2f(x - 0.01f, y2 + 0.008f);
			glVertex2f(x2 - 0.01f, y); glVertex2f(x2 - 0.01f, y2 + 0.008f);

			// Draw rect
			glColor3f(0.0f, 1.0f, 1.0f);
			glVertex2f(x, y); glVertex2f(x2 + 0.008f, y);
			glVertex2f(x, y2); glVertex2f(x2 + 0.008f, y2);
			glVertex2f(x, y); glVertex2f(x, y2 + 0.008f);
			glVertex2f(x2, y); glVertex2f(x2, y2 + 0.008f);
		}

		glEnd();

		// Restore Line Size
		glLineWidth(lSize);

		// Clear stored regions
		stats.efb_regions.clear();
	}

    if (g_Config.bOverlayStats) 
	{
        p+=sprintf(p,"textures created: %i\n",stats.numTexturesCreated);
        p+=sprintf(p,"textures alive:   %i\n",stats.numTexturesAlive);
        p+=sprintf(p,"pshaders created: %i\n",stats.numPixelShadersCreated);
        p+=sprintf(p,"pshaders alive:   %i\n",stats.numPixelShadersAlive);
        p+=sprintf(p,"vshaders created: %i\n",stats.numVertexShadersCreated);
        p+=sprintf(p,"vshaders alive:   %i\n",stats.numVertexShadersAlive);
        p+=sprintf(p,"dlists called:    %i\n",stats.numDListsCalled);
        p+=sprintf(p,"dlists called(f): %i\n",stats.thisFrame.numDListsCalled);
		// not used.
        //p+=sprintf(p,"dlists created:  %i\n",stats.numDListsCreated);
        //p+=sprintf(p,"dlists alive:    %i\n",stats.numDListsAlive);
        //p+=sprintf(p,"strip joins:     %i\n",stats.numJoins);
        p+=sprintf(p,"primitives:       %i\n",stats.thisFrame.numPrims);
		p+=sprintf(p,"primitive joins:  %i\n",stats.thisFrame.numPrimitiveJoins);
		p+=sprintf(p,"buffer splits:    %i\n",stats.thisFrame.numBufferSplits);
		p+=sprintf(p,"draw calls:       %i\n",stats.thisFrame.numDrawCalls);
        p+=sprintf(p,"primitives (DL):  %i\n",stats.thisFrame.numDLPrims);
        p+=sprintf(p,"XF loads:         %i\n",stats.thisFrame.numXFLoads);
        p+=sprintf(p,"XF loads (DL):    %i\n",stats.thisFrame.numXFLoadsInDL);
        p+=sprintf(p,"CP loads:         %i\n",stats.thisFrame.numCPLoads);
        p+=sprintf(p,"CP loads (DL):    %i\n",stats.thisFrame.numCPLoadsInDL);
        p+=sprintf(p,"BP loads:         %i\n",stats.thisFrame.numBPLoads);
        p+=sprintf(p,"BP loads (DL):    %i\n",stats.thisFrame.numBPLoadsInDL);
        p+=sprintf(p,"vertex loaders:   %i\n",stats.numVertexLoaders);

		std::string text1;
		VertexLoaderManager::AppendListToString(&text1);
		// TODO: Check for buffer overflow
		p+=sprintf(p,"%s",text1.c_str());
    }

	if (g_Config.bOverlayBlendStats)
	{
		p+=sprintf(p,"LogicOp Mode: %i\n", stats.logicOpMode);
		p+=sprintf(p,"Source Factor: %i\n", stats.srcFactor);
		p+=sprintf(p,"Destination Factor: %i\n", stats.dstFactor);
		p+=sprintf(p,"Dithering: %s\n", stats.dither==1 ? "Enabled" : "Disabled");
		p+=sprintf(p,"Color Update: %s\n", stats.colorUpdate==1 ? "Enabled" : "Disabled");
		p+=sprintf(p,"Alpha Update: %s\n", stats.alphaUpdate==1 ? "Enabled" : "Disabled");
		p+=sprintf(p,"Dst Alpha Enabled: %s\n", stats.dstAlphaEnable==1 ? "Enabled" : "Disabled");
		p+=sprintf(p,"Dst Alpha: %08x\n", stats.dstAlpha);
	}

	if (g_Config.bOverlayProjStats)
	{
		p+=sprintf(p,"Projection #: X for Raw 6=0 (X for Raw 6!=0)\n\n");
		p+=sprintf(p,"Projection 0: %f (%f) Raw 0: %f\n", stats.gproj_0, stats.g2proj_0, stats.proj_0);
		p+=sprintf(p,"Projection 1: %f (%f)\n", stats.gproj_1, stats.g2proj_1);
		p+=sprintf(p,"Projection 2: %f (%f) Raw 1: %f\n", stats.gproj_2, stats.g2proj_2, stats.proj_1);
		p+=sprintf(p,"Projection 3: %f (%f)\n\n", stats.gproj_3, stats.g2proj_3);
		p+=sprintf(p,"Projection 4: %f (%f)\n", stats.gproj_4, stats.g2proj_4);
		p+=sprintf(p,"Projection 5: %f (%f) Raw 2: %f\n", stats.gproj_5, stats.g2proj_5, stats.proj_2);
		p+=sprintf(p,"Projection 6: %f (%f) Raw 3: %f\n", stats.gproj_6, stats.g2proj_6, stats.proj_3);
		p+=sprintf(p,"Projection 7: %f (%f)\n\n", stats.gproj_7, stats.g2proj_7);
		p+=sprintf(p,"Projection 8: %f (%f)\n", stats.gproj_8, stats.g2proj_8);
		p+=sprintf(p,"Projection 9: %f (%f)\n", stats.gproj_9, stats.g2proj_9);
		p+=sprintf(p,"Projection 10: %f (%f) Raw 4: %f\n\n", stats.gproj_10, stats.g2proj_10, stats.proj_4);
		p+=sprintf(p,"Projection 11: %f (%f) Raw 5: %f\n\n", stats.gproj_11, stats.g2proj_11, stats.proj_5);
		p+=sprintf(p,"Projection 12: %f (%f)\n", stats.gproj_12, stats.g2proj_12);
		p+=sprintf(p,"Projection 13: %f (%f)\n", stats.gproj_13, stats.g2proj_13);
		p+=sprintf(p,"Projection 14: %f (%f)\n", stats.gproj_14, stats.g2proj_14);
		p+=sprintf(p,"Projection 15: %f (%f)\n", stats.gproj_15, stats.g2proj_15);
	}

	// Render a shadow, and then the text.
	Renderer::RenderText(debugtext_buffer, 21, 21, 0xDD000000);
	Renderer::RenderText(debugtext_buffer, 20, 20, 0xFF00FFFF);
}

//////////////////////////////////////////////////////////////////////////////////////
// We can now draw whatever we want on top of the picture. Then we copy the final picture to the output.
// ----------------------
void Renderer::SwapBuffers()
{
	// Count FPS.
	static int fpscount = 0;
    static unsigned long lasttime;
    ++fpscount;
    if (timeGetTime() - lasttime > 1000) 
    {
        lasttime = timeGetTime();
        s_fps = fpscount - 1;
        fpscount = 0;
    }

	DrawDebugText();

	OSD::DrawMessages();
	// -----------------------------

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

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

    GL_REPORT_ERRORD();

    // Clean out old stuff from caches
    VertexShaderCache::ProgressiveCleanup();
    PixelShaderCache::ProgressiveCleanup();
    TextureMngr::ProgressiveCleanup();

    frameCount++;

    // New frame
    stats.ResetFrame();

	// Render to the framebuffer.
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_uFramebuffer);
    if (nZBufferRender > 0) {
        if (--nZBufferRender == 0) {
            // turn off
            nZBufferRender = 0;
            glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);
            Renderer::SetRenderMode(RM_Normal);  // turn off any zwrites
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
}

void Renderer::SetScreenshot(const char *filename)
{
	s_criticalScreenshot.Enter();
	s_sScreenshotName = filename;
	s_bScreenshot = true;
	s_criticalScreenshot.Leave();
}

bool Renderer::SaveRenderTarget(const char *filename, int w, int h)
{
	u8 *data = (u8 *)malloc(3 * w * h);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
	if (glGetError() != GL_NO_ERROR)
		return false;
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

#if defined(HAVE_WX) && HAVE_WX
	wxImage a(w, h, data);
	a.SaveFile(wxString::FromAscii(filename), wxBITMAP_TYPE_BMP);
	bool result = true;
#else
	bool result = SaveTGA(filename, w, h, data);
	free(data);
#endif
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////
// Function: This function does not have the final picture. Use Renderer::Swap() to adjust the final picture.
// Call schedule: Called from VertexShaderManager
// ----------------------
void UpdateViewport()
{
	// -----------------------------------------------------------------------
	// Logging
	// ------------------
    // reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
    // [0] = width/2
    // [1] = height/2
    // [2] = 16777215 * (farz - nearz)
    // [3] = xorig + width/2 + 342
    // [4] = yorig + height/2 + 342
    // [5] = 16777215 * farz

	/*INFO_LOG(VIDEO, "view: topleft=(%f,%f), wh=(%f,%f), z=(%f,%f)",
		rawViewport[3]-rawViewport[0]-342, rawViewport[4]+rawViewport[1]-342,
		2 * rawViewport[0], 2 * rawViewport[1],
		(rawViewport[5] - rawViewport[2]) / 16777215.0f, rawViewport[5] / 16777215.0f);*/
	// --------------------------

	int scissorXOff = bpmem.scissorOffset.x * 2 - 342;
	int scissorYOff = bpmem.scissorOffset.y * 2 - 342;
	// -------------------------------------

	float MValueX = Renderer::GetTargetScaleX();
	float MValueY = Renderer::GetTargetScaleY();
	// -----------------------------------------------------------------------
	// Stretch picture with increased internal resolution
	// ------------------
	int GLx = (int)ceil((xfregs.rawViewport[3] - xfregs.rawViewport[0] - 342 - scissorXOff) * MValueX);
	int GLy = (int)ceil(Renderer::GetTargetHeight() - ((int)(xfregs.rawViewport[4] - xfregs.rawViewport[1] - 342 - scissorYOff)) * MValueY);
	int GLWidth = (int)ceil(abs((int)(2 * xfregs.rawViewport[0])) * MValueX);
	int GLHeight = (int)ceil(abs((int)(2 * xfregs.rawViewport[1])) * MValueY);
	// -------------------------------------

	// Update the view port
	glViewport(GLx, GLy, GLWidth, GLHeight);

	// GLDepthRange - this could be a source of trouble - see the viewport hacks.
	double GLNear = (xfregs.rawViewport[5] - xfregs.rawViewport[2]) / 16777215.0f;
	double GLFar = xfregs.rawViewport[5] / 16777215.0f;
	glDepthRange(GLNear, GLFar);

	// -------------------------------------
	
	// Logging
	/*
	RECT RcTop, RcParent, RcChild;
	HWND Child = EmuWindow::GetWnd();
	HWND Parent = GetParent(Child);
	HWND Top = GetParent(Parent);
	GetWindowRect(Top, &RcTop);
	GetWindowRect(Parent, &RcParent);
	GetWindowRect(Child, &RcChild);
	
	//Console::ClearScreen();	
	DEBUG_LOG(CONSOLE, "----------------------------------------------------------------");
	DEBUG_LOG(CONSOLE, "Top window:     X:%03i Y:%03i Width:%03i Height:%03i", RcTop.left, RcTop.top, RcTop.right - RcTop.left, RcTop.bottom - RcTop.top);
	DEBUG_LOG(CONSOLE, "Parent window:  X:%03i Y:%03i Width:%03i Height:%03i", RcParent.left, RcParent.top, RcParent.right - RcParent.left, RcParent.bottom - RcParent.top);
	DEBUG_LOG(CONSOLE, "Child window:   X:%03i Y:%03i Width:%03i Height:%03i", RcChild.left, RcChild.top, RcChild.right - RcChild.left, RcChild.bottom - RcChild.top);
	DEBUG_LOG(CONSOLE, "----------------------------------------------------------------");
	DEBUG_LOG(CONSOLE, "Res. MValue:    X:%f Y:%f XOffs:%f YOffs:%f", OpenGL_GetXmax(), OpenGL_GetYmax(), OpenGL_GetXoff(), OpenGL_GetYoff());
	DEBUG_LOG(CONSOLE, "GLViewPort:     X:%03i Y:%03i Width:%03i Height:%03i", GLx, GLy, GLWidth, GLHeight);
	DEBUG_LOG(CONSOLE, "GLDepthRange:   Near:%f Far:%f", GLNear, GLFar);
	DEBUG_LOG(CONSOLE, "GLScissor:      X:%03i Y:%03i Width:%03i Height:%03i", GLScissorX, GLScissorY, GLScissorW, GLScissorH);
	DEBUG_LOG(CONSOLE, "----------------------------------------------------------------");
	*/
}
