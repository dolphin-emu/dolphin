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
#include <list>
#include <vector>

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
#include "VertexShader.h"
#include "PixelShaderManager.h"
#include "VertexLoader.h"
#include "XFB.h"
#if !defined(OSX64)
#include "Debugger/Debugger.h" // for the CDebugger class
#endif
#include "Logging/Logging.h" // for Logging()

#ifdef _WIN32
#include "OS/Win32.h"
#else
#endif

struct MESSAGE
{
    MESSAGE() {}
    MESSAGE(const char* p, u32 dw) { strcpy(str, p); dwTimeStamp = dw; }
    char str[255];
    u32 dwTimeStamp;
};	

CGcontext g_cgcontext;
CGprofile g_cgvProf, g_cgfProf;
#if !defined(OSX64)
extern CDebugger* m_frame; // the debugging class
#endif

static int g_MaxTexWidth = 0, g_MaxTexHeight = 0;
static RasterFont* s_pfont = NULL;
static std::list<MESSAGE> s_listMsgs;
static bool s_bFullscreen = false;
static bool s_bOutputCgErrors = true;
static int nZBufferRender = 0; // if > 0, then using zbuffer render
static u32 s_uFramebuffer = 0;
static u32 s_RenderTargets[1] = {0}, s_DepthTarget = 0, s_ZBufferTarget = 0;

static bool s_bATIDrawBuffers = false, s_bHaveStencilBuffer = false;
static Renderer::RenderMode s_RenderMode = Renderer::RM_Normal;
static int s_nCurTarget = 0;
bool g_bBlendLogicOp = false;

float MValueX, MValueY; // Since it can Stretch to fit Window, we need two different multiplication values
int frameCount;

void HandleCgError(CGcontext ctx, CGerror err, void* appdata);

bool Renderer::Create2() 
{
    bool bSuccess = true;
    GLenum err = GL_NO_ERROR;
    g_cgcontext = cgCreateContext();
    cgGetError();
    cgSetErrorHandler(HandleCgError, NULL);

    // fill the opengl extension map
    const char* ptoken = (const char*)glGetString( GL_EXTENSIONS );
    if (ptoken == NULL) return false;

    __Log("Supported OpenGL Extensions:\n");
    __Log(ptoken);     // write to the log file
    __Log("\n");

    if( strstr(ptoken, "GL_EXT_blend_logic_op") != NULL )
        g_bBlendLogicOp = true;
    if( strstr(ptoken, "ATI_draw_buffers") != NULL  && strstr(ptoken, "ARB_draw_buffers") == NULL)
        //Checks if it ONLY has the ATI_draw_buffers extension, some have both
        s_bATIDrawBuffers = true;

    s_bFullscreen = g_Config.bFullscreen;

    if (glewInit() != GLEW_OK) {
        ERROR_LOG("glewInit() failed!\n");
        return false;
    }

    if (!GLEW_EXT_framebuffer_object) {
        ERROR_LOG("*********\nGPU: ERROR: Need GL_EXT_framebufer_object for multiple render targets\nGPU: *********\n");
        bSuccess = false;
    }
    
    if (!GLEW_EXT_secondary_color) {
        ERROR_LOG("*********\nGPU: OGL ERROR: Need GL_EXT_secondary_color\nGPU: *********\n");
        bSuccess = false;
    }

    int numvertexattribs=0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, (GLint *)&numvertexattribs);

    if (numvertexattribs < 11) {
        ERROR_LOG("*********\nGPU: OGL ERROR: Number of attributes %d not enough\nGPU: *********\n", numvertexattribs);
        bSuccess = false;
    }

    if (!bSuccess)
        return false;

#ifdef _WIN32
    if (WGLEW_EXT_swap_control)
        wglSwapIntervalEXT(0);
    else
        ERROR_LOG("no support for SwapInterval (framerate clamped to monitor refresh rate)\n");
#else
#ifdef __linux__
    if (glXSwapIntervalSGI)
       glXSwapIntervalSGI(0);
    else
        ERROR_LOG("no support for SwapInterval (framerate clamped to monitor refresh rate)\n");
#else

	//TODO

