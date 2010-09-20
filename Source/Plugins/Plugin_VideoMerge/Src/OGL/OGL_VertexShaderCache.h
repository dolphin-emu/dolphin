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

#ifndef _OGL_VERTEXSHADERCACHE_H_
#define _OGL_VERTEXSHADERCACHE_H_

#include <map>
#include <string>

// VideoCommon
#include "BPMemory.h"
#include "VertexShaderGen.h"

// OGL
#include "OGL_GLUtil.h"

#include "../VertexShaderCache.h"

namespace OGL
{

struct VERTEXSHADER
{
	VERTEXSHADER() : glprogid(0) {}
	GLuint glprogid; // opengl program id

#if defined(_DEBUG) || defined(DEBUGFAST) 
	std::string strprog;
#endif
};

class VertexShaderCache : public ::VertexShaderCacheBase
{
	struct VSCacheEntry
	{ 
		VERTEXSHADER shader;
		int frameCount;
		VSCacheEntry() : frameCount(0) {}
		void Destroy() {
			// printf("Destroying vs %i\n", shader.glprogid);
			glDeleteProgramsARB(1, &shader.glprogid);
			shader.glprogid = 0;
		}
	};

	typedef std::map<VERTEXSHADERUID, VSCacheEntry> VSCache;

	static VSCache vshaders;

    static bool s_displayCompileAlert;
	
	static GLuint CurrentShader;
	static bool ShaderEnabled;

public:
	VertexShaderCache();
	~VertexShaderCache();

	bool SetShader(u32 components);

	static VERTEXSHADER* GetShader(u32 components);
	static bool CompileVertexShader(VERTEXSHADER& ps, const char* pstrprogram);

	static void SetCurrentShader(GLuint Shader);
	static void DisableShader();

	void SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4);
	void SetVSConstant4fv(unsigned int const_number, const float* f);
	void SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float* f);
	void SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float* f);
};

}

#endif // _VERTEXSHADERCACHE_H_
