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

#include "GLUtil.h"

#include <cmath>

#include "Statistics.h"
#include "VideoConfig.h"
#include "ImageWrite.h"
#include "Common.h"
#include "Render.h"
#include "VertexShaderGen.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "FileUtil.h"
#include "Debugger.h"

namespace OGL
{

static int s_nMaxPixelInstructions;
static GLuint s_ColorMatrixProgram = 0;
static GLuint s_DepthMatrixProgram = 0;
PixelShaderCache::PSCache PixelShaderCache::PixelShaders;
PIXELSHADERUID PixelShaderCache::s_curuid;
bool PixelShaderCache::s_displayCompileAlert;
GLuint PixelShaderCache::CurrentShader;
bool PixelShaderCache::ShaderEnabled;

PixelShaderCache::PSCacheEntry* PixelShaderCache::last_entry = NULL;
PIXELSHADERUID PixelShaderCache::last_uid;

GLuint PixelShaderCache::GetDepthMatrixProgram()
{
	return s_DepthMatrixProgram;
}

GLuint PixelShaderCache::GetColorMatrixProgram()
{
	return s_ColorMatrixProgram;
}

void PixelShaderCache::Init()
{
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	ShaderEnabled = true;
	CurrentShader = 0;
	last_entry = NULL;
	GL_REPORT_ERRORD();

	s_displayCompileAlert = true;

	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB, (GLint *)&s_nMaxPixelInstructions);
	
	if(s_nMaxPixelInstructions == 0) // Some combination of drivers and hardware returns zero for some reason.
		s_nMaxPixelInstructions = 4096;
	if (strstr((const char*)glGetString(GL_VENDOR), "Humper") != NULL) s_nMaxPixelInstructions = 4096;
#if CG_VERSION_NUM == 2100
	if (strstr((const char*)glGetString(GL_VENDOR), "ATI") != NULL)
	{
		s_nMaxPixelInstructions = 4096;
	}
#endif

	int maxinst, maxattribs;
	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, (GLint *)&maxinst);
	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB, (GLint *)&maxattribs);
	INFO_LOG(VIDEO, "pixel max_alu=%d, max_inst=%d, max_attrib=%d", s_nMaxPixelInstructions, maxinst, maxattribs);
	
	char pmatrixprog[2048];
	sprintf(pmatrixprog, "!!ARBfp1.0"
						"TEMP R0;\n"
						"TEMP R1;\n"
						"PARAM K0 = { 0.5, 0.5, 0.5, 0.5};\n" 
						"TEX R0, fragment.texcoord[0], texture[0], RECT;\n"
						"MUL R0, R0, program.env[%d];\n"
						"ADD R0, R0, K0;\n"
						"FLR R0, R0;\n"
						"MUL R0, R0, program.env[%d];\n"
						"DP4 R1.x, R0, program.env[%d];\n"
						"DP4 R1.y, R0, program.env[%d];\n"
						"DP4 R1.z, R0, program.env[%d];\n"
						"DP4 R1.w, R0, program.env[%d];\n"
						"ADD result.color, R1, program.env[%d];\n"
						"END\n",C_COLORMATRIX+5,C_COLORMATRIX+6, C_COLORMATRIX, C_COLORMATRIX+1, C_COLORMATRIX+2, C_COLORMATRIX+3, C_COLORMATRIX+4);
	glGenProgramsARB(1, &s_ColorMatrixProgram);
	SetCurrentShader(s_ColorMatrixProgram);
	glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pmatrixprog), pmatrixprog);

	GLenum err = GL_REPORT_ERROR();
	if (err != GL_NO_ERROR) {
		ERROR_LOG(VIDEO, "Failed to create color matrix fragment program");
		glDeleteProgramsARB(1, &s_ColorMatrixProgram);
		s_ColorMatrixProgram = 0;
	}

     sprintf(pmatrixprog, "!!ARBfp1.0\n"
						"TEMP R0;\n"
						"TEMP R1;\n"
                        "TEMP R2;\n"
                        //16777215/16777216*256, 1/255, 256, 0
                        "PARAM K0 = { 255.99998474121, 0.003921568627451, 256.0, 0.0};\n"
						"PARAM K1 = { 15.0, 0.066666666666, 0.0, 0.0};\n"
                        //sample the depth value
						"TEX R2, fragment.texcoord[0], texture[0], RECT;\n"

                        //scale from [0*16777216..1*16777216] to
                        //[0*16777215..1*16777215], multiply by 256
						"MUL R0, R2.x, K0.x;\n" // *16777215/16777216*256

                        //It is easy to get bad results due to low precision
                        //here, for example converting like this:
                        //MUL R0,R0,{ 65536, 256, 1, 16777216 }
                        //FRC R0,R0
                        //gives {?, 128/255, 254/255, ?} for depth value 254/255
                        //on some gpus

                        "FLR R0.x,R0;\n"        //bits 31..24

                        "SUB R0.yzw,R0,R0.x;\n" //subtract bits 31..24 from rest
                        "MUL R0.yzw,R0,K0.z;\n" // *256
                        "FLR R0.y,R0;\n"        //bits 23..16

                        "SUB R0.zw,R0,R0.y;\n"  //subtract bits 23..16 from rest
                        "MUL R0.zw,R0,K0.z;\n"  // *256
                        "FLR R0.z,R0;\n"        //bits 15..8

                        "MOV R0.w,R0.x;\n"   //duplicate bit 31..24
                        
						"MUL R0,R0,K0.y;\n"     // /255

						"MUL R0.w,R0,K1.x;\n"   // *15
                        "FLR R0.w,R0;\n"        //bits 31..28
						"MUL R0.w,R0,K1.y;\n"   // /15

						"DP4 R1.x, R0, program.env[%d];\n"
						"DP4 R1.y, R0, program.env[%d];\n"
                        "DP4 R1.z, R0, program.env[%d];\n"
                        "DP4 R1.w, R0, program.env[%d];\n"
						"ADD result.color, R1, program.env[%d];\n"
						"END\n", C_COLORMATRIX, C_COLORMATRIX+1, C_COLORMATRIX+2, C_COLORMATRIX+3, C_COLORMATRIX+4);
	glGenProgramsARB(1, &s_DepthMatrixProgram);
	SetCurrentShader(s_DepthMatrixProgram);
	glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pmatrixprog), pmatrixprog);

	err = GL_REPORT_ERROR();
	if (err != GL_NO_ERROR) {
		ERROR_LOG(VIDEO, "Failed to create depth matrix fragment program");
		glDeleteProgramsARB(1, &s_DepthMatrixProgram);
		s_DepthMatrixProgram = 0;	
	}
	
}

