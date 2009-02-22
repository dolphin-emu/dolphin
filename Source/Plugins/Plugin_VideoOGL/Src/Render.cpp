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
#include "VertexLoaderManager.h"
#include "VertexLoader.h"
#include "XFB.h"
#include "OnScreenDisplay.h"
#include "Timer.h"

#include "main.h" // Local
#ifdef _WIN32
#include "OS/Win32.h"
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

static int nZBufferRender = 0; // if > 0, then use zbuffer render, and count down.

// A framebuffer is a set of render targets: a color and a z buffer. They can be either RenderBuffers or Textures.
static GLuint s_uFramebuffer = 0;

// The size of these should be a (not necessarily even) multiple of the EFB size, 640x528, but isn'.t
static GLuint s_RenderTarget = 0;
static GLuint s_DepthTarget = 0;
static GLuint s_ZBufferTarget = 0;

static bool s_bATIDrawBuffers = false;
static bool s_bHaveStencilBuffer = false;

static Renderer::RenderMode s_RenderMode = Renderer::RM_Normal;
bool g_bBlendSeparate = false;
int frameCount;

void HandleCgError(CGcontext ctx, CGerror err, void *appdata);

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

static u32 s_blendMode;

bool Renderer::Init()
{
    bool bSuccess = true;
	int numvertexattribs = 0;
    GLenum err = GL_NO_ERROR;
    g_cgcontext = cgCreateContext();

    cgGetError();
    cgSetErrorHandler(HandleCgError, NULL);

    // fill the opengl extension map
    const char* ptoken = (const char*)glGetString(GL_EXTENSIONS);
    if (ptoken == NULL) return false;

    INFO_LOG("Supported OpenGL Extensions:\n");
    INFO_LOG(ptoken);  // write to the log file
    INFO_LOG("\n");

	if (GLEW_EXT_blend_func_separate && GLEW_EXT_blend_equation_separate)
        g_bBlendSeparate = true;

	//Checks if it ONLY has the ATI_draw_buffers extension, some have both
    if (GLEW_ATI_draw_buffers && !GLEW_ARB_draw_buffers) 
        s_bATIDrawBuffers = true;

    s_bFullscreen = g_Config.bFullscreen;

    if (glewInit() != GLEW_OK) {
        ERROR_LOG("glewInit() failed!\nDoes your video card support OpenGL 2.x?");
        return false;
    }

    if (!GLEW_EXT_framebuffer_object) {
        ERROR_LOG("*********\nGPU: ERROR: Need GL_EXT_framebufer_object for multiple render targets\nGPU: *********\nDoes your video card support OpenGL 2.x?");
        bSuccess = false;
    }
    
    if (!GLEW_EXT_secondary_color) {
        ERROR_LOG("*********\nGPU: OGL ERROR: Need GL_EXT_secondary_color\nGPU: *********\nDoes your video card support OpenGL 2.x?");
        bSuccess = false;
    }

    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, (GLint *)&numvertexattribs);

    if (numvertexattribs < 11) {
        ERROR_LOG("*********\nGPU: OGL ERROR: Number of attributes %d not enough\nGPU: *********\nDoes your video card support OpenGL 2.x?", numvertexattribs);
        bSuccess = false;
    }

    if (!bSuccess)
        return false;

#ifdef _WIN32
    if (WGLEW_EXT_swap_control)
        wglSwapIntervalEXT(0);
    else
        ERROR_LOG("no support for SwapInterval (framerate clamped to monitor refresh rate)\nDoes your video card support OpenGL 2.x?");
#elif defined(HAVE_X11) && HAVE_X11
    if (glXSwapIntervalSGI)
       glXSwapIntervalSGI(0);
    else
        ERROR_LOG("no support for SwapInterval (framerate clamped to monitor refresh rate)\n");
#else

	//TODO

