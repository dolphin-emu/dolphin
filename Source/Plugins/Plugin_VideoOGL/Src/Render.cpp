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
#include "Atomic.h"

#include <vector>
#include <cmath>
#include <cstdio>

#include "GLUtil.h"

#include "FileUtil.h"

#ifdef _WIN32
#include <mmsystem.h>
#endif

#include "CommonPaths.h"
#include "VideoConfig.h"
#include "Statistics.h"
#include "ImageWrite.h"
#include "PixelEngine.h"
#include "Render.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "TextureCache.h"
#include "RasterFont.h"
#include "VertexShaderGen.h"
#include "DLCache.h"
#include "PixelShaderManager.h"
#include "ProgramShaderCache.h"
#include "VertexShaderManager.h"
#include "VertexLoaderManager.h"
#include "VertexLoader.h"
#include "PostProcessing.h"
#include "TextureConverter.h"
#include "OnScreenDisplay.h"
#include "Timer.h"
#include "StringUtil.h"
#include "FramebufferManager.h"
#include "Fifo.h"
#include "Debugger.h"
#include "Core.h"
#include "Movie.h"
#include "Host.h"
#include "BPFunctions.h"
#include "FPSCounter.h"
#include "ConfigManager.h"
#include "VertexManager.h"
#include "SamplerCache.h"

#include "main.h" // Local
#ifdef _WIN32
#include "EmuWindow.h"
#endif
#if defined _WIN32 || defined HAVE_LIBAV
#include "AVIDump.h"
#endif

#if defined(HAVE_WX) && HAVE_WX
#include <wx/image.h>
#endif



void VideoConfig::UpdateProjectionHack()
{
	::UpdateProjectionHack(g_Config.iPhackvalue, g_Config.sPhackvalue);
}


#if defined(HAVE_WX) && HAVE_WX
// Screenshot thread struct
typedef struct
{
	int W, H;
	std::string filename;
	wxImage *img;
} ScrStrct;
#endif


int OSDInternalW, OSDInternalH;

namespace OGL
{

// Declarations and definitions
// ----------------------------
static int s_fps = 0;
static GLuint s_ShowEFBCopyRegions_VBO = 0;
static GLuint s_ShowEFBCopyRegions_VAO = 0;
static SHADER s_ShowEFBCopyRegions;

static RasterFont* s_pfont = NULL;

// 1 for no MSAA. Use s_MSAASamples > 1 to check for MSAA.
static int s_MSAASamples = 1;
static int s_MSAACoverageSamples = 0;
static int s_LastMultisampleMode = 0;

static bool s_bHaveCoverageMSAA = false;
static u32 s_blendMode;

#if defined(HAVE_WX) && HAVE_WX
static std::thread scrshotThread;
#endif

// EFB cache related
static const u32 EFB_CACHE_RECT_SIZE = 64; // Cache 64x64 blocks.
static const u32 EFB_CACHE_WIDTH = (EFB_WIDTH + EFB_CACHE_RECT_SIZE - 1) / EFB_CACHE_RECT_SIZE; // round up
static const u32 EFB_CACHE_HEIGHT = (EFB_HEIGHT + EFB_CACHE_RECT_SIZE - 1) / EFB_CACHE_RECT_SIZE;
static bool s_efbCacheValid[2][EFB_CACHE_WIDTH * EFB_CACHE_HEIGHT];
static std::vector<u32> s_efbCache[2][EFB_CACHE_WIDTH * EFB_CACHE_HEIGHT]; // 2 for PEEK_Z and PEEK_COLOR

static SamplerCache s_sampler_cache;

int GetNumMSAASamples(int MSAAMode)
{
	switch (MSAAMode)
	{
		case MULTISAMPLE_OFF:
			return 1;

		case MULTISAMPLE_2X:
			return 2;

		case MULTISAMPLE_4X:
		case MULTISAMPLE_CSAA_8X:
		case MULTISAMPLE_CSAA_16X:
			return 4;

		case MULTISAMPLE_8X:
		case MULTISAMPLE_CSAA_8XQ:
		case MULTISAMPLE_CSAA_16XQ:
			return 8;

		default:
			return 1;
	}
}

int GetNumMSAACoverageSamples(int MSAAMode)
{
	if (!s_bHaveCoverageMSAA)
		return 0;

	switch (g_ActiveConfig.iMultisampleMode)
	{
		case MULTISAMPLE_CSAA_8X:
		case MULTISAMPLE_CSAA_8XQ:
			return 8;

		case MULTISAMPLE_CSAA_16X:
		case MULTISAMPLE_CSAA_16XQ:
			return 16;

		default:
			return 0;
	}
}

// Init functions
Renderer::Renderer()
{
	OSDInternalW = 0;
	OSDInternalH = 0;

	s_fps=0;
	s_ShowEFBCopyRegions_VBO = 0;
	s_blendMode = 0;
	InitFPSCounter();

	OSD::AddMessage(StringFromFormat("Video Info: %s, %s, %s",
				glGetString(GL_VENDOR),
				glGetString(GL_RENDERER),
				glGetString(GL_VERSION)).c_str(), 5000);

	bool bSuccess = true;
	GLint numvertexattribs = 0;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numvertexattribs);
	if (numvertexattribs < 16)
	{
		ERROR_LOG(VIDEO, "GPU: OGL ERROR: Number of attributes %d not enough.\n"
				"GPU: Does your video card support OpenGL 2.x?",
				numvertexattribs);
		bSuccess = false;
	}

	// Init extension support.
#ifdef __APPLE__
	glewExperimental = 1;
#endif
	if (glewInit() != GLEW_OK)
	{
		ERROR_LOG(VIDEO, "glewInit() failed! Does your video card support OpenGL 2.x?");
		return;	// TODO: fail
	}

	if (!GLEW_EXT_secondary_color)
	{
		ERROR_LOG(VIDEO, "GPU: OGL ERROR: Need GL_EXT_secondary_color.\n"
				"GPU: Does your video card support OpenGL 2.x?");
		bSuccess = false;
	}