void PixelShaderCache::Shutdown()
{
	glDeleteProgramsARB(1, &s_ColorMatrixProgram);
	s_ColorMatrixProgram = 0;
    glDeleteProgramsARB(1, &s_DepthMatrixProgram);
	s_DepthMatrixProgram = 0;
	PSCache::iterator iter = PixelShaders.begin();
	for (; iter != PixelShaders.end(); iter++)
		iter->second.Destroy();
	PixelShaders.clear();
}

FRAGMENTSHADER* PixelShaderCache::SetShader(DSTALPHA_MODE dstAlphaMode, u32 components)
{
	PIXELSHADERUID uid;
	GetPixelShaderId(&uid, dstAlphaMode, components);
	
	// Check if the shader is already set
	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
			ValidatePixelShaderIDs(API_OPENGL, last_entry->safe_uid, last_entry->shader.strprog, dstAlphaMode, components);
			return &last_entry->shader;
		}
	}

	last_uid = uid;

	PSCache::iterator iter = PixelShaders.find(uid);
	if (iter != PixelShaders.end())
	{
		PSCacheEntry &entry = iter->second;
		last_entry = &entry;

		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
		ValidatePixelShaderIDs(API_OPENGL, entry.safe_uid, entry.shader.strprog, dstAlphaMode, components);
		return &last_entry->shader;
	}

	// Make an entry in the table
	PSCacheEntry& newentry = PixelShaders[uid];
	last_entry = &newentry;
	const char *code = GeneratePixelShaderCode(dstAlphaMode, API_OPENGL, components);

	if (g_ActiveConfig.bEnableShaderDebugging && code)
	{
		GetSafePixelShaderId(&newentry.safe_uid, dstAlphaMode, components);
		newentry.shader.strprog = code;
	}

#if defined(_DEBUG) || defined(DEBUGFAST)
	if (g_ActiveConfig.iLog & CONF_SAVESHADERS && code) {	
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%sps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), counter++);
		
		SaveData(szTemp, code);
	}
