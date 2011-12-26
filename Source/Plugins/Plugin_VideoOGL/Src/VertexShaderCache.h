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

#ifndef _VERTEXSHADERCACHE_H_
#define _VERTEXSHADERCACHE_H_

#include <map>
#include <string>

#include "BPMemory.h"
#include "VertexShaderGen.h"

namespace OGL
{

struct VERTEXSHADER
{
	VERTEXSHADER() : glprogid(0), bGLSL(0) {}
	void Destroy()
	{
		if (bGLSL)
			glDeleteShader(glprogid);
		else
			glDeleteProgramsARB(1, &glprogid);
		glprogid = 0;
	}
	GLuint glprogid; // opengl program id

	bool bGLSL;
	std::string strprog;
};

class VertexShaderCache
{
	struct VSCacheEntry
	{ 
		VERTEXSHADER shader;
		VERTEXSHADERUIDSAFE safe_uid;
		VSCacheEntry() {}
		void Destroy() {
			shader.Destroy();
		}
	};

	typedef std::map<VERTEXSHADERUID, VSCacheEntry> VSCache;

	static VSCache vshaders;

	static VSCacheEntry* last_entry;
	static VERTEXSHADERUID last_uid;

	static GLuint CurrentShader;
	static bool ShaderEnabled;

public:
	static void Init();
	static void Shutdown();

	static VERTEXSHADER* SetShader(u32 components);
	static bool CompileVertexShader(VERTEXSHADER& ps, const char* pstrprogram);

	static void SetCurrentShader(GLuint Shader);
	static void DisableShader();
	
};
// GLSL Specific
void SetGLSLVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
void SetGLSLVSConstant4fv(unsigned int const_number, const float *f);
void SetMultiGLSLVSConstant4fv(unsigned int const_number, unsigned int count, const float *f);
void SetMultiGLSLVSConstant3fv(unsigned int const_number, unsigned int count, const float *f);
bool CompileGLSLVertexShader(VERTEXSHADER& vs, const char* pstrprogram);

// CG Specific
void SetCGVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
void SetCGVSConstant4fv(unsigned int const_number, const float *f);
void SetMultiCGVSConstant4fv(unsigned int const_number, unsigned int count, const float *f);
void SetMultiCGVSConstant3fv(unsigned int const_number, unsigned int count, const float *f);
bool CompileCGVertexShader(VERTEXSHADER& vs, const char* pstrprogram);
}  // namespace OGL

#endif // _VERTEXSHADERCACHE_H_