	if (!GLEW_ARB_framebuffer_object)
	{
		ERROR_LOG(VIDEO, "GPU: ERROR: Need GL_ARB_framebufer_object for multiple render targets.\n"
				"GPU: Does your video card support OpenGL 3.0?");
		bSuccess = false;
	}

	if (!GLEW_ARB_vertex_array_object)
	{
		ERROR_LOG(VIDEO, "GPU: OGL ERROR: Need GL_ARB_vertex_array_object.\n"
				"GPU: Does your video card support OpenGL 3.0?");
		bSuccess = false;
	}
	
	if (!GLEW_ARB_map_buffer_range)
	{
		ERROR_LOG(VIDEO, "GPU: OGL ERROR: Need GL_ARB_map_buffer_range.\n"
				"GPU: Does your video card support OpenGL 3.0?");
		bSuccess = false;
	}

	if (!GLEW_ARB_draw_elements_base_vertex)
	{
		ERROR_LOG(VIDEO, "GPU: OGL ERROR: Need GL_ARB_draw_elements_base_vertex.\n"
				"GPU: Does your video card support OpenGL 3.2?");
		bSuccess = false;
	}

	if (!GLEW_ARB_sampler_objects)
	{
		ERROR_LOG(VIDEO, "GPU: OGL ERROR: Need GL_ARB_sampler_objects.");
		bSuccess = false;
	}

	s_bHaveCoverageMSAA = GLEW_NV_framebuffer_multisample_coverage;
	
	g_Config.backend_info.bSupportsDualSourceBlend = GLEW_ARB_blend_func_extended;
	
	g_Config.backend_info.bSupportsGLSLUBO = GLEW_ARB_uniform_buffer_object;
	
	g_Config.backend_info.bSupportsGLPinnedMemory = GLEW_AMD_pinned_memory;
	
	g_Config.backend_info.bSupportsGLSync = GLEW_ARB_sync;
	
	g_Config.backend_info.bSupportsGLSLCache = GLEW_ARB_get_program_binary;

	UpdateActiveConfig();
	OSD::AddMessage(StringFromFormat("Supports: %s%s%s%s- Missing: %s%s%s%s",
			g_ActiveConfig.backend_info.bSupportsDualSourceBlend ? "DualSourceBlend " : "",
			g_ActiveConfig.backend_info.bSupportsGLSLUBO ? "UniformBuffer " : "",
			g_ActiveConfig.backend_info.bSupportsGLPinnedMemory ? "PinnedMemory " : "",
			g_ActiveConfig.backend_info.bSupportsGLSLCache ? "ShaderCache " : "",
			g_ActiveConfig.backend_info.bSupportsDualSourceBlend ? "" : "DualSourceBlend ",
			g_ActiveConfig.backend_info.bSupportsGLSLUBO ? "" : "UniformBuffer ",
			g_ActiveConfig.backend_info.bSupportsGLPinnedMemory ? "" : "PinnedMemory ",
			g_ActiveConfig.backend_info.bSupportsGLSLCache ? "" : "ShaderCache "
			).c_str(), 5000);
			
	s_LastMultisampleMode = g_ActiveConfig.iMultisampleMode;
	s_MSAASamples = GetNumMSAASamples(s_LastMultisampleMode);
	s_MSAACoverageSamples = GetNumMSAACoverageSamples(s_LastMultisampleMode);

	if (!bSuccess)
		return;	// TODO: fail

	// Decide frambuffer size
	s_backbuffer_width = (int)GLInterface->GetBackBufferWidth();
	s_backbuffer_height = (int)GLInterface->GetBackBufferHeight();

	// Handle VSync on/off
	int swapInterval = g_ActiveConfig.bVSync ? 1 : 0;
	GLInterface->SwapInterval(swapInterval);

	// check the max texture width and height
	GLint max_texture_size;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&max_texture_size);
	if (max_texture_size < 1024)
		ERROR_LOG(VIDEO, "GL_MAX_TEXTURE_SIZE too small at %i - must be at least 1024.",
				max_texture_size);

	if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

	if (!GLEW_ARB_texture_non_power_of_two)
		WARN_LOG(VIDEO, "ARB_texture_non_power_of_two not supported.");

	// TODO: Move these somewhere else?
	FramebufferManagerBase::SetLastXfbWidth(MAX_XFB_WIDTH);
	FramebufferManagerBase::SetLastXfbHeight(MAX_XFB_HEIGHT);

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

	s_LastEFBScale = g_ActiveConfig.iEFBScale;
	CalculateTargetSize(s_backbuffer_width, s_backbuffer_height);

	// Because of the fixed framebuffer size we need to disable the resolution
	// options while running
	g_Config.bRunning = true;

	if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;

	// Initialize the FramebufferManager
	g_framebuffer_manager = new FramebufferManager(s_target_width, s_target_height,
			s_MSAASamples, s_MSAACoverageSamples);

	if (GL_REPORT_ERROR() != GL_NO_ERROR)
		bSuccess = false;
	
	glStencilFunc(GL_ALWAYS, 0, 0);
	glBlendFunc(GL_ONE, GL_ONE);

	glViewport(0, 0, GetTargetWidth(), GetTargetHeight()); // Reset The Current Viewport

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // 4-byte pixel alignment

	glDisable(GL_STENCIL_TEST);
	glEnable(GL_SCISSOR_TEST);

	glScissor(0, 0, GetTargetWidth(), GetTargetHeight());
	glBlendColor(0, 0, 0, 0.5f);
	glClearDepth(1.0f);

	UpdateActiveConfig();
}

Renderer::~Renderer()
{

#if defined(HAVE_WX) && HAVE_WX
	if (scrshotThread.joinable())
		scrshotThread.join();
#endif

	delete g_framebuffer_manager;
}

void Renderer::Shutdown()
{
	g_Config.bRunning = false;
	UpdateActiveConfig();
	
	glDeleteBuffers(1, &s_ShowEFBCopyRegions_VBO);
	glDeleteVertexArrays(1, &s_ShowEFBCopyRegions_VAO);
	s_ShowEFBCopyRegions_VBO = 0;
	
	delete s_pfont;
	s_pfont = 0;
	s_ShowEFBCopyRegions.Destroy();
	
	s_sampler_cache.Clear();
}

