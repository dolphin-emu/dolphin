// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Common.h"
#include "Core/Core.h"
#include "VideoBackends/OGL/GLUtil.h"
#include "VideoBackends/Software/RasterFont.h"
#include "VideoBackends/Software/SWCommandProcessor.h"
#include "VideoBackends/Software/SWRenderer.h"
#include "VideoBackends/Software/SWStatistics.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/OnScreenDisplay.h"

static GLuint s_RenderTarget = 0;

static GLint attr_pos = -1, attr_tex = -1;
static GLint uni_tex = -1;
static GLuint program;

static u8 *s_xfbColorTexture[2];
static int s_currentColorTexture = 0;

static volatile bool s_bScreenshot;
static std::mutex s_criticalScreenshot;
static std::string s_sScreenshotName;


// Rasterfont isn't compatible with GLES
// degasus: I think it does, but I can't test it
RasterFont* s_pfont = nullptr;

void SWRenderer::Init()
{
	s_bScreenshot = false;
}

void SWRenderer::Shutdown()
{
	delete [] s_xfbColorTexture[0];
	delete [] s_xfbColorTexture[1];
	glDeleteProgram(program);
	glDeleteTextures(1, &s_RenderTarget);
	if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGL)
	{
		delete s_pfont;
		s_pfont = nullptr;
	}
}

void CreateShaders()
{
	static const char *fragShaderText =
		"#ifdef GL_ES\n"
		"precision highp float;\n"
		"#endif\n"
		"varying vec2 TexCoordOut;\n"
		"uniform sampler2D Texture;\n"
		"void main() {\n"
		"	gl_FragColor = texture2D(Texture, TexCoordOut);\n"
		"}\n";
	static const char *vertShaderText =
		"#ifdef GL_ES\n"
		"precision highp float;\n"
		"#endif\n"
		"attribute vec4 pos;\n"
		"attribute vec2 TexCoordIn;\n "
		"varying vec2 TexCoordOut;\n "
		"void main() {\n"
		"	gl_Position = pos;\n"
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
	s_xfbColorTexture[0] = new u8[640*568*4];
	s_xfbColorTexture[1] = new u8[640*568*4];

	s_currentColorTexture = 0;

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment
	glGenTextures(1, &s_RenderTarget);

	CreateShaders();
	// TODO: Enable for GLES once RasterFont supports GLES
	if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGL)
	{
		s_pfont = new RasterFont();
		glEnable(GL_TEXTURE_2D);
	}
	GL_REPORT_ERRORD();
}

void SWRenderer::SetScreenshot(const char *_szFilename)
{
	std::lock_guard<std::mutex> lk(s_criticalScreenshot);
	s_sScreenshotName = _szFilename;
	s_bScreenshot = true;
}

void SWRenderer::RenderText(const char* pstr, int left, int top, u32 color)
{
	if (GLInterface->GetMode() != GLInterfaceMode::MODE_OPENGL)
		return;
	int nBackbufferWidth = (int)GLInterface->GetBackBufferWidth();
	int nBackbufferHeight = (int)GLInterface->GetBackBufferHeight();
	glColor4f(((color>>16) & 0xff)/255.0f, ((color>> 8) & 0xff)/255.0f,
			((color>> 0) & 0xff)/255.0f, ((color>>24) & 0xFF)/255.0f);
	s_pfont->printMultilineText(pstr,
			left * 2.0f / (float)nBackbufferWidth - 1,
			1 - top * 2.0f / (float)nBackbufferHeight,
			0, nBackbufferWidth, nBackbufferHeight);
}

