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
#include "Profiler.h"

#include "GLUtil.h"

#include <Cg/cg.h>
#include <Cg/cgGL.h>

#include <cmath>

#include "Statistics.h"
#include "Config.h"
#include "ImageWrite.h"
#include "Common.h"
#include "Render.h"
#include "VertexShaderGen.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"

static int s_nMaxPixelInstructions;
static GLuint s_ColorMatrixProgram = 0;
PixelShaderCache::PSCache PixelShaderCache::pshaders;
PIXELSHADERUID PixelShaderCache::s_curuid;

static FRAGMENTSHADER* pShaderLast = NULL;

void SetPSConstant4f(int const_number, float f1, float f2, float f3, float f4)
{
	glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, const_number, f1, f2, f3, f4);
}

void SetPSConstant4fv(int const_number, const float *f)
{
	glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, const_number, f);
}

void PixelShaderCache::Init()
{
	GL_REPORT_ERRORD();

	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB, (GLint *)&s_nMaxPixelInstructions);
	
	int maxinst, maxattribs;
	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, (GLint *)&maxinst);
	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB, (GLint *)&maxattribs);
	ERROR_LOG("pixel max_alu=%d, max_inst=%d, max_attrib=%d\n", s_nMaxPixelInstructions, maxinst, maxattribs);
	
	char pmatrixprog[1024];
	sprintf(pmatrixprog, "!!ARBfp1.0"
						"TEMP R0;\n"
						"TEMP R1;\n"
						"TEX R0, fragment.texcoord[0], texture[0], RECT;\n"
						"DP4 R1.w, R0, program.env[%d];\n"
						"DP4 R1.z, R0, program.env[%d];\n"
						"DP4 R1.x, R0, program.env[%d];\n"
						"DP4 R1.y, R0, program.env[%d];\n"
						"ADD result.color, R1, program.env[%d];\n"
						"END\n", C_COLORMATRIX+3, C_COLORMATRIX+2, C_COLORMATRIX, C_COLORMATRIX+1, C_COLORMATRIX+4);
	glGenProgramsARB(1, &s_ColorMatrixProgram);
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, s_ColorMatrixProgram);
	glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pmatrixprog), pmatrixprog);

	GLenum err = GL_NO_ERROR;
	GL_REPORT_ERROR();
	if (err != GL_NO_ERROR) {
		ERROR_LOG("Failed to create color matrix fragment program\n");
		glDeleteProgramsARB(1, &s_ColorMatrixProgram);
		s_ColorMatrixProgram = 0;
	}
}

void PixelShaderCache::Shutdown()
{
	glDeleteProgramsARB(1, &s_ColorMatrixProgram);
	s_ColorMatrixProgram = 0;
	PSCache::iterator iter = pshaders.begin();
	for (; iter != pshaders.end(); iter++)
		iter->second.Destroy();
	pshaders.clear();
}

GLuint PixelShaderCache::GetColorMatrixProgram()
{
	return s_ColorMatrixProgram;
}


FRAGMENTSHADER* PixelShaderCache::GetShader()
{
	DVSTARTPROFILE();
	PIXELSHADERUID uid;
	u32 zbufrender = (Renderer::GetZBufferTarget() && bpmem.zmode.updateenable) ? 1 : 0;
	u32 zBufRenderToCol0 = Renderer::GetRenderMode() != Renderer::RM_Normal;
	GetPixelShaderId(uid, PixelShaderManager::GetTextureMask(), zbufrender, zBufRenderToCol0);

	PSCache::iterator iter = pshaders.find(uid);

	if (iter != pshaders.end()) {
		iter->second.frameCount = frameCount;
		PSCacheEntry &entry = iter->second;
		if (&entry.shader != pShaderLast)
		{
			pShaderLast = &entry.shader;
		}
		return pShaderLast;
	}

	PSCacheEntry& newentry = pshaders[uid];
	const char *code = GeneratePixelShader(PixelShaderManager::GetTextureMask(),
										   Renderer::GetZBufferTarget() != 0,
										   Renderer::GetRenderMode() != Renderer::RM_Normal);

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_Config.iLog & CONF_SAVESHADERS && code) {	
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%s/ps_%04i.txt", FULL_DUMP_DIR, counter++);
		
		SaveData(szTemp, code);
	}
#endif

	//	printf("Compiling pixel shader. size = %i\n", strlen(code));
	if (!code || !CompilePixelShader(newentry.shader, code)) {
		ERROR_LOG("failed to create pixel shader\n");
		return NULL;
	}

	//Make an entry in the table
	newentry.frameCount = frameCount;
	
	pShaderLast = &newentry.shader;
	INCSTAT(stats.numPixelShadersCreated);
	SETSTAT(stats.numPixelShadersAlive, pshaders.size());
	return pShaderLast;
}

void PixelShaderCache::Cleanup()
{
	PSCache::iterator iter = pshaders.begin();
	while (iter != pshaders.end()) {
		PSCacheEntry &entry = iter->second;
		if (entry.frameCount < frameCount - 400) {
			entry.Destroy();
#ifdef _WIN32
			iter = pshaders.erase(iter);
#else
			pshaders.erase(iter++);  // (this is gcc standard!)
#endif
		}
		else
			iter++;
	}
	SETSTAT(stats.numPixelShadersAlive,(int)pshaders.size());
}

bool PixelShaderCache::CompilePixelShader(FRAGMENTSHADER& ps, const char* pstrprogram)
{
	char stropt[64];
	sprintf(stropt, "MaxLocalParams=32,NumInstructionSlots=%d", s_nMaxPixelInstructions);
	const char* opts[] = {"-profileopts", stropt, "-O2", "-q", NULL};
	CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgfProf, "main", opts);
	if (!cgIsProgram(tempprog) || cgGetError() != CG_NO_ERROR) {
		ERROR_LOG("Failed to create ps %s:\n", cgGetLastListing(g_cgcontext));
		ERROR_LOG(pstrprogram);
		return false;
	}

	char *pcompiledprog = (char*)cgGetProgramString(tempprog, CG_COMPILED_PROGRAM);
	char *plocal = strstr(pcompiledprog, "program.local");
	while (plocal != NULL) {
		const char* penv = "  program.env";
		memcpy(plocal, penv, 13);
		plocal = strstr(plocal+13, "program.local");
	}

	if (Renderer::IsUsingATIDrawBuffers()) {
		// sometimes compilation can use ARB_draw_buffers, which would fail for ATI cards
		// replace the three characters ARB with ATI. TODO - check whether this is fixed in modern ATI drivers.
		char* poptions = strstr(pcompiledprog, "ARB_draw_buffers");
		if (poptions != NULL) {
			poptions[0] = 'A';
			poptions[1] = 'T';
			poptions[2] = 'I';
		}
	}

	glGenProgramsARB( 1, &ps.glprogid );
	glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, ps.glprogid );
	glProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pcompiledprog), pcompiledprog);

	GLenum err = GL_NO_ERROR;
	GL_REPORT_ERROR();
	if (err != GL_NO_ERROR) {
		ERROR_LOG(pstrprogram);
		ERROR_LOG(pcompiledprog);
	}

	cgDestroyProgram(tempprog);
	// printf("Compiled pixel shader %i\n", ps.glprogid);

#if defined(_DEBUG) || defined(DEBUGFAST) 
	ps.strprog = pstrprogram;
#endif
	return true;
}