void Renderer::Init()
{
	s_pfont = new RasterFont();
	
	ProgramShaderCache::CompileShader(s_ShowEFBCopyRegions, 
		"in vec2 rawpos;\n"
		"in vec3 color0;\n"
		"out vec4 c;\n"
		"void main(void) {\n"
		"	gl_Position = vec4(rawpos,0,1);\n"
		"	c = vec4(color0, 1.0);\n"
		"}\n",
		"in vec4 c;\n"
		"out vec4 ocol0;\n"
		"void main(void) {\n"
		"	ocol0 = c;\n"
		"}\n");
	
	// creating buffers
	glGenBuffers(1, &s_ShowEFBCopyRegions_VBO);
	glGenVertexArrays(1, &s_ShowEFBCopyRegions_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, s_ShowEFBCopyRegions_VBO);
	glBindVertexArray( s_ShowEFBCopyRegions_VAO );
	glEnableVertexAttribArray(SHADER_POSITION_ATTRIB);
	glVertexAttribPointer(SHADER_POSITION_ATTRIB, 2, GL_FLOAT, 0, sizeof(GLfloat)*5, NULL);
	glEnableVertexAttribArray(SHADER_COLOR0_ATTRIB);
	glVertexAttribPointer(SHADER_COLOR0_ATTRIB, 3, GL_FLOAT, 0, sizeof(GLfloat)*5, (GLfloat*)NULL+2);
}

// Create On-Screen-Messages
void Renderer::DrawDebugInfo()
{
	// Reset viewport for drawing text
	glViewport(0, 0, GLInterface->GetBackBufferWidth(), GLInterface->GetBackBufferHeight());
	// Draw various messages on the screen, like FPS, statistics, etc.
	char debugtext_buffer[8192];
	char *p = debugtext_buffer;
	p[0] = 0;

	if (g_ActiveConfig.bShowFPS)
		p+=sprintf(p, "FPS: %d\n", s_fps);

	if (SConfig::GetInstance().m_ShowLag)
		p+=sprintf(p, "Lag: %llu\n", Movie::g_currentLagCount);

	if (g_ActiveConfig.bShowInputDisplay)
		p+=sprintf(p, "%s", Movie::GetInputDisplay().c_str());

	if (g_ActiveConfig.bShowEFBCopyRegions)
	{
		// Store Line Size
		GLfloat lSize;
		glGetFloatv(GL_LINE_WIDTH, &lSize);

		// Set Line Size
		glLineWidth(3.0f);

		// 2*Coords + 3*Color
		glBindBuffer(GL_ARRAY_BUFFER, s_ShowEFBCopyRegions_VBO);
		glBufferData(GL_ARRAY_BUFFER, stats.efb_regions.size() * sizeof(GLfloat) * (2+3)*2*6, NULL, GL_STREAM_DRAW);
		GLfloat *Vertices = (GLfloat*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

		// Draw EFB copy regions rectangles
		int a = 0;
		GLfloat color[3] = {0.0f, 1.0f, 1.0f};
		
		for (std::vector<EFBRectangle>::const_iterator it = stats.efb_regions.begin();
			it != stats.efb_regions.end(); ++it)
		{
			GLfloat halfWidth = EFB_WIDTH / 2.0f;
			GLfloat halfHeight = EFB_HEIGHT / 2.0f;
			GLfloat x =  (GLfloat) -1.0f + ((GLfloat)it->left / halfWidth);
			GLfloat y =  (GLfloat) 1.0f - ((GLfloat)it->top / halfHeight);
			GLfloat x2 = (GLfloat) -1.0f + ((GLfloat)it->right / halfWidth);
			GLfloat y2 = (GLfloat) 1.0f - ((GLfloat)it->bottom / halfHeight);

			Vertices[a++] = x;
			Vertices[a++] = y;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			Vertices[a++] = x2;
			Vertices[a++] = y;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			
			Vertices[a++] = x2;
			Vertices[a++] = y;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			Vertices[a++] = x2;
			Vertices[a++] = y2;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			
			Vertices[a++] = x2;
			Vertices[a++] = y2;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			Vertices[a++] = x;
			Vertices[a++] = y2;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			
			Vertices[a++] = x;
			Vertices[a++] = y2;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			Vertices[a++] = x;
			Vertices[a++] = y;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			
			Vertices[a++] = x;
			Vertices[a++] = y;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			Vertices[a++] = x2;
			Vertices[a++] = y2;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			
			Vertices[a++] = x2;
			Vertices[a++] = y;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			Vertices[a++] = x;
			Vertices[a++] = y2;
			Vertices[a++] = color[0];
			Vertices[a++] = color[1];
			Vertices[a++] = color[2];
			
			// TO DO: build something nicer here
			GLfloat temp = color[0];
			color[0] = color[1];
			color[1] = color[2];
			color[2] = temp;
		}
		glUnmapBuffer(GL_ARRAY_BUFFER);
		
		s_ShowEFBCopyRegions.Bind();
		glBindVertexArray( s_ShowEFBCopyRegions_VAO );
		glDrawArrays(GL_LINES, 0, stats.efb_regions.size() * 2*6);

		// Restore Line Size
		glLineWidth(lSize);

		// Clear stored regions
		stats.efb_regions.clear();
	}

	if (g_ActiveConfig.bOverlayStats)
		p = Statistics::ToString(p);

	if (g_ActiveConfig.bOverlayProjStats)
		p = Statistics::ToStringProj(p);

	// Render a shadow, and then the text.
	if (p != debugtext_buffer)
	{
		Renderer::RenderText(debugtext_buffer, 21, 21, 0xDD000000);
		Renderer::RenderText(debugtext_buffer, 20, 20, 0xFF00FFFF);
	}
}

void Renderer::RenderText(const char *text, int left, int top, u32 color)
{
	const int nBackbufferWidth = (int)GLInterface->GetBackBufferWidth();
	const int nBackbufferHeight = (int)GLInterface->GetBackBufferHeight();

	s_pfont->printMultilineText(text,
		left * 2.0f / (float)nBackbufferWidth - 1,
		1 - top * 2.0f / (float)nBackbufferHeight,
		0, nBackbufferWidth, nBackbufferHeight, color);

	GL_REPORT_ERRORD();
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
void Renderer::SetScissorRect(const TargetRectangle& rc)
{
	glScissor(rc.left, rc.bottom, rc.GetWidth(), rc.GetHeight());
}

void Renderer::SetColorMask()
{
	// Only enable alpha channel if it's supported by the current EFB format
	GLenum ColorMask = GL_FALSE, AlphaMask = GL_FALSE;
	if (bpmem.alpha_test.TestResult() != AlphaTest::FAIL)
	{
		if (bpmem.blendmode.colorupdate)
			ColorMask = GL_TRUE;
		if (bpmem.blendmode.alphaupdate && (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24))
			AlphaMask = GL_TRUE;
	}
	glColorMask(ColorMask,  ColorMask,  ColorMask,  AlphaMask);
}

void ClearEFBCache()
{
	for (u32 i = 0; i < EFB_CACHE_WIDTH * EFB_CACHE_HEIGHT; ++i)
		s_efbCacheValid[0][i] = false;

	for (u32 i = 0; i < EFB_CACHE_WIDTH * EFB_CACHE_HEIGHT; ++i)
		s_efbCacheValid[1][i] = false;
}

void Renderer::UpdateEFBCache(EFBAccessType type, u32 cacheRectIdx, const EFBRectangle& efbPixelRc, const TargetRectangle& targetPixelRc, const u32* data)
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
			s_efbCache[cacheType][cacheRectIdx][yCache * EFB_CACHE_RECT_SIZE + xCache] = data[yData * targetPixelRcWidth + xData];
		}
	}

	s_efbCacheValid[cacheType][cacheRectIdx] = true;
}