#endif

    // check the max texture width and height
	GLint max_texture_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&max_texture_size);
	if (max_texture_size < 1024) {
		ERROR_LOG("GL_MAX_TEXTURE_SIZE too small at %i - must be at least 1024", max_texture_size);
	}

    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR) bSuccess = false;

    if (glDrawBuffers == NULL && !GLEW_ARB_draw_buffers)
        glDrawBuffers = glDrawBuffersARB;

    glGenFramebuffersEXT(1, (GLuint *)&s_uFramebuffer);
    if (s_uFramebuffer == 0) {
        ERROR_LOG("failed to create the renderbuffer\nDoes your video card support OpenGL 2.x?");
    }

    _assert_(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_uFramebuffer);

	// The size of the framebuffer targets should really NOT be the size of the OpenGL viewport.
	// The EFB is larger than 640x480 - in fact, it's 640x528, give or take a couple of lines.
	// So the below is wrong.
    int nBackbufferWidth = (int)OpenGL_GetWidth();
    int nBackbufferHeight = (int)OpenGL_GetHeight();
    
    // Create the framebuffer target
    glGenTextures(1, (GLuint *)&s_RenderTarget);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_RenderTarget);

	// Setup the texture params
    // initialize to default
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, nBackbufferWidth, nBackbufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (glGetError() != GL_NO_ERROR) {
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
        GL_REPORT_ERROR();
    }
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    GL_REPORT_ERROR();

    int nMaxMRT = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, (GLint *)&nMaxMRT);

    if (nMaxMRT > 1) 
	{
        // create zbuffer target
        glGenTextures(1, (GLuint *)&s_ZBufferTarget);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_ZBufferTarget);
        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, nBackbufferWidth, nBackbufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

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

    // create the depth buffer
    glGenRenderbuffersEXT(1, (GLuint *)&s_DepthTarget);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_DepthTarget);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, nBackbufferWidth, nBackbufferHeight);
    
    if (glGetError() != GL_NO_ERROR) 
	{
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, nBackbufferWidth, nBackbufferHeight);
        s_bHaveStencilBuffer = false;
    } 
	else 
	{
		s_bHaveStencilBuffer = true;
	}

    GL_REPORT_ERROR();

    // set as render targets
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, s_RenderTarget, 0);
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, s_DepthTarget);
    
	GL_REPORT_ERROR();

    if (s_ZBufferTarget != 0) {
        // test to make sure it works
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, s_ZBufferTarget, 0);
        bool bFailed = glGetError() != GL_NO_ERROR || glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT;
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);
            
        if (bFailed) {
            glDeleteTextures(1, (GLuint *)&s_ZBufferTarget);
            s_ZBufferTarget = 0;
        }
    }

	if (s_ZBufferTarget == 0)
        ERROR_LOG("disabling ztarget mrt feature (max mrt=%d)\n", nMaxMRT);

	// Why is this left here?
    //glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, s_DepthTarget);

    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    nZBufferRender = 0;

    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR)
		bSuccess = false;

    s_pfont = new RasterFont();

    // load the effect, find the best profiles (if any)
    if (cgGLIsProfileSupported(CG_PROFILE_ARBVP1) != CG_TRUE) {
        ERROR_LOG("arbvp1 not supported\n");
        return false;
    }

    if (cgGLIsProfileSupported(CG_PROFILE_ARBFP1) != CG_TRUE) {
        ERROR_LOG("arbfp1 not supported\n");
        return false;
    }

    g_cgvProf = cgGLGetLatestProfile(CG_GL_VERTEX);
    g_cgfProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);
    cgGLSetOptimalOptions(g_cgvProf);
    cgGLSetOptimalOptions(g_cgfProf);

    //ERROR_LOG("max buffer sizes: %d %d\n", cgGetProgramBufferMaxSize(g_cgvProf), cgGetProgramBufferMaxSize(g_cgfProf));
    int nenvvertparams, nenvfragparams, naddrregisters[2];
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, (GLint *)&nenvvertparams);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, (GLint *)&nenvfragparams);
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB, (GLint *)&naddrregisters[0]);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB, (GLint *)&naddrregisters[1]);
    DEBUG_LOG("max program env parameters: vert=%d, frag=%d\n", nenvvertparams, nenvfragparams);
    DEBUG_LOG("max program address register parameters: vert=%d, frag=%d\n", naddrregisters[0], naddrregisters[1]);

	if (nenvvertparams < 238)
        ERROR_LOG("not enough vertex shader environment constants!!\n");