#endif
#endif
    // check the max texture width and height
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&g_MaxTexWidth);
    g_MaxTexHeight = g_MaxTexWidth;

    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR) bSuccess = false;

    if (glDrawBuffers == NULL && !GLEW_ARB_draw_buffers)
        glDrawBuffers = glDrawBuffersARB;

    glGenFramebuffersEXT( 1, (GLuint *)&s_uFramebuffer);
    if (s_uFramebuffer == 0) {
        ERROR_LOG("failed to create the renderbuffer\n");
    }

    _assert_( glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT );
    glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, s_uFramebuffer );

    // create the framebuffer targets
    glGenTextures(ARRAYSIZE(s_RenderTargets), (GLuint *)s_RenderTargets);
    for(u32 i = 0; i < ARRAYSIZE(s_RenderTargets); ++i) {
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_RenderTargets[i]);
        // initialize to default
        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, nBackbufferWidth, nBackbufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if( glGetError() != GL_NO_ERROR) {
            glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
            GL_REPORT_ERROR();
        }
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    s_nCurTarget = 0;

    GL_REPORT_ERROR();

    int nMaxMRT = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, (GLint *)&nMaxMRT);

    if (nMaxMRT > 1) {
        // create zbuffer target
        glGenTextures(1, (GLuint *)&s_ZBufferTarget);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_ZBufferTarget);
        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, nBackbufferWidth, nBackbufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if( glGetError() != GL_NO_ERROR) {
            glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
            GL_REPORT_ERROR();
        }
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    // create the depth buffer
    glGenRenderbuffersEXT( 1, (GLuint *)&s_DepthTarget);
    glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, s_DepthTarget);
    glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT, nBackbufferWidth, nBackbufferHeight);
    if (glGetError() != GL_NO_ERROR) {
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, nBackbufferWidth, nBackbufferHeight);
        s_bHaveStencilBuffer = false;
    } else {
		s_bHaveStencilBuffer = true;
	}

    GL_REPORT_ERROR();

    // set as render targets
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, s_RenderTargets[s_nCurTarget], 0 );
    glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, s_DepthTarget );
    GL_REPORT_ERROR();

    if (s_ZBufferTarget != 0) {
        // test to make sure it works
        glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, s_ZBufferTarget, 0);
        bool bFailed = glGetError() != GL_NO_ERROR || glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT;
        glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);
            
        if( bFailed ) {
            glDeleteTextures(1, (GLuint *)&s_ZBufferTarget);
            s_ZBufferTarget = 0;
        }
    }

    if (s_ZBufferTarget == 0)
        ERROR_LOG("disabling ztarget mrt feature (max mrt=%d)\n", nMaxMRT);

    //glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, s_DepthTarget );

    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    nZBufferRender = 0;

    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR)
		bSuccess = false;

    s_pfont = new RasterFont();

	SetAA(g_Config.iMultisampleMode);
    GL_REPORT_ERROR();

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
    g_cgfProf = cgGLGetLatestProfile(CG_GL_FRAGMENT);//CG_PROFILE_ARBFP1;
    cgGLSetOptimalOptions(g_cgvProf);
    cgGLSetOptimalOptions(g_cgfProf);

    //ERROR_LOG("max buffer sizes: %d %d\n", cgGetProgramBufferMaxSize(g_cgvProf), cgGetProgramBufferMaxSize(g_cgfProf));
    int nenvvertparams, nenvfragparams, naddrregisters[2];
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, (GLint *)&nenvvertparams);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, (GLint *)&nenvfragparams);
    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB, (GLint *)&naddrregisters[0]);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB, (GLint *)&naddrregisters[1]);
    __Log("max program env parameters: vert=%d, frag=%d\n", nenvvertparams, nenvfragparams);
    __Log("max program address register parameters: vert=%d, frag=%d\n", naddrregisters[0], naddrregisters[1]);

    if( nenvvertparams < 238 )
        ERROR_LOG("not enough vertex shader environment constants!!\n");

#ifndef _DEBUG
    cgGLSetDebugMode(GL_FALSE);
#endif

    if( cgGetError() != CG_NO_ERROR ) {
        ERROR_LOG("cg error\n");
        return false;
    }
    
    //glEnable(GL_POLYGON_OFFSET_FILL);
    //glEnable(GL_POLYGON_OFFSET_LINE);
    //glPolygonOffset(0, 1);
    if (!Initialize())
        return false;

	XFB_Init();
    return glGetError() == GL_NO_ERROR && bSuccess;
}