// This function allows the CPU to directly access the EFB.
// There are EFB peeks (which will read the color or depth of a pixel)
// and EFB pokes (which will change the color or depth of a pixel).
//
// The behavior of EFB peeks can only be modified by:
//	- GX_PokeAlphaRead
// The behavior of EFB pokes can be modified by:
//	- GX_PokeAlphaMode (TODO)
//	- GX_PokeAlphaUpdate (TODO)
//	- GX_PokeBlendMode (TODO)
//	- GX_PokeColorUpdate (TODO)
//	- GX_PokeDither (TODO)
//	- GX_PokeDstAlpha (TODO)
//	- GX_PokeZMode (TODO)
u32 Renderer::AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data)
{
	if (!g_ActiveConfig.bEFBAccessEnable)
		return 0;

	u32 cacheRectIdx = (y / EFB_CACHE_RECT_SIZE) * EFB_CACHE_WIDTH
	                 + (x / EFB_CACHE_RECT_SIZE);

	// Get the rectangular target region containing the EFB pixel
	EFBRectangle efbPixelRc;
	efbPixelRc.left = (x / EFB_CACHE_RECT_SIZE) * EFB_CACHE_RECT_SIZE;
	efbPixelRc.top = (y / EFB_CACHE_RECT_SIZE) * EFB_CACHE_RECT_SIZE;
	efbPixelRc.right = std::min(efbPixelRc.left + EFB_CACHE_RECT_SIZE, (u32)EFB_WIDTH);
	efbPixelRc.bottom = std::min(efbPixelRc.top + EFB_CACHE_RECT_SIZE, (u32)EFB_HEIGHT);

	TargetRectangle targetPixelRc = ConvertEFBRectangle(efbPixelRc);
	u32 targetPixelRcWidth = targetPixelRc.right - targetPixelRc.left;
	u32 targetPixelRcHeight = targetPixelRc.top - targetPixelRc.bottom;

	// TODO (FIX) : currently, AA path is broken/offset and doesn't return the correct pixel
	switch (type)
	{
	case PEEK_Z:
		{
			u32 z;

			if (!s_efbCacheValid[0][cacheRectIdx])
			{
				if (s_MSAASamples > 1)
				{
					// Resolve our rectangle.
					FramebufferManager::GetEFBDepthTexture(efbPixelRc);
					glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferManager::GetResolvedFramebuffer());
				}

				u32* depthMap = new u32[targetPixelRcWidth * targetPixelRcHeight];

				glReadPixels(targetPixelRc.left, targetPixelRc.bottom, targetPixelRcWidth, targetPixelRcHeight,
				             GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, depthMap);
				GL_REPORT_ERRORD();

				UpdateEFBCache(type, cacheRectIdx, efbPixelRc, targetPixelRc, depthMap);

				delete[] depthMap;
			}

			u32 xRect = x % EFB_CACHE_RECT_SIZE;
			u32 yRect = y % EFB_CACHE_RECT_SIZE;
			z = s_efbCache[0][cacheRectIdx][yRect * EFB_CACHE_RECT_SIZE + xRect];

			// Scale the 32-bit value returned by glReadPixels to a 24-bit
			// value (GC uses a 24-bit Z-buffer).
			// TODO: in RE0 this value is often off by one, which causes lighting to disappear
			if(bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16)
			{
				// if Z is in 16 bit format you must return a 16 bit integer
				z = z >> 16;
			}
			else
			{
				z = z >> 8;
			}
			return z;
		}

	case PEEK_COLOR: // GXPeekARGB
		{
			// Although it may sound strange, this really is A8R8G8B8 and not RGBA or 24-bit...

			// Tested in Killer 7, the first 8bits represent the alpha value which is used to
			// determine if we're aiming at an enemy (0x80 / 0x88) or not (0x70)
			// Wind Waker is also using it for the pictograph to determine the color of each pixel

			u32 color;

			if (!s_efbCacheValid[1][cacheRectIdx])
			{
				if (s_MSAASamples > 1)
				{
					// Resolve our rectangle.
					FramebufferManager::GetEFBColorTexture(efbPixelRc);
					glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferManager::GetResolvedFramebuffer());
				}

				u32* colorMap = new u32[targetPixelRcWidth * targetPixelRcHeight];

				glReadPixels(targetPixelRc.left, targetPixelRc.bottom, targetPixelRcWidth, targetPixelRcHeight,
				             GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, colorMap);
				GL_REPORT_ERRORD();

				UpdateEFBCache(type, cacheRectIdx, efbPixelRc, targetPixelRc, colorMap);

				delete[] colorMap;
			}

			u32 xRect = x % EFB_CACHE_RECT_SIZE;
			u32 yRect = y % EFB_CACHE_RECT_SIZE;
			color = s_efbCache[1][cacheRectIdx][yRect * EFB_CACHE_RECT_SIZE + xRect];

			// check what to do with the alpha channel (GX_PokeAlphaRead)
			PixelEngine::UPEAlphaReadReg alpha_read_mode;
			PixelEngine::Read16((u16&)alpha_read_mode, PE_ALPHAREAD);

			if (bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24)
			{
				color = RGBA8ToRGBA6ToRGBA8(color);
			}
			else if (bpmem.zcontrol.pixel_format == PIXELFMT_RGB565_Z16)
			{
				color = RGBA8ToRGB565ToRGBA8(color);
			}
			if(bpmem.zcontrol.pixel_format != PIXELFMT_RGBA6_Z24)
			{
				color |= 0xFF000000;
			}
			if(alpha_read_mode.ReadMode == 2) return color; // GX_READ_NONE
			else if(alpha_read_mode.ReadMode == 1) return (color | 0xFF000000); // GX_READ_FF
			else /*if(alpha_read_mode.ReadMode == 0)*/ return (color & 0x00FFFFFF); // GX_READ_00
		}

	case POKE_COLOR:
	case POKE_Z:
		// TODO: Implement. One way is to draw a tiny pixel-sized rectangle at
		// the exact location. Note: EFB pokes are susceptible to Z-buffering
		// and perhaps blending.
		//WARN_LOG(VIDEOINTERFACE, "This is probably some kind of software rendering");
		break;

	default:
		break;
	}

	return 0;
}

