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
#include <math.h>

#include "../../Plugin_VideoOGL/Src/GLUtil.h"
#include "../../Plugin_VideoOGL/Src/RasterFont.h"
#include "SWRenderer.h"
#include "SWStatistics.h"

static GLuint s_RenderTarget = 0;

static GLint attr_pos = -1, attr_tex = -1;
static GLint uni_tex = -1;
static GLuint program;

// Rasterfont isn't compatible with GLES
#ifndef USE_GLES
RasterFont* s_pfont = NULL;
#endif

void SWRenderer::Init()
{
}

void SWRenderer::Shutdown()
{
	glDeleteProgram(program);
	glDeleteTextures(1, &s_RenderTarget);	
#ifndef USE_GLES
	delete s_pfont;
	s_pfont = 0;
#endif
}

void CreateShaders()
{
	static const char *fragShaderText =
		"varying " PREC " vec2 TexCoordOut;\n"
		"uniform " TEXTYPE " Texture;\n"
		"void main() {\n"
		"	" PREC " vec4 tmpcolor;\n"
		"	tmpcolor = " TEXFUNC "(Texture, TexCoordOut);\n"
		"	gl_FragColor = tmpcolor;\n"
		"}\n";
	static const char *vertShaderText =
		"attribute vec4 pos;\n"
		"attribute vec2 TexCoordIn;\n "
		"varying vec2 TexCoordOut;\n "
		"void main() {\n"
		"   gl_Position = pos;\n"
		"	TexCoordOut = TexCoordIn;\n"
		"}\n";

	program = OpenGL_CompileProgram(vertShaderText, fragShaderText);

	glUseProgram(program);

	uni_tex = glGetUniformLocation(program, "Texture");
	attr_pos = glGetAttribLocation(program, "pos");
	attr_tex = glGetAttribLocation(program, "TexCoordIn"); 
}

void SWRenderer::Prepare()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment
	glGenTextures(1, &s_RenderTarget);

	CreateShaders();
	// TODO: Enable for GLES once RasterFont supports GLES
#ifndef USE_GLES
	s_pfont = new RasterFont();
	glEnable(GL_TEXTURE_RECTANGLE_ARB);
#endif
	GL_REPORT_ERRORD();
}

void SWRenderer::RenderText(const char* pstr, int left, int top, u32 color)
{
#ifndef USE_GLES
	int nBackbufferWidth = (int)GLInterface->GetBackBufferWidth();
	int nBackbufferHeight = (int)GLInterface->GetBackBufferHeight();
	
	s_pfont->printMultilineText(pstr,
		left * 2.0f / (float)nBackbufferWidth - 1,
		1 - top * 2.0f / (float)nBackbufferHeight,
		0, nBackbufferWidth, nBackbufferHeight, color);
#endif
}

void SWRenderer::DrawDebugText()
{
	char debugtext_buffer[8192];
	char *p = debugtext_buffer;
	p[0] = 0;

	if (g_SWVideoConfig.bShowStats) 
	{
        p+=sprintf(p,"Objects: %i\n",swstats.thisFrame.numDrawnObjects);
        p+=sprintf(p,"Primatives: %i\n",swstats.thisFrame.numPrimatives);
        p+=sprintf(p,"Vertices Loaded: %i\n",swstats.thisFrame.numVerticesLoaded);

        p+=sprintf(p,"Triangles Input:   %i\n",swstats.thisFrame.numTrianglesIn);
        p+=sprintf(p,"Triangles Rejected:   %i\n",swstats.thisFrame.numTrianglesRejected);
        p+=sprintf(p,"Triangles Culled:   %i\n",swstats.thisFrame.numTrianglesCulled);
        p+=sprintf(p,"Triangles Clipped:  %i\n",swstats.thisFrame.numTrianglesClipped);
        p+=sprintf(p,"Triangles Drawn:   %i\n",swstats.thisFrame.numTrianglesDrawn);

        p+=sprintf(p,"Rasterized Pix:   %i\n",swstats.thisFrame.rasterizedPixels);
        p+=sprintf(p,"TEV Pix In:   %i\n",swstats.thisFrame.tevPixelsIn);
        p+=sprintf(p,"TEV Pix Out:   %i\n",swstats.thisFrame.tevPixelsOut);        
    }

	// Render a shadow, and then the text.
	SWRenderer::RenderText(debugtext_buffer, 21, 21, 0xDD000000);
	SWRenderer::RenderText(debugtext_buffer, 20, 20, 0xFFFFFF00);
}

void SWRenderer::DrawTexture(u8 *texture, int width, int height)
{
	GLsizei glWidth = (GLsizei)GLInterface->GetBackBufferWidth();
	GLsizei glHeight = (GLsizei)GLInterface->GetBackBufferHeight();

	// Update GLViewPort
	glViewport(0, 0, glWidth, glHeight);
	glScissor(0, 0, glWidth, glHeight);

	glBindTexture(TEX2D, s_RenderTarget);

	glTexImage2D(TEX2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture);
	glTexParameteri(TEX2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(TEX2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	GLfloat u_max = (GLfloat)width;
	GLfloat v_max = (GLfloat)height;
	 
	static const GLfloat verts[4][2] = {
		{ -1, -1}, // Left top
		{ -1,  1}, // left bottom
		{  1,  1}, // right bottom
		{  1, -1} // right top
	};
	//Texture rectangle uses pixel coordinates
#ifndef USE_GLES
	static const GLfloat texverts[4][2] = {
		{0, v_max},
		{0, 0},
		{u_max, 0},
		{u_max, v_max}
	};
#else
	static const GLfloat texverts[4][2] = {
		{0, 1},
		{0, 0},
		{1, 0},
		{1, 1}
	};
#endif
	glVertexAttribPointer(attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(attr_tex, 2, GL_FLOAT, GL_FALSE, 0, texverts);
	glEnableVertexAttribArray(attr_pos);
	glEnableVertexAttribArray(attr_tex);
		glActiveTexture(GL_TEXTURE0); 
		glUniform1i(uni_tex, 0);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(attr_pos);
	glDisableVertexAttribArray(attr_tex);

	glBindTexture(TEX2D, 0); 
	GL_REPORT_ERRORD();
}

void SWRenderer::SwapBuffer()
{
	DrawDebugText();

	glFlush();

	GLInterface->Swap();
    
	 swstats.ResetFrame();
	
#ifndef USE_GLES
	glClearDepth(1.0f);
#endif
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GL_REPORT_ERRORD();
}