#ifndef _DEBUG
    cgGLSetDebugMode(GL_FALSE);
#endif

    if (cgGetError() != CG_NO_ERROR) {
        ERROR_LOG("cg error\n");
        return false;
    }
    
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
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
	// Do we really need to set this initial glScissor() value? Wont it be called all the time while the game is running?
    //glScissor(0, 0, (int)OpenGL_GetWidth(), (int)OpenGL_GetHeight());
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
    return (g_Config.bStretchToFit ? 640 : (int)OpenGL_GetWidth());
}

int Renderer::GetTargetHeight()
{
    return (g_Config.bStretchToFit ? 480 : (int)OpenGL_GetHeight());
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
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,
		fb != 0 ? fb : s_uFramebuffer);
}

GLuint Renderer::GetRenderTarget()
{
	return s_RenderTarget;
}
GLuint Renderer::GetZBufferTarget()
{
	return nZBufferRender > 0 ? s_ZBufferTarget : 0;
}

void Renderer::ResetGLState()
{
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
    glEnable(GL_SCISSOR_TEST);

    if (bpmem.genMode.cullmode > 0) glEnable(GL_CULL_FACE);
    if (bpmem.zmode.testenable) glEnable(GL_DEPTH_TEST);
    if (bpmem.zmode.updateenable) glDepthMask(GL_TRUE);

    glEnable(GL_VERTEX_PROGRAM_ARB);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    SetColorMask();
	SetBlendMode(true);
}

void Renderer::SetColorMask()
{
    if (bpmem.blendmode.alphaupdate && bpmem.blendmode.colorupdate)
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    else if (bpmem.blendmode.alphaupdate)
        glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_TRUE);
    else if (bpmem.blendmode.colorupdate) 
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
}