// Called from VertexShaderManager
void Renderer::UpdateViewport(Matrix44& vpCorrection)
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
	int X = EFBToScaledX((int)ceil(xfregs.viewport.xOrig - xfregs.viewport.wd - (float)scissorXOff));
	int Y = EFBToScaledY((int)ceil((float)EFB_HEIGHT - xfregs.viewport.yOrig + xfregs.viewport.ht + (float)scissorYOff));
	int Width = EFBToScaledX((int)ceil(2.0f * xfregs.viewport.wd));
	int Height = EFBToScaledY((int)ceil(-2.0f * xfregs.viewport.ht));
	double GLNear = (xfregs.viewport.farZ - xfregs.viewport.zRange) / 16777216.0f;
	double GLFar = xfregs.viewport.farZ / 16777216.0f;
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

	// OpenGL does not require any viewport correct
	Matrix44::LoadIdentity(vpCorrection);

	// Update the view port
	glViewport(X, Y, Width, Height);
	glDepthRange(GLNear, GLFar);
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

	glClearDepth(float(z & 0xFFFFFF) / float(0xFFFFFF));

	// Update rect for clearing the picture
	glEnable(GL_SCISSOR_TEST);

	TargetRectangle const targetRc = ConvertEFBRectangle(rc);
	glScissor(targetRc.left, targetRc.bottom, targetRc.GetWidth(), targetRc.GetHeight());

	// glColorMask/glDepthMask/glScissor affect glClear (glViewport does not)
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	RestoreAPIState();

	ClearEFBCache();
}

void Renderer::ReinterpretPixelData(unsigned int convtype)
{
	// TODO
}

