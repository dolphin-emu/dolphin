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

#ifndef _VERTEXSHADERMANAGER_H
#define _VERTEXSHADERMANAGER_H

#include <map>
#include <string>

#include "GLUtil.h"
#include "VertexShader.h"

struct VERTEXSHADER
{
    VERTEXSHADER() : glprogid(0) {}
    GLuint glprogid; // opengl program id

#if defined(_DEBUG) || defined(DEBUGFAST) 
	std::string strprog;
#endif
};

class VertexShaderCache
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

public:
    static void Init();
    static void Cleanup();
    static void Shutdown();

	static VERTEXSHADER* GetShader(u32 components);
    static bool CompileVertexShader(VERTEXSHADER& ps, const char* pstrprogram);
};

// The non-API dependent parts.
class VertexShaderManager
{
public:
    static void Init();
    static void Shutdown();

    // constant management
    static void SetConstants();

    static void SetViewport(float* _Viewport);
    static void SetViewportChanged();
    static void SetProjection(float* _pProjection, int constantIndex = -1);
    static void InvalidateXFRange(int start, int end);
    static void SetTexMatrixChangedA(u32 Value);
    static void SetTexMatrixChangedB(u32 Value);

    static float* GetPosNormalMat();
};

#endif