void Renderer::Shutdown(void)
{    
    delete s_pfont;
	s_pfont = 0;

	XFB_Shutdown();

    if (g_cgcontext != 0) {
        cgDestroyContext(g_cgcontext);
        g_cgcontext = 0;
    }

    if (s_RenderTargets[0]) {
        glDeleteTextures(ARRAYSIZE(s_RenderTargets), (GLuint *)s_RenderTargets);
        memset(s_RenderTargets, 0, sizeof(s_RenderTargets));
    }
    if (s_DepthTarget) {
        glDeleteRenderbuffersEXT(1, (GLuint *)&s_DepthTarget); s_DepthTarget = 0;
    }
    if (s_uFramebuffer != 0) {
        glDeleteFramebuffersEXT( 1, (GLuint *)&s_uFramebuffer);
        s_uFramebuffer = 0;
    }
}


bool Renderer::Initialize()
{
    glStencilFunc(GL_ALWAYS, 0, 0);
    glBlendFunc(GL_ONE, GL_ONE);

    glViewport(0, 0, GetTargetWidth(), GetTargetHeight());                     // Reset The Current Viewport

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
    
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // perspective correct interpolation of colors and tex coords
    
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, nBackbufferWidth, nBackbufferHeight);
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

    s_RenderMode = Renderer::RM_Normal;

    GLenum err = GL_NO_ERROR;
    GL_REPORT_ERROR();

    return err == GL_NO_ERROR;
}

void Renderer::AddMessage(const char* pstr, u32 ms)
{
    s_listMsgs.push_back(MESSAGE(pstr, timeGetTime() + ms));
}

void Renderer::ProcessMessages()
{
	GLboolean wasEnabled = glIsEnabled(GL_BLEND);

	if (!wasEnabled) glEnable(GL_BLEND);
    
	if (s_listMsgs.size() > 0) {
        int left = 25, top = 15;
		std::list<MESSAGE>::iterator it = s_listMsgs.begin();
        while (it != s_listMsgs.end()) 
		{
			int time_left = (int)(it->dwTimeStamp - timeGetTime());
			int alpha = 255;

			if (time_left < 1024)
			{
				alpha = time_left >> 2;
				if (time_left < 0) alpha = 0;
			}

			alpha <<= 24;

            RenderText(it->str, left+1, top+1, 0x000000|alpha);
            RenderText(it->str, left, top, 0xffff30|alpha);
            top += 15;

            if (time_left <= 0)
                it = s_listMsgs.erase(it);
            else ++it;
        }
    }

	if(!wasEnabled) glDisable(GL_BLEND);
}

void Renderer::RenderText(const char* pstr, int left, int top, u32 color)
{
    glColor4f(
		((color>>16) & 0xff)/255.0f,
		((color>> 8) & 0xff)/255.0f,
		((color>> 0) & 0xff)/255.0f,
		((color>>24) & 0xFF)/255.0f
		);
    s_pfont->printMultilineText(pstr, left * 2.0f / (float)nBackbufferWidth - 1, 1 - top * 2.0f / (float)nBackbufferHeight,0,nBackbufferWidth,nBackbufferHeight);
}

void Renderer::SetAA(int aa)
{

}

void Renderer::ReinitView(int nNewWidth, int nNewHeight)
{
    int oldscreen = s_bFullscreen;

	OpenGL_Shutdown();
    int oldwidth = nBackbufferWidth, oldheight = nBackbufferHeight;
    if (!OpenGL_Create(g_VideoInitialize, nNewWidth, nNewHeight)) {//nNewWidth&~7, nNewHeight&~7) ) {
        ERROR_LOG("Failed to recreate, reverting to old settings\n");
        if (!OpenGL_Create(g_VideoInitialize, oldwidth, oldheight)) {
            g_VideoInitialize.pSysMessage("Failed to revert, exiting...\n");
			// TODO - don't takedown the entire emu
            exit(0);
        }
    }
	OpenGL_MakeCurrent();

    if (oldscreen && !g_Config.bFullscreen) { // if transitioning from full screen
#ifdef _WIN32
        RECT rc;
        rc.left = 0; rc.top = 0;
        rc.right = nNewWidth; rc.bottom = nNewHeight;
        AdjustWindowRect(&rc, EmuWindow::g_winstyle, FALSE);

        RECT rcdesktop;
        GetWindowRect(GetDesktopWindow(), &rcdesktop);

        SetWindowLong( EmuWindow::GetWnd(), GWL_STYLE, EmuWindow::g_winstyle );
        SetWindowPos(EmuWindow::GetWnd(), HWND_TOP, ((rcdesktop.right-rcdesktop.left)-(rc.right-rc.left))/2,
            ((rcdesktop.bottom-rcdesktop.top)-(rc.bottom-rc.top))/2,
            rc.right-rc.left, rc.bottom-rc.top, SWP_SHOWWINDOW);
        UpdateWindow(EmuWindow::GetWnd());
#else // linux
#endif
    }

    nBackbufferWidth = nNewWidth > 16 ? nNewWidth : 16;
    nBackbufferHeight = nNewHeight > 16 ? nNewHeight : 16;
}
int Renderer::GetTargetWidth()
{
	if(g_Config.bStretchToFit)
		return 640;
	else
		return nBackbufferWidth; // return the actual window width
}