void Renderer::SetBlendMode(bool forceUpdate)
{
	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate
		&& bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;
	bool useDualSource = useDstAlpha && g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
	
	// Our render target always uses an alpha channel, so we need to override the blend functions to assume a destination alpha of 1 if the render target isn't supposed to have an alpha channel
	// Example: D3DBLEND_DESTALPHA needs to be D3DBLEND_ONE since the result without an alpha channel is assumed to always be 1.
	bool target_has_alpha = bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;
	const GLenum glSrcFactors[8] =
	{
		GL_ZERO,
		GL_ONE,
		GL_DST_COLOR,
		GL_ONE_MINUS_DST_COLOR,
		GL_SRC_ALPHA,
		GL_ONE_MINUS_SRC_ALPHA, // NOTE: If dual-source blending is enabled, use SRC1_ALPHA
		(target_has_alpha) ? GL_DST_ALPHA : (GLenum)GL_ONE,
		(target_has_alpha) ? GL_ONE_MINUS_DST_ALPHA : (GLenum)GL_ZERO
	};
	const GLenum glDestFactors[8] =
	{
		GL_ZERO,
		GL_ONE,
		GL_SRC_COLOR,
		GL_ONE_MINUS_SRC_COLOR,
		GL_SRC_ALPHA,
		GL_ONE_MINUS_SRC_ALPHA, // NOTE: If dual-source blending is enabled, use SRC1_ALPHA
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
	{
		// subtract enable change
		GLenum equation = newval & 4 ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD;
		GLenum equationAlpha = useDualSource ? GL_FUNC_ADD : equation;
		
		glBlendEquationSeparate(equation, equationAlpha);
	}

	if (changes & 0x1FA)
	{
		GLenum srcFactor = glSrcFactors[(newval >> 3) & 7];
		GLenum dstFactor = glDestFactors[(newval >> 6) & 7];
		GLenum srcFactorAlpha = srcFactor;
		GLenum dstFactorAlpha = dstFactor;
		if (useDualSource)
		{
			srcFactorAlpha = GL_ONE;
			dstFactorAlpha = GL_ZERO;

			if (srcFactor == GL_SRC_ALPHA)
				srcFactor = GL_SRC1_ALPHA;
			else if (srcFactor == GL_ONE_MINUS_SRC_ALPHA)
				srcFactor = GL_ONE_MINUS_SRC1_ALPHA;

			if (dstFactor == GL_SRC_ALPHA)
				dstFactor = GL_SRC1_ALPHA;
			else if (dstFactor == GL_ONE_MINUS_SRC_ALPHA)
				dstFactor = GL_ONE_MINUS_SRC1_ALPHA;
		}
		
		// blend RGB change
		glBlendFuncSeparate(srcFactor, dstFactor, srcFactorAlpha, dstFactorAlpha);
	}

	s_blendMode = newval;
}

// This function has the final picture. We adjust the aspect ratio here.
void Renderer::Swap(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight,const EFBRectangle& rc,float Gamma)
{
	static int w = 0, h = 0;
	if (g_bSkipCurrentFrame || (!XFBWrited && !g_ActiveConfig.RealXFBEnabled()) || !fbWidth || !fbHeight)
	{
		if (g_ActiveConfig.bDumpFrames && frame_data)
		{
#ifdef _WIN32
			AVIDump::AddFrame(frame_data);
#elif defined HAVE_LIBAV
			AVIDump::AddFrame((u8*)frame_data, w, h);
#endif
		}
		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	if (field == FIELD_LOWER) xfbAddr -= fbWidth * 2;
	u32 xfbCount = 0;
	const XFBSourceBase* const* xfbSourceList = FramebufferManager::GetXFBSource(xfbAddr, fbWidth, fbHeight, xfbCount);
	if (g_ActiveConfig.VirtualXFBEnabled() && (!xfbSourceList || xfbCount == 0))
	{
		if (g_ActiveConfig.bDumpFrames && frame_data)
		{
#ifdef _WIN32
			AVIDump::AddFrame(frame_data);
#elif defined HAVE_LIBAV
			AVIDump::AddFrame((u8*)frame_data, w, h);
#endif
		}
		Core::Callback_VideoCopiedToXFB(false);
		return;
	}

	ResetAPIState();

	UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);
	TargetRectangle flipped_trc = GetTargetRectangle();

	// Flip top and bottom for some reason; TODO: Fix the code to suck less?
	int tmp = flipped_trc.top;
	flipped_trc.top = flipped_trc.bottom;
	flipped_trc.bottom = tmp;

	// Textured triangles are necessary because of post-processing shaders

	// Disable all other stages
	for (int i = 0; i < 8; ++i)
		OGL::TextureCache::DisableStage(i);

	// Update GLViewPort
	glViewport(flipped_trc.left, flipped_trc.bottom, flipped_trc.GetWidth(), flipped_trc.GetHeight());

	GL_REPORT_ERRORD();

	// We must call ApplyShader here even if no post proc is selected - it takes
	// care of disabling it in that case. It returns false in case of no post processing.
	//bool applyShader = PostProcessing::ApplyShader();
	// degasus: disabled for blitting
	
	// Copy the framebuffer to screen.

	const XFBSourceBase* xfbSource = NULL;

	if(g_ActiveConfig.bUseXFB)
	{
		// Render to the real buffer now.
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // switch to the window backbuffer
		
		// draw each xfb source
		glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferManager::GetXFBFramebuffer());

		for (u32 i = 0; i < xfbCount; ++i)
		{
			xfbSource = xfbSourceList[i];

			MathUtil::Rectangle<float> drawRc;

			if (g_ActiveConfig.bUseRealXFB)
			{
				drawRc.top = flipped_trc.top;
				drawRc.bottom = flipped_trc.bottom;
				drawRc.left = flipped_trc.left;
				drawRc.right = flipped_trc.right;
			}
			else
			{
				// use virtual xfb with offset
				int xfbHeight = xfbSource->srcHeight;
				int xfbWidth = xfbSource->srcWidth;
				int hOffset = ((s32)xfbSource->srcAddr - (s32)xfbAddr) / ((s32)fbWidth * 2);
				
				drawRc.top = flipped_trc.bottom + (hOffset + xfbHeight) * flipped_trc.GetHeight() / fbHeight;
				drawRc.bottom = flipped_trc.bottom + hOffset * flipped_trc.GetHeight() / fbHeight;
				drawRc.left = flipped_trc.left + (flipped_trc.GetWidth() - xfbWidth * flipped_trc.GetWidth() / fbWidth)/2;
				drawRc.right = flipped_trc.left + (flipped_trc.GetWidth() + xfbWidth * flipped_trc.GetWidth() / fbWidth)/2;
				
				// The following code disables auto stretch.  Kept for reference.
				// scale draw area for a 1 to 1 pixel mapping with the draw target
				//float vScale = (float)fbHeight / (float)flipped_trc.GetHeight();
				//float hScale = (float)fbWidth / (float)flipped_trc.GetWidth();
				//drawRc.top *= vScale;
				//drawRc.bottom *= vScale;
				//drawRc.left *= hScale;
				//drawRc.right *= hScale;
			}
			// Tell the OSD Menu about the current internal resolution
			OSDInternalW = xfbSource->sourceRc.GetWidth(); OSDInternalH = xfbSource->sourceRc.GetHeight();

			MathUtil::Rectangle<float> sourceRc;
			sourceRc.left = xfbSource->sourceRc.left;
			sourceRc.right = xfbSource->sourceRc.right;
			sourceRc.top = xfbSource->sourceRc.top;
			sourceRc.bottom = xfbSource->sourceRc.bottom;

			xfbSource->Draw(sourceRc, drawRc, 0, 0);
		}
	}
	else
	{
		TargetRectangle targetRc = ConvertEFBRectangle(rc);
		
		// for msaa mode, we must resolve the efb content to non-msaa
		FramebufferManager::ResolveAndGetRenderTarget(rc);
		
		// Render to the real buffer now. (resolve have changed this in msaa mode)
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		
		// always the non-msaa fbo
		GLuint fb = s_MSAASamples>1?FramebufferManager::GetResolvedFramebuffer():FramebufferManager::GetEFBFramebuffer();
			
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
		glBlitFramebuffer(targetRc.left, targetRc.bottom, targetRc.right, targetRc.top,
			flipped_trc.left, flipped_trc.bottom, flipped_trc.right, flipped_trc.top,
			GL_COLOR_BUFFER_BIT, GL_LINEAR);
	}

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	// Save screenshot
	if (s_bScreenshot)
	{
		std::lock_guard<std::mutex> lk(s_criticalScreenshot);
		SaveScreenshot(s_sScreenshotName, flipped_trc);
		// Reset settings
		s_sScreenshotName.clear();
		s_bScreenshot = false;
	}

	// Frame dumps are handled a little differently in Windows
#if defined _WIN32 || defined HAVE_LIBAV
	if (g_ActiveConfig.bDumpFrames)
	{
		std::lock_guard<std::mutex> lk(s_criticalScreenshot);
		if (!frame_data || w != flipped_trc.GetWidth() ||
		             h != flipped_trc.GetHeight())
		{
			if (frame_data) delete[] frame_data;
			w = flipped_trc.GetWidth();
			h = flipped_trc.GetHeight();
			frame_data = new char[3 * w * h];
		}
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(flipped_trc.left, flipped_trc.bottom, w, h, GL_BGR, GL_UNSIGNED_BYTE, frame_data);
		if (GL_REPORT_ERROR() == GL_NO_ERROR && w > 0 && h > 0)
		{
			if (!bLastFrameDumped)
			{
				#ifdef _WIN32
					bAVIDumping = AVIDump::Start(EmuWindow::GetParentWnd(), w, h);
				#else
					bAVIDumping = AVIDump::Start(w, h);
				#endif
				if (!bAVIDumping)
					OSD::AddMessage("AVIDump Start failed", 2000);
				else
				{
					OSD::AddMessage(StringFromFormat(
								"Dumping Frames to \"%sframedump0.avi\" (%dx%d RGB24)",
								File::GetUserPath(D_DUMPFRAMES_IDX).c_str(), w, h).c_str(), 2000);
				}
			}
			if (bAVIDumping)
			{
				#ifdef _WIN32
					AVIDump::AddFrame(frame_data);
				#else
					FlipImageData((u8*)frame_data, w, h);
					AVIDump::AddFrame((u8*)frame_data, w, h);
				#endif
			}

			bLastFrameDumped = true;
		}
		else
			NOTICE_LOG(VIDEO, "Error reading framebuffer");
	}
	else
	{
		if (bLastFrameDumped && bAVIDumping)
		{
			if (frame_data)
			{
				delete[] frame_data;
				frame_data = NULL;
				w = h = 0;
			}
			AVIDump::Stop();
			bAVIDumping = false;
			OSD::AddMessage("Stop dumping frames", 2000);
		}
		bLastFrameDumped = false;
	}
#else
	if (g_ActiveConfig.bDumpFrames)
	{
		std::lock_guard<std::mutex> lk(s_criticalScreenshot);
		std::string movie_file_name;
		w = GetTargetRectangle().GetWidth();
		h = GetTargetRectangle().GetHeight();
		frame_data = new char[3 * w * h];
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(GetTargetRectangle().left, GetTargetRectangle().bottom, w, h, GL_BGR, GL_UNSIGNED_BYTE, frame_data);
		if (GL_REPORT_ERROR() == GL_NO_ERROR)
		{
			if (!bLastFrameDumped)
			{
				movie_file_name = File::GetUserPath(D_DUMPFRAMES_IDX) + "framedump.raw";
				pFrameDump.Open(movie_file_name, "wb");
				if (!pFrameDump)
					OSD::AddMessage("Error opening framedump.raw for writing.", 2000);
				else
				{
					char msg [255];
					sprintf(msg, "Dumping Frames to \"%s\" (%dx%d RGB24)", movie_file_name.c_str(), w, h);
					OSD::AddMessage(msg, 2000);
				}
			}
			if (pFrameDump)
			{
				FlipImageData((u8*)frame_data, w, h);
				pFrameDump.WriteBytes(frame_data, w * 3 * h);
				pFrameDump.Flush();
			}
			bLastFrameDumped = true;
		}

		delete[] frame_data;
	}
	else
	{
		if (bLastFrameDumped)
			pFrameDump.Close();
		bLastFrameDumped = false;
	}
#endif

	// Finish up the current frame, print some stats

	SetWindowSize(fbWidth, fbHeight);

	GLInterface->Update(); // just updates the render window position and the backbuffer size

	bool xfbchanged = false;

	if (FramebufferManagerBase::LastXfbWidth() != fbWidth || FramebufferManagerBase::LastXfbHeight() != fbHeight)
	{
		xfbchanged = true;
		unsigned int const last_w = (fbWidth < 1 || fbWidth > MAX_XFB_WIDTH) ? MAX_XFB_WIDTH : fbWidth;
		unsigned int const last_h = (fbHeight < 1 || fbHeight > MAX_XFB_HEIGHT) ? MAX_XFB_HEIGHT : fbHeight;
		FramebufferManagerBase::SetLastXfbWidth(last_w);
		FramebufferManagerBase::SetLastXfbHeight(last_h);
	}

	bool WindowResized = false;
	int W = (int)GLInterface->GetBackBufferWidth();
	int H = (int)GLInterface->GetBackBufferHeight();
	if (W != s_backbuffer_width || H != s_backbuffer_height || s_LastEFBScale != g_ActiveConfig.iEFBScale)
	{
		WindowResized = true;
		s_backbuffer_width = W;
		s_backbuffer_height = H;
		s_LastEFBScale = g_ActiveConfig.iEFBScale;
	}

	if (xfbchanged || WindowResized || (s_LastMultisampleMode != g_ActiveConfig.iMultisampleMode))
	{
		UpdateDrawRectangle(s_backbuffer_width, s_backbuffer_height);

		if (CalculateTargetSize(s_backbuffer_width, s_backbuffer_height) || s_LastMultisampleMode != g_ActiveConfig.iMultisampleMode)
		{
			s_LastMultisampleMode = g_ActiveConfig.iMultisampleMode;
			s_MSAASamples = GetNumMSAASamples(s_LastMultisampleMode);
			s_MSAACoverageSamples = GetNumMSAACoverageSamples(s_LastMultisampleMode);

			delete g_framebuffer_manager;
			g_framebuffer_manager = new FramebufferManager(s_target_width, s_target_height,
				s_MSAASamples, s_MSAACoverageSamples);
		}
	}

	if (XFBWrited)
		s_fps = UpdateFPSCounter();
	// ---------------------------------------------------------------------
	GL_REPORT_ERRORD();
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	DrawDebugText();
	DrawDebugInfo();

	GL_REPORT_ERRORD();

	OSD::DrawMessages();
	GL_REPORT_ERRORD();

	// Copy the rendered frame to the real window
	GLInterface->Swap();

	GL_REPORT_ERRORD();

	// Clear framebuffer
	if(!g_ActiveConfig.bAnaglyphStereo)
	{
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	GL_REPORT_ERRORD();

	// Clean out old stuff from caches. It's not worth it to clean out the shader caches.
	DLCache::ProgressiveCleanup();
	TextureCache::Cleanup();

	frameCount++;

	GFX_DEBUGGER_PAUSE_AT(NEXT_FRAME, true);

	// Begin new frame
	// Set default viewport and scissor, for the clear to work correctly
	// New frame
	stats.ResetFrame();

	// Render to the framebuffer.
	FramebufferManager::SetFramebuffer(0);

	GL_REPORT_ERRORD();

	RestoreAPIState();

	GL_REPORT_ERRORD();
	g_Config.iSaveTargetId = 0;

	UpdateActiveConfig();
	TextureCache::OnConfigChanged(g_ActiveConfig);

	// For testing zbuffer targets.
	// Renderer::SetZBufferRender();
	// SaveTexture("tex.tga", GL_TEXTURE_RECTANGLE_ARB, s_FakeZTarget,
	//	      GetTargetWidth(), GetTargetHeight());
	Core::Callback_VideoCopiedToXFB(XFBWrited || (g_ActiveConfig.bUseXFB && g_ActiveConfig.bUseRealXFB));
	XFBWrited = false;

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
	VertexShaderManager::SetViewportChanged();

	glPolygonMode(GL_FRONT_AND_BACK, g_ActiveConfig.bWireFrame ? GL_LINE : GL_FILL);
	
	VertexManager *vm = (OGL::VertexManager*)g_vertex_manager;
	glBindBuffer(GL_ARRAY_BUFFER, vm->m_vertex_buffers);
	vm->m_last_vao = 0;
	
	TextureCache::SetStage();
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
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
	}
}

