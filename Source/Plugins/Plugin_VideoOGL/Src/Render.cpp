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

#include "CommonPaths.h"
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

#ifdef _WIN32
	#include "Win32Window.h" // warning: crapcode
#else
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
CGcontext g_cgcontext;
CGprofile g_cgvProf;
CGprofile g_cgfProf;

Common::Thread *scrshotThread = NULL;

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
static bool s_bNativeResolution = false;

static volatile bool s_bScreenshot = false;
static Common::CriticalSection s_criticalScreenshot;
static std::string s_sScreenshotName;

int frameCount;
static int s_fps = 0;

// These STAY CONSTANT during execution, no matter how much you resize the game window.
// TODO: Add functionality to reinit all the render targets when the window is resized.
static int s_targetwidth;   // Size of render buffer FBO.
static int s_targetheight;

static FramebufferManager s_framebufferManager;
static GLuint s_tempScreenshotFramebuffer = 0;

#ifndef _WIN32
int OSDChoice = 0 , OSDTime = 0, OSDInternalW = 0, OSDInternalH = 0;
#endif

namespace {

// Screenshot thread struct
typedef struct
{
	int W, H;
	std::string filename;
	wxImage *img;
} ScrStrct;

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


// Init functions
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

	if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

    if (glDrawBuffers == NULL && !GLEW_ARB_draw_buffers)
        glDrawBuffers = glDrawBuffersARB;
	
	if (!GLEW_ARB_texture_non_power_of_two) {
		WARN_LOG(VIDEO, "ARB_texture_non_power_of_two not supported. This extension is not yet used, though.");
	}

	if (g_Config.bNativeResolution)
	{
		s_targetwidth = g_Config.b2xResolution ? EFB_WIDTH * 2 : EFB_WIDTH;
		s_targetheight = g_Config.b2xResolution ? EFB_HEIGHT * 2 : EFB_HEIGHT;
	}
	else
	{
		// The size of the framebuffer targets should really NOT be the size of the OpenGL viewport.
		// The EFB is larger than 640x480 - in fact, it's 640x528, give or take a couple of lines.
		// So the below is wrong.
		// This should really be grabbed from config rather than from OpenGL.
		// JP: Set these to the biggest of the 2x mode and the custom resolution so that the framebuffer
		// does not get to be too small
		int W = (int)OpenGL_GetBackbufferWidth(), H = (int)OpenGL_GetBackbufferHeight();
		s_targetwidth = (1280 >= W) ? 1280 : W;
		s_targetheight = (960 >= H) ? 960 : H;

		// Compensate height of render target for scaling, so that we get something close to the correct number of
		// vertical pixels.
		s_targetheight *= 528.0 / 480.0;
	}

	// Ensure a minimum target size so that the native res target always fits.
	if (s_targetwidth < EFB_WIDTH)
		s_targetwidth = EFB_WIDTH;
	if (s_targetheight < EFB_HEIGHT)
		s_targetheight = EFB_HEIGHT;

    if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

	// Initialize the FramebufferManager
	s_framebufferManager.Init(s_targetwidth, s_targetheight, s_MSAASamples, s_MSAACoverageSamples);

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
    
    if (!InitializeGL())
        return false;

    return glGetError() == GL_NO_ERROR && bSuccess;
}

void Renderer::Shutdown(void)
{    
    delete s_pfont;
	s_pfont = 0;

    if (g_cgcontext) {
        cgDestroyContext(g_cgcontext);
        g_cgcontext = 0;
	}

	glDeleteFramebuffersEXT(1, &s_tempScreenshotFramebuffer);
	s_tempScreenshotFramebuffer = 0;

	s_framebufferManager.Shutdown();

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

    return GL_REPORT_ERROR() == GL_NO_ERROR;
}


// Return the rendering window width and height
int Renderer::GetTargetWidth()
{
	return s_targetwidth;
}

int Renderer::GetTargetHeight()
{
	return s_targetheight;
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
	return s_framebufferManager.ConvertEFBRectangle(rc);
}

void Renderer::SetFramebuffer(GLuint fb)
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb != 0 ? fb : s_framebufferManager.GetEFBFramebuffer());
}