void SWRenderer::DrawDebugText()
{
	char debugtext_buffer[8192];
	char *p = debugtext_buffer;
	p[0] = 0;

	if (g_SWVideoConfig.bShowStats)
	{
		p+=sprintf(p,"Objects: %i\n",swstats.thisFrame.numDrawnObjects);
		p+=sprintf(p,"Primitives: %i\n",swstats.thisFrame.numPrimatives);
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

u8* SWRenderer::getColorTexture() {
	return s_xfbColorTexture[!s_currentColorTexture];
}

void SWRenderer::swapColorTexture() {
	s_currentColorTexture = !s_currentColorTexture;
}

void SWRenderer::UpdateColorTexture(EfbInterface::yuv422_packed *xfb, u32 fbWidth, u32 fbHeight)
{
	if (fbWidth*fbHeight > 640*568) {
		ERROR_LOG(VIDEO, "Framebuffer is too large: %ix%i", fbWidth, fbHeight);
		return;
	}

	u32 offset = 0;
	u8 *TexturePointer = getColorTexture();

	for (u16 y = 0; y < fbHeight; y++)
	{
		for (u16 x = 0; x < fbWidth; x+=2)
		{
			// We do this one color sample (aka 2 RGB pixles) at a time
			int Y1 = xfb[x].Y - 16;
			int Y2 = xfb[x+1].Y - 16;
			int U  = int(xfb[x].UV) - 128;
			int V  = int(xfb[x+1].UV) - 128;

			// We do the inverse BT.601 conversion for YCbCr to RGB
			// http://www.equasys.de/colorconversion.html#YCbCr-RGBColorFormatConversion
			TexturePointer[offset++] = std::min<u8>(255.0f, std::max(0.0f, 1.164f * Y1              + 1.596f * V));
			TexturePointer[offset++] = std::min<u8>(255.0f, std::max(0.0f, 1.164f * Y1 - 0.392f * U - 0.813f * V));
			TexturePointer[offset++] = std::min<u8>(255.0f, std::max(0.0f, 1.164f * Y1 + 2.017f * U             ));
			TexturePointer[offset++] = 255;

			TexturePointer[offset++] = std::min<u8>(255.0f, std::max(0.0f, 1.164f * Y2              + 1.596f * V));
			TexturePointer[offset++] = std::min<u8>(255.0f, std::max(0.0f, 1.164f * Y2 - 0.392f * U - 0.813f * V));
			TexturePointer[offset++] = std::min<u8>(255.0f, std::max(0.0f, 1.164f * Y2 + 2.017f * U             ));
			TexturePointer[offset++] = 255;
		}
		xfb += fbWidth;
	}
	swapColorTexture();
}

// Called on the GPU thread
void SWRenderer::Swap(u32 fbWidth, u32 fbHeight)
{
	GLInterface->Update(); // just updates the render window position and the backbuffer size
	if (!g_SWVideoConfig.bHwRasterizer)
		SWRenderer::DrawTexture(s_xfbColorTexture[s_currentColorTexture], fbWidth, fbHeight);

	swstats.frameCount++;
	SWRenderer::SwapBuffer();
	Core::Callback_VideoCopiedToXFB(true); // FIXME: should this function be called FrameRendered?
}

void SWRenderer::DrawTexture(u8 *texture, int width, int height)
{
	// FIXME: This should add black bars when the game has set the VI to render less than the full xfb.

	// Save screenshot
	if (s_bScreenshot)
	{
		std::lock_guard<std::mutex> lk(s_criticalScreenshot);
		TextureToPng(texture, width*4, s_sScreenshotName, width, height, false);
		// Reset settings
		s_sScreenshotName.clear();
		s_bScreenshot = false;
	}

	GLsizei glWidth = (GLsizei)GLInterface->GetBackBufferWidth();
	GLsizei glHeight = (GLsizei)GLInterface->GetBackBufferHeight();


	// Update GLViewPort
	glViewport(0, 0, glWidth, glHeight);
	glScissor(0, 0, glWidth, glHeight);

	glBindTexture(GL_TEXTURE_2D, s_RenderTarget);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_UNSIGNED_BYTE, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glUseProgram(program);
	static const GLfloat verts[4][2] = {
		{ -1, -1}, // Left top
		{ -1,  1}, // left bottom
		{  1,  1}, // right bottom
		{  1, -1} // right top
	};
	static const GLfloat texverts[4][2] = {
		{0, 1},
		{0, 0},
		{1, 0},
		{1, 1}
	};

	glVertexAttribPointer(attr_pos, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(attr_tex, 2, GL_FLOAT, GL_FALSE, 0, texverts);
	glEnableVertexAttribArray(attr_pos);
	glEnableVertexAttribArray(attr_tex);
	glUniform1i(uni_tex, 0);
	glActiveTexture(GL_TEXTURE0);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(attr_pos);
	glDisableVertexAttribArray(attr_tex);

	glBindTexture(GL_TEXTURE_2D, 0);
	GL_REPORT_ERRORD();
}

void SWRenderer::SwapBuffer()
{
	// Do our OSD callbacks
	OSD::DoCallbacks(OSD::OSD_ONFRAME);

	DrawDebugText();

	glFlush();

	GLInterface->Swap();

	swstats.ResetFrame();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GL_REPORT_ERRORD();
}