void Renderer::SetBlendMode(bool forceUpdate)
{	
	// blend mode bit mask
	// 0 - blend enable
	// 1 - dst alpha enable
	// 2 - reverse subtract enable (else add)
	// 3-5 - srcRGB function
	// 6-8 - dstRGB function
	// 9-16 - dstAlpha

	u32 newval = bpmem.blendmode.subtract << 2;

	if (g_bBlendSeparate) {
		newval |= bpmem.dstalpha.enable ? 3 : 0;
		newval |= bpmem.dstalpha.alpha << 9;
	}

	if (bpmem.blendmode.blendenable) {		
		newval |= 1;

		if (bpmem.blendmode.subtract) {
			newval |= 0x0048; // src 1 dst 1
		} else {
			newval |= bpmem.blendmode.srcfactor << 3;
			newval |= bpmem.blendmode.dstfactor << 6;
		}
	} else {
		newval |= 0x0008;	// src 1 dst 0
	}

	u32 changes = forceUpdate ? 0xFFFFFFFF : newval ^ s_blendMode;

	if (changes & 1) {
		newval & 1 ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
	}

	bool dstAlphaEnable = g_bBlendSeparate && newval & 2;

	if (changes & 6) {		
		// dst alpha enable or subtract enable change
		if (dstAlphaEnable)
			glBlendEquationSeparate(newval & 4 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD, GL_FUNC_ADD);
		else
			glBlendEquation(newval & 4 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
	}

	if (changes & 0x1FA) {
		// dst alpha enable or blend RGB change
		if (dstAlphaEnable)
			glBlendFuncSeparate(glSrcFactors[(newval >> 3) & 7], glDestFactors[(newval >> 6) & 7], GL_CONSTANT_ALPHA, GL_ZERO);
		else
			glBlendFunc(glSrcFactors[(newval >> 3) & 7], glDestFactors[(newval >> 6) & 7]);
	}

	if (dstAlphaEnable && changes & 0x1FE00) {
		// dst alpha color change
		glBlendColor(0, 0, 0, (float)bpmem.dstalpha.alpha / 255.0f);
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
    float MValueX = OpenGL_GetXmax();
    float MValueY = OpenGL_GetYmax();
	float rc_left = (float)bpmem.scissorTL.x - xoff - 342; // left = 0
	rc_left *= MValueX;
	if (rc_left < 0) rc_left = 0;

	float rc_top = (float)bpmem.scissorTL.y - yoff - 342; // right = 0
	rc_top *= MValueY;
	if (rc_top < 0) rc_top = 0;
    
	float rc_right = (float)bpmem.scissorBR.x - xoff - 341; // right = 640
	rc_right *= MValueX;
	if (rc_right > 640 * MValueX) rc_right = 640 * MValueX;

	float rc_bottom = (float)bpmem.scissorBR.y - yoff - 341; // bottom = 480
	rc_bottom *= MValueY;
	if (rc_bottom > 480 * MValueY) rc_bottom = 480 * MValueY;

   /*LOG("Scissor: lt=(%d,%d), rb=(%d,%d,%i), off=(%d,%d)\n",
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
			(int)(rc_right-rc_left), // width = 640 for example
			(int)(rc_bottom-rc_top) // height = 480 for example
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
    nZBufferRender = 10; // give it 10 frames
    GLenum s_drawbuffers[2] = {
		GL_COLOR_ATTACHMENT0_EXT,
		GL_COLOR_ATTACHMENT1_EXT
	};
    glDrawBuffers(2, s_drawbuffers);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, s_ZBufferTarget, 0);
    _assert_(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT);
}

void Renderer::FlushZBufferAlphaToTarget()
{
    ResetGLState();

    SetRenderTarget(0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);

    glViewport(0, 0, GetTargetWidth(), GetTargetHeight());

    // texture map s_RenderTargets[s_curtarget] onto the main buffer
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_ZBufferTarget);
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
	// TODO: Investigate BlitFramebufferEXT.
	if (g_Config.bStretchToFit)
	{
		//TODO: Do Correctly in a bit
        float FactorW = 640.f / (float)OpenGL_GetWidth();
		float FactorH = 480.f / (float)OpenGL_GetHeight();

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
        _assert_(GetZBufferTarget() && bpmem.zmode.updateenable);

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
        _assert_(GetZBufferTarget());
        _assert_(s_bHaveStencilBuffer);

        if (mode == RM_ZBufferOnly) {
            // flush and remove stencil
            _assert_(s_RenderMode==RM_ZBufferAlpha);
            FlushZBufferAlphaToTarget();
            glDisable(GL_STENCIL_TEST);

            SetRenderTarget(s_ZBufferTarget);
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


//////////////////////////////////////////////////////////////////////////////////////
// This function has the final picture if the XFB functions are not used. We adjust the aspect ratio
// here.
// ----------------------
void Renderer::Swap(const TRectangle& rc)
{
    OpenGL_Update(); // just updates the render window position and the backbuffer size

    DVSTARTPROFILE();

    Renderer::SetRenderMode(Renderer::RM_Normal);

#if 0
	// Blit the FBO to do Anti-Aliasing
	// Not working?
	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_uFramebuffer); 
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, 0);
	glBlitFramebufferEXT(0, 0, 640, 480, 0, 0, OpenGL_GetWidth(), OpenGL_GetHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, s_uFramebuffer); 

#else
	// render to the real buffer now 
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); // switch to the backbuffer

	// -----------------------------------------------------------------------
	// GLViewPort variables
	// ------------------
	/* Work with float values for the XFB supplement and aspect ratio functions. These are default
	   values that are used if the XFB supplement and the keep aspect ratio function are unused */
	float FloatGLWidth = (float)OpenGL_GetWidth();
	float FloatGLHeight = (float)OpenGL_GetHeight();
	float FloatXOffset = 0, FloatYOffset = 0;
	// -------------------------------------


	// -----------------------------------------------------------------------
	// XFB supplement, fix the black borders problem
	// ------------------
	/* I'm limiting it to the stretch to fit option because I don't know how the other mode works. The reason
	   I don't allow this option together with UseXFB is that they are supplements and the XFB function
	   should be able to produce the same result */
	if(g_Config.bStretchToFit && !g_Config.bUseXFB)
	{
		// The rendering window size
		float WinWidth = (float)OpenGL_GetWidth();
		float WinHeight = (float)OpenGL_GetHeight();

		// The fraction of the screen that the image occupies
		// Rc.right and rc.bottom is the original picture pixel size
		/* There is a +1 in Rc (earlier called multirc, the input to this function), but these
		   adjustments seems to work better without it. */
		float WidthRatio = (float)(rc.right - 1) / 640.0f;
		float HeightRatio = (float)(rc.bottom - 1) / 480.0f;

		// The pixel size of the image on the screen, adjusted for the actual window size
		float OldWidth = WidthRatio * (float)WinWidth;
		float OldHeight = HeightRatio * (float)WinHeight;

		// The adjusted width and height
		FloatGLWidth = ceil((float)WinWidth / WidthRatio);
		FloatGLHeight = ceil((float)WinHeight / HeightRatio);

		// The width and height deficit in actual pixels
		float WidthDeficit = (float)WinWidth - OldWidth;
		float HeightDeficit = (float)WinHeight - OldHeight;

		// The picture will be drawn from the bottom so we need this YOffset
		// The X-axis needs no adjustment because the picture begins from the left
		FloatYOffset = -HeightDeficit / HeightRatio;

		// -----------------------------------------------------------------------
		// Logging
		// ------------------
		/*
		Console::ClearScreen();
		Console::Print("Bpmem        L:%i T:%i X:%i Y:%i\n", bpmem.copyTexSrcXY.x, bpmem.copyTexSrcXY.y, bpmem.copyTexSrcWH.x, bpmem.copyTexSrcWH.y);
		Console::Print("Config       Left:%i Top:%i Width:%i Height:%i\n", g_Config.iScreenLeft, g_Config.iScreenTop, g_Config.iScreenWidth, g_Config.iScreenHeight);
		Console::Print("Input        Left:%i Top:%i Right:%i Bottom:%i\n", rc.left, rc.top, rc.right, rc.bottom);
		Console::Print("Old picture: Width[%1.2f]:%4.0f Height[%1.2f]:%4.0f\n", WidthRatio, OldWidth, HeightRatio, OldHeight);
		Console::Print("New picture: Width[%1.2f]:%4.0f Height[%1.2f]:%4.0f YOffset:%4.0f YDeficit:%4.0f\n", WidthRatio, WinWidth, HeightRatio, WinHeight, FloatYOffset, HeightDeficit);
		Console::Print("----------------------------------------------------------------\n");
		*/
		// ------------------------------
	}
	// ------------------------------



	// -----------------------------------------------------------------------
	/* Keep aspect ratio at 4:3. This may be interesting if you for example have a 5:4 screen but don't like
	   the stretching, and would rather have a letterbox. */
	//		Output: GLWidth, GLHeight, XOffset, YOffset
	// ------------------

	// The rendering window size
	float WinWidth = (float)OpenGL_GetWidth();
	float WinHeight = (float)OpenGL_GetHeight();
	// The rendering window aspect ratio as a fraction of the 4:3 ratio
	float Ratio = WinWidth / WinHeight / (4.0f / 3.0f);
	float wAdj, hAdj;
	// Actual pixel size of the picture after adjustment
	float PictureWidth = WinWidth, PictureHeight = WinHeight;

	// This function currently only works together with the Stretch To Fit option
	if (g_Config.bKeepAR && g_Config.bStretchToFit)
	{
		// Check if height or width is the limiting factor. If ratio > 1 the picture is to wide and have to limit the width.
		if (Ratio > 1)
		{
			// ------------------------------------------------
			// Calculate the new width and height for glViewport, this is not the actual size of either the picture or the screen
			// ----------------
			wAdj = Ratio;
			hAdj = 1.0;
			FloatGLWidth = FloatGLWidth / wAdj;
			FloatGLHeight = FloatGLHeight / hAdj;
			// --------------------

			// ------------------------------------------------
			// Calculate the new X offset
			// ----------------
			// The picture width
			PictureWidth = WinWidth / Ratio;
			// Move the left of the picture to the middle of the screen
			FloatXOffset = FloatXOffset + WinWidth / 2.0f;
			// Then remove half the picture height to move it to the horizontal center
			FloatXOffset = FloatXOffset - PictureWidth / 2.0f;
			// --------------------
		}
		// The window is to high, we have to limit the height
		else
		{
			// ------------------------------------------------
			// Calculate the new width and height for glViewport, this is not the actual size of either the picture or the screen
			// ----------------
			// Invert the ratio to make it > 1
			Ratio = 1.0f / Ratio;
			wAdj = 1.0f;
			hAdj = Ratio;
			FloatGLWidth = FloatGLWidth / wAdj;
			FloatGLHeight = FloatGLHeight / hAdj;
			// --------------------
			
			// ------------------------------------------------
			// Calculate the new Y offset
			// ----------------
			// The picture height
			PictureHeight = WinHeight / Ratio;
			// Keep the picture on the bottom of the screen, this is needed because YOffset may not be 0 here
			FloatYOffset = FloatYOffset / hAdj;
			// Move the bottom of the picture to the middle of the screen
			FloatYOffset = FloatYOffset + WinHeight / 2.0f;
			// Then remove half the picture height to move it to the vertical center
			FloatYOffset = FloatYOffset - PictureHeight / 2.0f;
			// --------------------
		}

		// -----------------------------------------------------------------------
		// Logging
		// ------------------
		/*
		Console::Print("Screen      Width:%4.0f Height:%4.0f Ratio:%1.2f\n", WinWidth, WinHeight, Ratio);
		Console::Print("GL          Width:%4.1f Height:%4.1f\n", FloatGLWidth, FloatGLHeight);
		Console::Print("Picture     Width:%4.1f Height:%4.1f YOffset:%4.0f\n", PictureWidth, PictureHeight, FloatYOffset);
		Console::Print("----------------------------------------------------------------\n");
		*/
		// ------------------------------
	}
	// -------------------------------------

	// -----------------------------------------------------------------------
	/* Adjustments to
			FloatGLWidth
			FloatGLHeight
			XOffset
			YOffset
	   are done. Calculate the new new windows width and height. */
	// --------------------
		int GLx = OpenGL_GetXoff(); // These two are zero
		int GLy = OpenGL_GetYoff();
		int GLWidth = ceil(FloatGLWidth);
		int GLHeight = ceil(FloatGLHeight);

		// Because there is no round() function we use round(float) = floor(float + 0.5) instead
		int YOffset = floor(FloatYOffset + 0.5);
		int XOffset = floor(FloatXOffset+ 0.5);
	// -------------------------------------


	// Update GLViewPort
	glViewport(
		GLx + XOffset,
		GLy + YOffset,
		GLWidth,
		GLHeight
		);

	// Reset GL state
    ResetGLState();

    // texture map s_RenderTargets[s_curtarget] onto the main buffer
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_RenderTarget);
    TextureMngr::EnableTexRECT(0);
    
	// disable all other stages
    for (int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);

    GL_REPORT_ERRORD();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);                                              glVertex2f(-1,-1);
    glTexCoord2f(0, (float)GetTargetHeight());                       glVertex2f(-1,1);
    glTexCoord2f((float)GetTargetWidth(), (float)GetTargetHeight()); glVertex2f(1,1);
    glTexCoord2f((float)GetTargetWidth(), 0);                        glVertex2f(1,-1);
    glEnd();

	// Wireframe
	if (g_Config.bWireFrame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    TextureMngr::DisableStage(0);

    SwapBuffers();

    RestoreGLState();
#endif
	GL_REPORT_ERRORD();

    g_Config.iSaveTargetId = 0;

    // for testing zbuffer targets
    //Renderer::SetZBufferRender();
    //SaveTexture("tex.tga", GL_TEXTURE_RECTANGLE_ARB, s_ZBufferTarget, GetTargetWidth(), GetTargetHeight());
}
////////////////////////////////////////////////


void Renderer::SwapBuffers()
{
	static int fpscount;
    static int s_fps;
    static unsigned long lasttime;
	char debugtext_buffer[8192];
	char *p = debugtext_buffer;
	p[0] = 0;

    ++fpscount;
    if (timeGetTime() - lasttime > 1000) 
    {
        lasttime = timeGetTime();
        s_fps = fpscount;
        fpscount = 0;
    }

	if (g_Config.bShowFPS)
    {
		p+=sprintf(p, "FPS: %d\n", s_fps);
    }

    if (g_Config.bOverlayStats) {
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

	OSD::DrawMessages();

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

	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);

    GL_REPORT_ERRORD();

    // Clean out old stuff from caches
    PixelShaderCache::Cleanup();
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
	int nBackbufferWidth = (int)OpenGL_GetWidth();
	int nBackbufferHeight = (int)OpenGL_GetHeight();
	glColor4f(((color>>16) & 0xff)/255.0f, ((color>> 8) & 0xff)/255.0f,
	          ((color>> 0) & 0xff)/255.0f, ((color>>24) & 0xFF)/255.0f);
	s_pfont->printMultilineText(pstr,
		left * 2.0f / (float)nBackbufferWidth - 1,
		1 - top * 2.0f / (float)nBackbufferHeight,
		0, nBackbufferWidth, nBackbufferHeight);
}

bool Renderer::SaveRenderTarget(const char* filename, int jpeg)
{
    bool bflip = true;
    int nBackbufferHeight = (int)OpenGL_GetHeight();
    int nBackbufferWidth = (int)OpenGL_GetWidth();

    std::vector<u32> data(nBackbufferWidth * nBackbufferHeight);
    glReadPixels(0, 0, nBackbufferWidth, nBackbufferHeight, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
    if (glGetError() != GL_NO_ERROR)
        return false;

    if (bflip) {
        // swap scanlines
        std::vector<u32> scanline(nBackbufferWidth);
        for (int i = 0; i < nBackbufferHeight/2; ++i) {
            memcpy(&scanline[0], &data[i*nBackbufferWidth], nBackbufferWidth*4);
            memcpy(&data[i*nBackbufferWidth], &data[(nBackbufferHeight-i-1)*nBackbufferWidth], nBackbufferWidth*4);
            memcpy(&data[(nBackbufferHeight-i-1)*nBackbufferWidth], &scanline[0], nBackbufferWidth*4);
        }
    }
    
    return SaveTGA(filename, nBackbufferWidth, nBackbufferHeight, &data[0]);
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

	/*INFO_LOG("view: topleft=(%f,%f), wh=(%f,%f), z=(%f,%f)\n",
		rawViewport[3]-rawViewport[0]-342, rawViewport[4]+rawViewport[1]-342,
		2 * rawViewport[0], 2 * rawViewport[1],
		(rawViewport[5] - rawViewport[2]) / 16777215.0f, rawViewport[5] / 16777215.0f);*/
	// --------------------------


	// -----------------------------------------------------------------------
	// GLViewPort variables
	// ------------------
	int GLWidth, GLHeight, GLx, GLy;
	float FloatGLWidth = fabs(2 * xfregs.rawViewport[0]);
	float FloatGLHeight = fabs(2 * xfregs.rawViewport[1]);

	// rawViewport[0] = 320, rawViewport[1] = -240
	int scissorXOff = bpmem.scissorOffset.x * 2 - 342;
	int scissorYOff = bpmem.scissorOffset.y * 2 - 342;
	// -------------------------------------


	// -----------------------------------------------------------------------
	// Stretch picture while keeping the native resolution
	// ------------------
	if (g_Config.bStretchToFit)
	{
		GLx = (int)(xfregs.rawViewport[3] - xfregs.rawViewport[0] - 342 - scissorXOff);
		GLy = Renderer::GetTargetHeight() - ((int)(xfregs.rawViewport[4] - xfregs.rawViewport[1] - 342 - scissorYOff));
		// Round up to the nearest integer
		GLWidth = (int)ceil(FloatGLWidth);
		GLHeight = (int)ceil(FloatGLHeight);
	}
	// -----------------------------------------------------------------------
	// Stretch picture with increased internal resolution
	// ------------------
	else
	{
	    float MValueX = OpenGL_GetXmax();
	    float MValueY = OpenGL_GetYmax();

		GLx = (int)ceil((xfregs.rawViewport[3] - xfregs.rawViewport[0] - 342 - scissorXOff) * MValueX);
		GLy = (int)ceil(Renderer::GetTargetHeight() - ((int)(xfregs.rawViewport[4] - xfregs.rawViewport[1] - 342 - scissorYOff)) * MValueY);
		GLWidth = (int)ceil(abs((int)(2 * xfregs.rawViewport[0])) * MValueX);
		GLHeight = (int)ceil(abs((int)(2 * xfregs.rawViewport[1])) * MValueY);
	}
	// -------------------------------------


	// Update the view port
	glViewport(
		GLx, GLy,
		GLWidth, GLHeight
		);


	// -----------------------------------------------------------------------
	// GLDepthRange
	// ------------------
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
	Console::Print("----------------------------------------------------------------\n");
	Console::Print("Top window:     X:%03i Y:%03i Width:%03i Height:%03i\n", RcTop.left, RcTop.top, RcTop.right - RcTop.left, RcTop.bottom - RcTop.top);
	Console::Print("Parent window:  X:%03i Y:%03i Width:%03i Height:%03i\n", RcParent.left, RcParent.top, RcParent.right - RcParent.left, RcParent.bottom - RcParent.top);
	Console::Print("Child window:   X:%03i Y:%03i Width:%03i Height:%03i\n", RcChild.left, RcChild.top, RcChild.right - RcChild.left, RcChild.bottom - RcChild.top);
	Console::Print("----------------------------------------------------------------\n");
	Console::Print("Res. MValue:    X:%f Y:%f XOffs:%f YOffs:%f\n", OpenGL_GetXmax(), OpenGL_GetYmax(), OpenGL_GetXoff(), OpenGL_GetYoff());
	Console::Print("GLViewPort:     X:%03i Y:%03i Width:%03i Height:%03i\n", GLx, GLy, GLWidth, GLHeight);
	Console::Print("GLDepthRange:   Near:%f Far:%f\n", GLNear, GLFar);
	Console::Print("GLScissor:      X:%03i Y:%03i Width:%03i Height:%03i\n", GLScissorX, GLScissorY, GLScissorW, GLScissorH);
	Console::Print("----------------------------------------------------------------\n");
	*/
}