int Renderer::GetTargetHeight()
{
	if(g_Config.bStretchToFit)
		return 480;
	else
		return nBackbufferHeight; // return the actual window height
}

bool Renderer::CanBlendLogicOp()
{
    return g_bBlendLogicOp;
}

void Renderer::SetRenderTarget(u32 targ)
{
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, targ!=0?targ:s_RenderTargets[s_nCurTarget], 0 );
}

void Renderer::SetDepthTarget(u32 targ)
{
    glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, targ != 0 ? targ : s_DepthTarget );
}

void Renderer::SetFramebuffer(u32 fb)
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb != 0 ? fb : s_uFramebuffer);
}

u32 Renderer::GetRenderTarget()
{
    return s_RenderTargets[s_nCurTarget];
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
    if (bpmem.blendmode.blendenable) glEnable(GL_BLEND);
    if(bpmem.zmode.updateenable) glDepthMask(GL_TRUE);

    glEnable(GL_VERTEX_PROGRAM_ARB);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    SetColorMask();
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
    for(int i = 1; i < 8; ++i)
		TextureMngr::DisableStage(i);
    GL_REPORT_ERRORD();

	// setup the stencil to only accept pixels that have been written
	glStencilFunc(GL_EQUAL, 1, 0xff);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	if(g_Config.bStretchToFit)
	{
		//TODO: Do Correctly in a bit
		float FactorW = (float)640 / (float)nBackbufferWidth;
		float FactorH = (float)480 / (float)nBackbufferHeight;

		float Max = (FactorW < FactorH) ? FactorH : FactorW;
		float Temp = 1 / Max;
		FactorW *= Temp;
		FactorH *= Temp;

		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2f(-FactorW,-FactorH);
		glTexCoord2f(0, (float)GetTargetHeight()); glVertex2f(-FactorW,FactorH);
		glTexCoord2f((float)GetTargetWidth(), (float)GetTargetHeight()); glVertex2f(FactorW,FactorH);
		glTexCoord2f((float)GetTargetWidth(), 0); glVertex2f(FactorW,-FactorH);

		__Log("%d, %d", FactorW, FactorH);
	}
	else
	{		
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex2f(-1,-1);
		glTexCoord2f(0, (float)(GetTargetHeight())); glVertex2f(-1,1);
		glTexCoord2f((float)(GetTargetWidth()), (float)(GetTargetHeight())); glVertex2f(1,1);
		glTexCoord2f((float)(GetTargetWidth()), 0); glVertex2f(1,-1);
	}
    glEnd();
    
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
        if( s_RenderMode == RM_ZBufferAlpha ) {
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

        if( mode == RM_ZBufferAlpha ) {
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

        if( mode == RM_ZBufferOnly ) {
            // flush and remove stencil
            _assert_(s_RenderMode==RM_ZBufferAlpha);
            FlushZBufferAlphaToTarget();
            glDisable(GL_STENCIL_TEST);

            SetRenderTarget(s_ZBufferTarget);
            glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
            GL_REPORT_ERRORD();
        }
        else {
            _assert_(mode == RM_ZBufferAlpha&&s_RenderMode==RM_ZBufferOnly);
            
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

u32 Renderer::GetZBufferTarget()
{
    return nZBufferRender > 0 ? s_ZBufferTarget : 0;
}

void Renderer::Swap(const TRectangle& rc)
{
    OpenGL_Update(); // just updates the render window position and the backbuffer size

    DVSTARTPROFILE();

    Renderer::SetRenderMode(Renderer::RM_Normal);

    // render to the real buffer now 
    glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 ); // switch to the backbuffer
    glViewport(nXoff, nYoff, nBackbufferWidth, nBackbufferHeight);

    ResetGLState();

    // texture map s_RenderTargets[s_curtarget] onto the main buffer
    glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_RenderTargets[s_nCurTarget]);
    TextureMngr::EnableTexRECT(0);
    // disable all other stages
    for(int i = 1; i < 8; ++i) TextureMngr::DisableStage(i);
    GL_REPORT_ERRORD();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);                                             glVertex2f(-1,-1);
    glTexCoord2f(0, (float)GetTargetHeight());                        glVertex2f(-1,1);
    glTexCoord2f((float)GetTargetWidth(), (float)GetTargetHeight());    glVertex2f(1,1);
    glTexCoord2f((float)GetTargetWidth(), 0);                         glVertex2f(1,-1);
    glEnd();

	if (g_Config.bWireFrame)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
    TextureMngr::DisableStage(0);

    SwapBuffers();

    RestoreGLState();
    GL_REPORT_ERRORD();

    g_Config.iSaveTargetId = 0;

    // for testing zbuffer targets
    //Renderer::SetZBufferRender();
    //SaveTexture("tex.tga", GL_TEXTURE_RECTANGLE_ARB, s_ZBufferTarget, GetTargetWidth(), GetTargetHeight());
}

void Renderer::SwapBuffers()
{
	static int fpscount;
    static int s_fps;
    static unsigned long lasttime;
    ++fpscount;
    if( timeGetTime() - lasttime > 1000 ) 
    {
        lasttime = timeGetTime();
        s_fps = fpscount;
        fpscount = 0;
    }

	// Write logging data to debugger
#if !defined(OSX64)
	if(m_frame)
	{
		Logging(0);
	}
#endif	
    if (g_Config.bOverlayStats) {
        char st[2048];
        char *p = st;
        if(g_Config.bShowFPS)
            p+=sprintf(p, "FPS: %d\n", s_fps); // So it shows up before the stats and doesn't make anyting ugly
        p+=sprintf(p,"Num textures created: %i\n",stats.numTexturesCreated);
        p+=sprintf(p,"Num textures alive:   %i\n",stats.numTexturesAlive);
        p+=sprintf(p,"Num pshaders created: %i\n",stats.numPixelShadersCreated);
        p+=sprintf(p,"Num pshaders alive:   %i\n",stats.numPixelShadersAlive);
        p+=sprintf(p,"Num vshaders created: %i\n",stats.numVertexShadersCreated);
        p+=sprintf(p,"Num vshaders alive:   %i\n",stats.numVertexShadersAlive);
        p+=sprintf(p,"Num dlists called:         %i\n",stats.numDListsCalled);
        p+=sprintf(p,"Num dlists called (frame): %i\n",stats.thisFrame.numDListsCalled);
		// not used.
        //p+=sprintf(p,"Num dlists created:  %i\n",stats.numDListsCreated);
        //p+=sprintf(p,"Num dlists alive:    %i\n",stats.numDListsAlive);
        //p+=sprintf(p,"Num strip joins:     %i\n",stats.numJoins);
        p+=sprintf(p,"Num primitives:       %i\n",stats.thisFrame.numPrims);
		p+=sprintf(p,"Num primitive joins:  %i\n",stats.thisFrame.numPrimitiveJoins);
		p+=sprintf(p,"Num buffer splits:    %i\n",stats.thisFrame.numBufferSplits);
		p+=sprintf(p,"Num draw calls:       %i\n",stats.thisFrame.numDrawCalls);
        p+=sprintf(p,"Num primitives (DL):  %i\n",stats.thisFrame.numDLPrims);
        p+=sprintf(p,"Num XF loads:      %i\n",stats.thisFrame.numXFLoads);
        p+=sprintf(p,"Num XF loads (DL): %i\n",stats.thisFrame.numXFLoadsInDL);
        p+=sprintf(p,"Num CP loads:      %i\n",stats.thisFrame.numCPLoads);
        p+=sprintf(p,"Num CP loads (DL): %i\n",stats.thisFrame.numCPLoadsInDL);
        p+=sprintf(p,"Num BP loads:      %i\n",stats.thisFrame.numBPLoads);
        p+=sprintf(p,"Num BP loads (DL): %i\n",stats.thisFrame.numBPLoadsInDL);
        p+=sprintf(p,"Num vertex loaders:       %i\n",stats.numVertexLoaders);

		Renderer::RenderText(st, 20, 20, 0xFF00FFFF);
    }
    else
    {
        if(g_Config.bShowFPS)
        {
            char strfps[25];
            sprintf(strfps, "%d\n", s_fps);
            Renderer::RenderText(strfps, 20, 20, 0xFF00FFFF);
        }
    }

	Renderer::ProcessMessages();

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

    // copy the rendered from to the real window
	OpenGL_SwapBuffers();

	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);

    GL_REPORT_ERRORD();

    //clean out old stuff from caches
    frameCount++;
    PixelShaderMngr::Cleanup();
    TextureMngr::Cleanup();

    // New frame
    stats.ResetFrame();
    
    glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, s_uFramebuffer );
   
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

