// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Common.h"

#include "GLUtil.h"
#include "Renderer.h"
#include "main.h"
#include "Statistics.h"
#include "../../Plugin_VideoOGL/Src/rasterfont.h"

#define VSYNC_ENABLED 0

static GLuint s_RenderTarget = 0;

RasterFont* s_pfont = NULL;

void Renderer::Init(SVideoInitialize *_pVideoInitialize)
{
    if (!OpenGL_Create(g_VideoInitialize, 640, 480)) // 640x480 will be the default if all else fails
	{
        g_VideoInitialize.pLog("Renderer::Create failed\n", TRUE);
        return;
    }

	_pVideoInitialize->pPeekMessages = g_VideoInitialize.pPeekMessages;
    _pVideoInitialize->pUpdateFPSDisplay = g_VideoInitialize.pUpdateFPSDisplay;
    _pVideoInitialize->pWindowHandle = g_VideoInitialize.pWindowHandle;    
}

void Renderer::Shutdown()
{
    glDeleteTextures(1, &s_RenderTarget);	

    delete s_pfont;
    s_pfont = 0;
}

void Renderer::Prepare()
{
    OpenGL_MakeCurrent();

    // Init extension support.
	if (glewInit() != GLEW_OK) {
        ERROR_LOG(VIDEO, "glewInit() failed!Does your video card support OpenGL 2.x?");
        return;
    }

    	// Handle VSync on/off
#ifdef _WIN32
	if (WGLEW_EXT_swap_control)
		wglSwapIntervalEXT(VSYNC_ENABLED);
	else
		ERROR_LOG(VIDEO, "no support for SwapInterval (framerate clamped to monitor refresh rate)Does your video card support OpenGL 2.x?");
#elif defined(HAVE_X11) && HAVE_X11
	if (glXSwapIntervalSGI)
		glXSwapIntervalSGI(VSYNC_ENABLED);
	else
		ERROR_LOG(VIDEO, "no support for SwapInterval (framerate clamped to monitor refresh rate)");
#endif

    glStencilFunc(GL_ALWAYS, 0, 0);
    // used by hw rasterizer if it enables blending and depth test
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);

    glShadeModel(GL_SMOOTH);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);
    glEnable(GL_SCISSOR_TEST);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment
    
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    //glDisable(GL_SCISSOR_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    s_pfont = new RasterFont();    

    // legacy multitexturing: select texture channel only.
    glActiveTexture(GL_TEXTURE0);
    glClientActiveTexture(GL_TEXTURE0);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glGenTextures(1, &s_RenderTarget);
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
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

void Renderer::DrawDebugText()
{
    char debugtext_buffer[8192];
	char *p = debugtext_buffer;
	p[0] = 0;

	if (g_Config.bShowStats) 
	{
        p+=sprintf(p,"Objects: %i\n",stats.thisFrame.numDrawnObjects);
        p+=sprintf(p,"Primatives: %i\n",stats.thisFrame.numPrimatives);
        p+=sprintf(p,"Vertices Loaded: %i\n",stats.thisFrame.numVerticesLoaded);

        p+=sprintf(p,"Triangles Input:   %i\n",stats.thisFrame.numTrianglesIn);
        p+=sprintf(p,"Triangles Rejected:   %i\n",stats.thisFrame.numTrianglesRejected);
        p+=sprintf(p,"Triangles Culled:   %i\n",stats.thisFrame.numTrianglesCulled);
        p+=sprintf(p,"Triangles Clipped:  %i\n",stats.thisFrame.numTrianglesClipped);
        p+=sprintf(p,"Triangles Drawn:   %i\n",stats.thisFrame.numTrianglesDrawn);

        p+=sprintf(p,"Rasterized Pix:   %i\n",stats.thisFrame.rasterizedPixels);
        p+=sprintf(p,"TEV Pix In:   %i\n",stats.thisFrame.tevPixelsIn);
        p+=sprintf(p,"TEV Pix Out:   %i\n",stats.thisFrame.tevPixelsOut);        
    }

	// Render a shadow, and then the text.
	Renderer::RenderText(debugtext_buffer, 21, 21, 0xDD000000);
	Renderer::RenderText(debugtext_buffer, 20, 20, 0xFFFFFF00);
}

void Renderer::DrawTexture(u8 *texture, int width, int height)
{
    OpenGL_Update(); // just updates the render window position and the backbuffer size	

	GLsizei glWidth = (GLsizei)OpenGL_GetBackbufferWidth();
	GLsizei glHeight = (GLsizei)OpenGL_GetBackbufferHeight();

	// Update GLViewPort
	glViewport(0, 0, glWidth, glHeight);
    glScissor(0, 0, glWidth, glHeight);

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, s_RenderTarget);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	GL_REPORT_ERRORD();

    GLfloat u_max = (GLfloat)width;
    GLfloat v_max = (GLfloat)glHeight;
    
    glBegin(GL_QUADS);
		glTexCoord2f(0, v_max); glVertex2f(-1, -1);
		glTexCoord2f(0, 0); glVertex2f(-1,  1);
		glTexCoord2f(u_max, 0); glVertex2f( 1,  1);
		glTexCoord2f(u_max, v_max); glVertex2f( 1, -1);
	glEnd();

    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);    
}

void Renderer::SwapBuffer()
{
    DrawDebugText();

    glFlush();

	OpenGL_SwapBuffers();
    
	GL_REPORT_ERRORD();

    stats.ResetFrame();
	
	// Clear framebuffer
	glClearColor(0, 0, 0, 0);
    glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GL_REPORT_ERRORD();
}