#endif

	if (!code || !CompilePixelShader(newentry.shader, code)) {
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return NULL;
	}
	
	INCSTAT(stats.numPixelShadersCreated);
	SETSTAT(stats.numPixelShadersAlive, PixelShaders.size());
	GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	return &last_entry->shader;
}

bool PixelShaderCache::CompilePixelShader(FRAGMENTSHADER& ps, const char* pstrprogram)
{
	GLenum err = GL_REPORT_ERROR();
	if (err != GL_NO_ERROR)
	{
		ERROR_LOG(VIDEO, "glError %08x before PS!", err);
	}

#if defined HAVE_CG && HAVE_CG
	char stropt[128];
	sprintf(stropt, "MaxLocalParams=224,NumInstructionSlots=%d", s_nMaxPixelInstructions);
	const char *opts[] = {"-profileopts", stropt, "-O2", "-q", NULL};
	CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgfProf, "main", opts);

	// handle errors
	if (!cgIsProgram(tempprog))
	{
		cgDestroyProgram(tempprog);

		static int num_failures = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%sbad_ps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
		std::ofstream file(szTemp);
		file << pstrprogram;
		file.close();

		PanicAlert("Failed to compile pixel shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%d):\n%s",
						szTemp,
						g_cgfProf,
						cgGetLastListing(g_cgcontext));

		return false;
	}

	// handle warnings
	if (cgGetError() != CG_NO_ERROR)
	{
		WARN_LOG(VIDEO, "Warnings on compile ps %s:", cgGetLastListing(g_cgcontext));
		WARN_LOG(VIDEO, "%s", pstrprogram);
	}

	// This looks evil - we modify the program through the const char * we got from cgGetProgramString!
	// It SHOULD not have any nasty side effects though - but you never know...
	char *pcompiledprog = (char*)cgGetProgramString(tempprog, CG_COMPILED_PROGRAM);
	char *plocal = strstr(pcompiledprog, "program.local");
	while (plocal != NULL)
	{
		const char *penv = "  program.env";
		memcpy(plocal, penv, 13);
		plocal = strstr(plocal+13, "program.local");
	}

	glGenProgramsARB(1, &ps.glprogid);
	SetCurrentShader(ps.glprogid);	
	glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pcompiledprog), pcompiledprog);

	err = GL_REPORT_ERROR();
	if (err != GL_NO_ERROR)
	{
		GLint error_pos, native_limit;
		glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &error_pos);
		glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &native_limit);
		// Error occur
		if (error_pos != -1) {
			const char *program_error = (const char *)glGetString(GL_PROGRAM_ERROR_STRING_ARB);
			char line[256];
			strncpy(line, (const char *)pcompiledprog + error_pos, 255);
			line[255] = 0;
			ERROR_LOG(VIDEO, "Error at %i: %s", error_pos, program_error);
			ERROR_LOG(VIDEO, "Line dump: \n%s", line);
		} else if (native_limit != -1) {
			ERROR_LOG(VIDEO, "Hit limit? %i", native_limit);
			// TODO
		}
		ERROR_LOG(VIDEO, "%s", pstrprogram);
		ERROR_LOG(VIDEO, "%s", pcompiledprog);
	}

	cgDestroyProgram(tempprog);
#endif

	return true;
}

//Disable Fragment programs and reset the selected Program
void PixelShaderCache::DisableShader()
{
	if(ShaderEnabled)
	{		
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
		ShaderEnabled = false;
	}	
}

//bind a program if is diferent from the binded oone
void PixelShaderCache::SetCurrentShader(GLuint Shader)
{
	if(!ShaderEnabled)
	{
		glEnable(GL_FRAGMENT_PROGRAM_ARB);
		ShaderEnabled = true;
	}
	if(CurrentShader != Shader)
	{
		if(Shader != 0)
			CurrentShader = Shader;
		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, CurrentShader);
	}
}

void Renderer::SetPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	float f[4] = { f1, f2, f3, f4 };
	glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, const_number, f);
}

void Renderer::SetPSConstant4fv(unsigned int const_number, const float *f)
{
	glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, const_number, f);
}

void Renderer::SetMultiPSConstant4fv(unsigned int const_number, unsigned int count, const float *f)
{
	for (unsigned int i = 0; i < count; i++,f+=4)
		glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, const_number + i, f);
}

}  // namespace OGL
