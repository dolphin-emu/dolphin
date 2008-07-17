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

#include "VertexShader.h"

// shader variables
#define I_POSNORMALMATRIX "cpnmtx"
#define I_PROJECTION "cproj"
#define I_MATERIALS "cmtrl"
#define I_LIGHTS "clights"
#define I_TEXMATRICES "ctexmtx"
#define I_TRANSFORMMATRICES "ctrmtx"
#define I_NORMALMATRICES "cnmtx"
#define I_POSTTRANSFORMMATRICES "cpostmtx"
#define I_FOGPARAMS "cfog"

#define C_POSNORMALMATRIX 0
#define C_PROJECTION (C_POSNORMALMATRIX+6)
#define C_MATERIALS (C_PROJECTION+4)
#define C_LIGHTS (C_MATERIALS+4)
#define C_TEXMATRICES (C_LIGHTS+40)
#define C_TRANSFORMMATRICES (C_TEXMATRICES+24)
#define C_NORMALMATRICES (C_TRANSFORMMATRICES+64)
#define C_POSTTRANSFORMMATRICES (C_NORMALMATRICES+32)
#define C_FOGPARAMS (C_POSTTRANSFORMMATRICES+64)


class VertexShaderMngr
{
    struct VSCacheEntry
    { 
        VERTEXSHADER shader;
        int frameCount;
        VSCacheEntry() : frameCount(0) {}
        void Destroy() {
            SAFE_RELEASE_PROG(shader.glprogid);
        }
    };

    class VERTEXSHADERUID
    {
    public:
        VERTEXSHADERUID() {}
        VERTEXSHADERUID(const VERTEXSHADERUID& r) {
			for(size_t i = 0; i < sizeof(values) / sizeof(u32); ++i) 
				values[i] = r.values[i]; 
		}

        bool operator<(const VERTEXSHADERUID& _Right) const
        {
            if( values[0] < _Right.values[0] )
                return true;
            else if( values[0] > _Right.values[0] )
                return false;

            int N = (((values[0]>>23)&0xf)*3+3)/4 + 3; // numTexGens*3/4+1
            for(int i = 1; i < N; ++i) {
                if( values[i] < _Right.values[i] )
                    return true;
                else if( values[i] > _Right.values[i] )
                    return false;
            }

            return false;
        }

        bool operator==(const VERTEXSHADERUID& _Right) const
        {
            if( values[0] != _Right.values[0] )
                return false;

            int N = (((values[0]>>23)&0xf)*3+3)/4 + 3; // numTexGens*3/4+1
            for(int i = 1; i < N; ++i) {
                if( values[i] != _Right.values[i] )
                    return false;
            }

            return true;
        }

        u32 values[9];
    };

    typedef std::map<VERTEXSHADERUID,VSCacheEntry> VSCache;

    static VSCache vshaders;
    static VERTEXSHADER* pShaderLast;
    static TMatrixIndexA MatrixIndexA;
    static TMatrixIndexB MatrixIndexB;

    static void GetVertexShaderId(VERTEXSHADERUID& uid, u32 components);

public:
    static void Init();
    static void Cleanup();
    static void Shutdown();
    static VERTEXSHADER* GetShader(u32 components);
    static bool CompileVertexShader(VERTEXSHADER& ps, const char* pstrprogram);

    // constant management
    static void SetConstants(VERTEXSHADER& vs);

    static void SetViewport(float* _Viewport);
    static void SetViewportChanged();
    static void SetProjection(float* _pProjection, int constantIndex = -1);
    static void InvalidateXFRange(int start, int end);
    static void SetTexMatrixChangedA(u32 Value);
    static void SetTexMatrixChangedB(u32 Value);

    static  size_t SaveLoadState(char *ptr, BOOL save);
    static void LoadXFReg(u32 transferSize, u32 address, u32 *pData);
    static void LoadIndexedXF(u32 val, int array);

    static float* GetPosNormalMat();

    static float rawViewport[6];
    static float rawProjection[7];
};


#endif