void Renderer::SetLogicOpMode()
{
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

	if (bpmem.blendmode.logicopenable)
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

void Renderer::SetLineWidth()
{
	float fratio = xfregs.viewport.wd != 0 ?
		((float)Renderer::GetTargetWidth() / EFB_WIDTH) : 1.0f;
	if (bpmem.lineptwidth.linesize > 0)
		// scale by ratio of widths
		glLineWidth((float)bpmem.lineptwidth.linesize * fratio / 6.0f);
	if (bpmem.lineptwidth.pointsize > 0)
		glPointSize((float)bpmem.lineptwidth.pointsize * fratio / 6.0f);
}

void Renderer::SetSamplerState(int stage, int texindex)
{
	auto const& tex = bpmem.tex[texindex];
	auto const& tm0 = tex.texMode0[stage];
	auto const& tm1 = tex.texMode1[stage];
	
	s_sampler_cache.SetSamplerState((texindex * 4) + stage, tm0, tm1);
}

void Renderer::SetInterlacingMode()
{
	// TODO
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

}

// TODO: remove
extern bool g_aspect_wide;

#if defined(HAVE_WX) && HAVE_WX
void TakeScreenshot(ScrStrct* threadStruct)
{
	// These will contain the final image size
	float FloatW = (float)threadStruct->W;
	float FloatH = (float)threadStruct->H;

	// Handle aspect ratio for the final ScrStrct to look exactly like what's on screen.
	if (g_ActiveConfig.iAspectRatio != ASPECT_STRETCH)
	{
		bool use16_9 = g_aspect_wide;

		// Check for force-settings and override.
		if (g_ActiveConfig.iAspectRatio == ASPECT_FORCE_16_9)
			use16_9 = true;
		else if (g_ActiveConfig.iAspectRatio == ASPECT_FORCE_4_3)
			use16_9 = false;

		float Ratio = (FloatW / FloatH) / (!use16_9 ? (4.0f / 3.0f) : (16.0f / 9.0f));

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
	threadStruct->img->SaveFile(wxString::FromAscii(threadStruct->filename.c_str()),
		wxBITMAP_TYPE_PNG);
	threadStruct->img->Destroy();

	// Show success messages
	OSD::AddMessage(StringFromFormat("Saved %i x %i %s", (int)FloatW, (int)FloatH,
		threadStruct->filename.c_str()).c_str(), 2000);
	delete threadStruct;
}
#endif

namespace OGL
{

bool Renderer::SaveScreenshot(const std::string &filename, const TargetRectangle &back_rc)
{
	u32 W = back_rc.GetWidth();
	u32 H = back_rc.GetHeight();
	u8 *data = (u8 *)malloc((sizeof(u8) * 3 * W * H));
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	glReadPixels(back_rc.left, back_rc.bottom, W, H, GL_RGB, GL_UNSIGNED_BYTE, data);

	// Show failure message
	if (GL_REPORT_ERROR() != GL_NO_ERROR)
	{
		free(data);
		OSD::AddMessage("Error capturing or saving screenshot.", 2000);
		return false;
	}

	// Turn image upside down
	FlipImageData(data, W, H);

#if defined(HAVE_WX) && HAVE_WX
	// Create wxImage
	wxImage *a = new wxImage(W, H, data);

	if (scrshotThread.joinable())
		scrshotThread.join();

	ScrStrct *threadStruct = new ScrStrct;
	threadStruct->filename = filename;
	threadStruct->img = a;
	threadStruct->H = H; threadStruct->W = W;

	scrshotThread = std::thread(TakeScreenshot, threadStruct);
#ifdef _WIN32
	SetThreadPriority(scrshotThread.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
	bool result = true;

	OSD::AddMessage("Saving Screenshot... ", 2000);

#else
	bool result = SaveTGA(filename.c_str(), W, H, data);
	free(data);
#endif

	return result;
}

}