bool Renderer::SaveRenderTarget(const char* filename, int jpeg)
{
    bool bflip = true;
    std::vector<u32> data(nBackbufferWidth * nBackbufferHeight);
    glReadPixels(0, 0, nBackbufferWidth, nBackbufferHeight, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
    if (glGetError() != GL_NO_ERROR)
        return false;

    if (bflip) {
        // swap scanlines
        std::vector<u32> scanline(nBackbufferWidth);
        for(int i = 0; i < nBackbufferHeight/2; ++i) {
            memcpy(&scanline[0], &data[i*nBackbufferWidth], nBackbufferWidth*4);
            memcpy(&data[i*nBackbufferWidth], &data[(nBackbufferHeight-i-1)*nBackbufferWidth], nBackbufferWidth*4);
            memcpy(&data[(nBackbufferHeight-i-1)*nBackbufferWidth], &scanline[0], nBackbufferWidth*4);
        }
    }
    
    return SaveTGA(filename, nBackbufferWidth, nBackbufferHeight, &data[0]);
}

void Renderer::SetCgErrorOutput(bool bOutput)
{
    s_bOutputCgErrors = bOutput;
}

void HandleGLError()
{
    const GLubyte* pstr = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
    if (pstr != NULL && pstr[0] != 0)
	{
        GLint loc = 0;
        glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &loc);
        ERROR_LOG("program error at %d: ", loc);
        ERROR_LOG((char*)pstr);
        ERROR_LOG("\n");
    }

    // check the error status of this framebuffer */
    GLenum error = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

    // if error != GL_FRAMEBUFFER_COMPLETE_EXT, there's an error of some sort 
    if (!error)
		return;

//	int w, h;
//	GLint fmt;
//	glGetRenderbufferParameterivEXT(GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_INTERNAL_FORMAT_EXT, &fmt);
//	glGetRenderbufferParameterivEXT(GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_WIDTH_EXT, (GLint *)&w);
//	glGetRenderbufferParameterivEXT(GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_HEIGHT_EXT, (GLint *)&h);

    switch(error)
	{
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			ERROR_LOG("Error! missing a required image/buffer attachment!\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			ERROR_LOG("Error! has no images/buffers attached!\n");
			break;
//      case GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT:
//          ERROR_LOG("Error! has an image/buffer attached in multiple locations!\n");
//           break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			ERROR_LOG("Error! has mismatched image/buffer dimensions!\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			ERROR_LOG("Error! colorbuffer attachments have different types!\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			ERROR_LOG("Error! trying to draw to non-attached color buffer!\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			ERROR_LOG("Error! trying to read from a non-attached color buffer!\n");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			ERROR_LOG("Error! format is not supported by current graphics card/driver!\n");
			break;
		default:
			ERROR_LOG("*UNKNOWN ERROR* reported from glCheckFramebufferStatusEXT()!\n");
			break;
	}
}

void HandleCgError(CGcontext ctx, CGerror err, void* appdata)
{
    if (s_bOutputCgErrors)
	{
        ERROR_LOG("Cg error: %s\n", cgGetErrorString(err));
        const char* listing = cgGetLastListing(g_cgcontext);
        if (listing != NULL) {
            ERROR_LOG("    last listing: %s\n", listing);
        }
    //    glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &loc);
    //    printf("pos: %d\n", loc);
    }
}
