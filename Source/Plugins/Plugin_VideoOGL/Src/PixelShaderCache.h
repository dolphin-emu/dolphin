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

#ifndef _PIXELSHADERCACHE_H_
#define _PIXELSHADERCACHE_H_

#include <map>
#include <string>

#include "BPMemory.h"
#include "PixelShaderGen.h"

namespace OGL
{

struct FRAGMENTSHADER
{
	FRAGMENTSHADER() : glprogid(0) { }
	void Destroy()
	{
		if (glprogid)
		{
			glDeleteProgramsARB(1, &glprogid);
			glprogid = 0;
		}
	}
	GLuint glprogid; // opengl program id
	std::string strprog;
};

class PixelShaderCache
{
	struct PSCacheEntry
	{
		FRAGMENTSHADER shader;
		PSCacheEntry() {}
		~PSCacheEntry() {}
		void Destroy()
		{
			shader.Destroy();
		}
		PIXELSHADERUIDSAFE safe_uid;
	};

	typedef std::map<PIXELSHADERUID, PSCacheEntry> PSCache;

	static PSCache PixelShaders;

	static PIXELSHADERUID s_curuid; // the current pixel shader uid (progressively changed as memory is written)

    static bool s_displayCompileAlert;

	static GLuint CurrentShader;
	static PSCacheEntry* last_entry;
	static PIXELSHADERUID last_uid;

	static bool ShaderEnabled;

public:
	static void Init();
	static void Shutdown();
	// This is a GLSL only function
    static void SetPSSampler(const char * name, unsigned int Tex);

	static FRAGMENTSHADER* SetShader(DSTALPHA_MODE dstAlphaMode, u32 components);
	static bool CompilePixelShader(FRAGMENTSHADER& ps, const char* pstrprogram);

	static GLuint GetColorMatrixProgram();

    static GLuint GetDepthMatrixProgram();

	static void SetCurrentShader(GLuint Shader);

	static void DisableShader();
};
// GLSL Specific
void SetGLSLPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
void SetGLSLPSConstant4fv(unsigned int const_number, const float *f);
void SetMultiGLSLPSConstant4fv(unsigned int const_number, unsigned int count, const float *f);
bool CompileGLSLPixelShader(FRAGMENTSHADER& ps, const char* pstrprogram);

//CG Specific
void SetCGPSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
void SetCGPSConstant4fv(unsigned int const_number, const float *f);
void SetMultiCGPSConstant4fv(unsigned int const_number, unsigned int count, const float *f);
bool CompileCGPixelShader(FRAGMENTSHADER& ps, const char* pstrprogram);
}  // namespace OGL

#endif // _PIXELSHADERCACHE_H_