void Renderer::ResetGLState()
{
	// Gets us to a reasonably sane state where it's possible to do things like
	// image copies with textured quads, etc.
    glDisable(GL_VERTEX_PROGRAM_ARB);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void UpdateViewport();

void Renderer::RestoreGLState()
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

u32 Renderer::AccessEFB(EFBAccessType type, int x, int y)
{
	// Get the rectangular target region covered by the EFB pixel.
	EFBRectangle efbPixelRc;
	efbPixelRc.left = x;
	efbPixelRc.top = y;
	efbPixelRc.right = x + 1;
	efbPixelRc.bottom = y + 1;

	TargetRectangle targetPixelRc = Renderer::ConvertEFBRectangle(efbPixelRc);

	switch (type)
	{

	case PEEK_Z:
	{
		if (s_MSAASamples > 1)
		{
			// Resolve our rectangle.
			s_framebufferManager.GetEFBDepthTexture(efbPixelRc);
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_framebufferManager.GetResolvedFramebuffer());
		}

		// Sample from the center of the target region.
		int srcX = (targetPixelRc.left + targetPixelRc.right) / 2;
		int srcY = (targetPixelRc.top + targetPixelRc.bottom) / 2;

		u32 z = 0;
		glReadPixels(srcX, srcY, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, &z);
		GL_REPORT_ERRORD();

		// Scale the 32-bit value returned by glReadPixels to a 24-bit
		// value (GC uses a 24-bit Z-buffer).
		return z >> 8;
	}

	case POKE_Z:
		// TODO: Implement
		break;

	case PEEK_COLOR:
	{
		// TODO: Find some way to test PEEK_COLOR. Wind Waker may be using it
		// for pictograph quests.

		if (s_MSAASamples > 1)
		{
			// Resolve our rectangle.
			s_framebufferManager.GetEFBColorTexture(efbPixelRc);
			glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_framebufferManager.GetResolvedFramebuffer());
		}

		// Sample from the center of the target region.
		int srcX = (targetPixelRc.left + targetPixelRc.right) / 2;
		int srcY = (targetPixelRc.top + targetPixelRc.bottom) / 2;

		// Read back pixel in BGRA format, then byteswap to get GameCube's
		// ARGB format.
		u32 color = 0;
		glReadPixels(srcX, srcY, 1, 1, GL_BGRA, GL_UNSIGNED_BYTE, &color);
		GL_REPORT_ERRORD();

		return Common::swap32(color);
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

// Apply AA if enabled
GLuint Renderer::ResolveAndGetRenderTarget(const EFBRectangle &source_rect)
{
	return s_framebufferManager.GetEFBColorTexture(source_rect);
}

GLuint Renderer::ResolveAndGetDepthTarget(const EFBRectangle &source_rect)
{
	return s_framebufferManager.GetEFBDepthTexture(source_rect);
}


/////////////////////////////////////////////////////////////////////////////
// Function: This function handles the OpenGL glScissor() function
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Call browser: OpcodeDecoding.cpp ExecuteDisplayList > Decode() > LoadBPReg()
//		case 0x52 > SetScissorRect()
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
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

// Aspect ratio functions
static void ComputeBackbufferRectangle(TargetRectangle *rc)
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
		// Adjust the X and Y offset
		FloatXOffset = FloatXOffset - (IncreasedWidth / 2.0);
		FloatYOffset = FloatYOffset - (IncreasedHeight / 2.0);
		//NOTICE_LOG(OSREPORT, "Crop       Ratio:%1.2f IncreasedHeight:%3.0f YOffset:%3.0f", Ratio, IncreasedHeight, FloatYOffset);
		//NOTICE_LOG(OSREPORT, "Crop       FloatGLWidth:%1.2f FloatGLHeight:%3.0f", (float)FloatGLWidth, (float)FloatGLHeight);
		//NOTICE_LOG(OSREPORT, "");
	}

	// round(float) = floor(float + 0.5)
	int XOffset = floor(FloatXOffset + 0.5);
	int YOffset = floor(FloatYOffset + 0.5);
	rc->left = XOffset;
	rc->top = YOffset + ceil(FloatGLHeight);
	rc->right = XOffset + ceil(FloatGLWidth);
	rc->bottom = YOffset;
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
	// If we're about to write to a requested XFB, make sure the previous
	// contents make it to the screen first.
	VideoFifo_CheckSwapRequestAt(xfbAddr, fbWidth, fbHeight);

	s_framebufferManager.CopyToXFB(xfbAddr, fbWidth, fbHeight, sourceRc);
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{
	const XFBSource* xfbSource = s_framebufferManager.GetXFBSource(xfbAddr, fbWidth, fbHeight);
	if (!xfbSource)
	{
		WARN_LOG(VIDEO, "Failed to get video for this frame");
		return;
	}

    OpenGL_Update(); // just updates the render window position and the backbuffer size
    DVSTARTPROFILE();

    ResetGLState();

	TargetRectangle back_rc;
	ComputeBackbufferRectangle(&back_rc);

	TargetRectangle sourceRc;

	if (g_Config.bAutoScale || g_Config.bUseXFB)
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

	int yOffset = (g_Config.bUseXFB && field == FIELD_LOWER) ? -1 : 0;
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

		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
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
	if (g_Config.bWireFrame)
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
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_framebufferManager.GetEFBFramebuffer());
	}

	// Frame dumps are handled a little differently in Windows
#ifdef _WIN32
	if (g_Config.bDumpFrames)
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
				s_bAVIDumping = AVIDump::Start(EmuWindow::GetChildParentWnd(), w, h);
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
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, s_framebufferManager.GetEFBFramebuffer());
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
	if (g_Config.bDumpFrames) {
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
		if(s_bLastFrameDumped && f_pFrameDump != NULL) {
			fclose(f_pFrameDump);
			f_pFrameDump = NULL;
		}

		s_bLastFrameDumped = false;
	}
#endif

	// Place messages on the picture, then copy it to the screen
    SwapBuffers();

	// Why save this as s_bNativeResolution if we updated it every frame?
	s_bNativeResolution = g_Config.bNativeResolution;

    RestoreGLState();

	GL_REPORT_ERRORD();
    g_Config.iSaveTargetId = 0;

	// For testing zbuffer targets.
    // Renderer::SetZBufferRender();
    // SaveTexture("tex.tga", GL_TEXTURE_RECTANGLE_ARB, s_FakeZTarget, GetTargetWidth(), GetTargetHeight());
}


//////////////////////////////////////////////////////////////////////////////////////////
// We can now draw whatever we want on top of the picture. Then we copy the final picture to the output.
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void Renderer::SwapBuffers()
{
	// ---------------------------------------------------------------------
	// Count FPS.
	// ¯¯¯¯¯¯¯¯¯¯¯¯¯
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

	for (int i = 0; i < 8; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_RECTANGLE_ARB);
	}
	glActiveTexture(GL_TEXTURE0);

	DrawDebugText();

    GL_REPORT_ERRORD();

	OSD::DrawMessages();

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
    VertexShaderCache::ProgressiveCleanup();
    PixelShaderCache::ProgressiveCleanup();
    TextureMngr::ProgressiveCleanup();

    frameCount++;

    // New frame
    stats.ResetFrame();

	// Render to the framebuffer.
	SetFramebuffer(0);

	GL_REPORT_ERRORD();
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

	if (g_Config.bShowFPS)
		p+=sprintf(p, "FPS: %d\n", s_fps);

	if (g_Config.bShowEFBCopyRegions)
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

	// OSD Menu messages
	if (OSDChoice > 0 && g_Config.bEFBCopyDisableHotKey)
	{
		OSDTime = timeGetTime() + 3000;
		OSDChoice = -OSDChoice;
	}
	if ((u32)OSDTime > timeGetTime() && g_Config.bEFBCopyDisableHotKey)
	{
		std::string T1 = "";
		std::string T2 = "";

		int W, H;
		sscanf(g_Config.iInternalRes, "%dx%d", &W, &H);

		std::string OSDM1 =
			g_Config.bNativeResolution || g_Config.b2xResolution ?
			(g_Config.bNativeResolution ? 
			StringFromFormat("%i x %i (native)", OSDInternalW, OSDInternalH)
			: StringFromFormat("%i x %i (2x)", OSDInternalW, OSDInternalH))
			: StringFromFormat("%i x %i (custom)", W, H);
		std::string OSDM21 =
			!(g_Config.bKeepAR43 || g_Config.bKeepAR169) ? "-": (g_Config.bKeepAR43 ? "4:3" : "16:9");
		std::string OSDM22 =
			g_Config.bCrop ? " (crop)" : "";			
		std::string OSDM31 =
			g_Config.bCopyEFBToRAM ? "RAM" : "Texture";
		std::string OSDM32 =
			g_Config.bEFBCopyDisable ? "No" : "Yes";

		// If there is more text than this we will have a collission
		if (g_Config.bShowFPS)
			{ T1 += "\n\n"; T2 += "\n\n"; }

		// The latest changed setting in yellow
		T1 += (OSDChoice == -1) ? StringFromFormat("3: Internal Resolution: %s\n", OSDM1.c_str()) : "\n";
		T1 += (OSDChoice == -2) ? StringFromFormat("4: Lock Aspect Ratio: %s%s\n", OSDM21.c_str(), OSDM22.c_str()) : "\n";
		T1 += (OSDChoice == -3) ? StringFromFormat("5: Copy Embedded Framebuffer to %s: %s\n", OSDM31.c_str(), OSDM32.c_str()) : "\n";
			
		// The other settings in cyan
		T2 += !(OSDChoice == -1) ? StringFromFormat("3: Internal Resolution: %s\n", OSDM1.c_str()) : "\n";
		T2 += !(OSDChoice == -2) ? StringFromFormat("4: Lock Aspect Ratio: %s\n", OSDM21.c_str(), OSDM22.c_str()) : "\n";
		T2 += !(OSDChoice == -3) ? StringFromFormat("5: Copy Embedded Framebuffer to %s: %s\n", OSDM31.c_str(), OSDM32.c_str()) : "\n";

		// Render a shadow, and then the text
		Renderer::RenderText(T1.c_str(), 21, 21, 0xDD000000);
		Renderer::RenderText(T1.c_str(), 20, 20, 0xFFffff00);
		Renderer::RenderText(T2.c_str(), 21, 21, 0xDD000000);
		Renderer::RenderText(T2.c_str(), 20, 20, 0xFF00FFFF);
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

THREAD_RETURN TakeScreenshot(void *pArgs)
{
	ScrStrct *threadStruct = (ScrStrct *)pArgs;
	
	// These will contain the final image size
	float FloatW = (float)threadStruct->W;
	float FloatH = (float)threadStruct->H;

	// Handle aspect ratio for the final ScrStrct to look exactly like what's on screen.
	if (g_Config.bKeepAR43 || g_Config.bKeepAR169)
	{
		float Ratio = (FloatW / FloatH) / (g_Config.bKeepAR43 ? (4.0f / 3.0f) : (16.0f / 9.0f));
		
		// If ratio > 1 the picture is too wide and we have to limit the width.
		if (Ratio > 1)
			FloatW /= Ratio;
		// ratio == 1 or the image is too high, we have to limit the height.
		else
			FloatH *= Ratio;

		threadStruct->img->Rescale((int)FloatW, (int)FloatH, wxIMAGE_QUALITY_HIGH);
	}

	// Save the screenshot and finally kill the wxImage object
	threadStruct->img->SaveFile(wxString::FromAscii(threadStruct->filename.c_str()), wxBITMAP_TYPE_PNG);
	threadStruct->img->Destroy();
	
	// Show success messages
	OSD::AddMessage(StringFromFormat("Saved %i x %i %s", (int)FloatW, (int)FloatH, threadStruct->filename.c_str()).c_str(), 2000);

	return 0;
}

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
	//Enable support for PNG file type.
	wxImage::AddHandler( new wxPNGHandler );

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

// This function does not have the final picture. Use Renderer::Swap() to adjust the final picture.
// Call schedule: Called from VertexShaderManager
void UpdateViewport()
{
	// ---------
	// Logging
	// ---------
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
	// --------

	int scissorXOff = bpmem.scissorOffset.x * 2 - 342;
	int scissorYOff = bpmem.scissorOffset.y * 2 - 342;
	// -------------------------------------

	float MValueX = Renderer::GetTargetScaleX();
	float MValueY = Renderer::GetTargetScaleY();

	// Stretch picture with increased internal resolution
	int GLx = (int)ceil((xfregs.rawViewport[3] - xfregs.rawViewport[0] - 342 - scissorXOff) * MValueX);
	int GLy = (int)ceil(Renderer::GetTargetHeight() - ((int)(xfregs.rawViewport[4] - xfregs.rawViewport[1] - 342 - scissorYOff)) * MValueY);
	int GLWidth = (int)ceil(abs((int)(2 * xfregs.rawViewport[0])) * MValueX);
	int GLHeight = (int)ceil(abs((int)(2 * xfregs.rawViewport[1])) * MValueY);

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

