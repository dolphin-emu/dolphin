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

#ifndef _PIXELSHADERMANAGER_H
#define _PIXELSHADERMANAGER_H

#include "PixelShader.h"

extern u32 s_texturemask;

class PixelShaderMngr
{
    class PIXELSHADERUID
    {
    public:
        PIXELSHADERUID() { values = new u32[3+32+6+11]; tevstages = indstages = 0; }
        ~PIXELSHADERUID() { delete[] values; }
        PIXELSHADERUID(const PIXELSHADERUID& r)
        {
            values = new u32[4+32+6+11];
            tevstages = r.tevstages; indstages = r.indstages;
            int N = tevstages + indstages + 3;
            _assert_(N <= 4+32+6+11);
            for(int i = 0; i < N; ++i) 
				values[i] = r.values[i];
        }

        bool operator <(const PIXELSHADERUID& _Right) const
        {
            if( values[0] < _Right.values[0] )
                return true;
            else if( values[0] > _Right.values[0] )
                return false;

            int N = tevstages + 3; // numTevStages*3/2+1
            int i = 1;
            for(; i < N; ++i) {
                if( values[i] < _Right.values[i] )
                    return true;
                else if( values[i] > _Right.values[i] )
                    return false;
            }

            N += indstages;
            for(; i < N; ++i) {
                if( values[i] < _Right.values[i] )
                    return true;
                else if( values[i] > _Right.values[i] )
                    return false;
            }

            return false;
        }

        bool operator ==(const PIXELSHADERUID& _Right) const
        {
            if( values[0] != _Right.values[0] )
                return false;

            int N = tevstages + 3; // numTevStages*3/2+1
            int i = 1;
            for(; i < N; ++i) {
                if( values[i] != _Right.values[i] )
                    return false;
            }

            N += indstages;
            for(; i < N; ++i) {
                if( values[i] != _Right.values[i] )
                    return false;
            }

            return true;
        }

        u32* values;
        u16 tevstages, indstages;
    };

    struct PSCacheEntry
    {
        FRAGMENTSHADER shader;
        int frameCount;
        PSCacheEntry() : frameCount(0) {}
		~PSCacheEntry() {}
        void Destroy() {
            glDeleteProgramsARB(1, &shader.glprogid);
        }
    };

    typedef std::map<PIXELSHADERUID,PSCacheEntry> PSCache;

    static FRAGMENTSHADER* pShaderLast; // last used shader
    static PSCache pshaders;

    static void GetPixelShaderId(PIXELSHADERUID&);
    static PIXELSHADERUID s_curuid; // the current pixel shader uid (progressively changed as memory is written)

public:
    static void Init();
    static void Cleanup();
    static void Shutdown();
    static FRAGMENTSHADER* GetShader();
    static bool CompilePixelShader(FRAGMENTSHADER& ps, const char* pstrprogram);

    static void SetConstants(FRAGMENTSHADER& ps); // sets pixel shader constants

    // constant management, should be called after memory is committed
    static void SetColorChanged(int type, int index);
    static void SetAlpha(const AlphaFunc& alpha);
    static void SetDestAlpha(const ConstantAlpha& alpha);
    static void SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt);
    static void SetZTetureBias(u32 bias);
    static void SetIndTexScaleChanged();
    static void SetIndMatrixChanged(int matrixidx);

    static void SetGenModeChanged();
    static void SetTevCombinerChanged(int id);
    static void SetTevKSelChanged(int id);
    static void SetTevOrderChanged(int id);
    static void SetTevIndirectChanged(int id);
    static void SetZTetureOpChanged();
    static void SetTexturesUsed(u32 nonpow2tex);
    static void SetTexDimsChanged(int texmapid);

    static void SetColorMatrix(const float* pmatrix, const float* pfConstAdd);
    static GLuint GetColorMatrixProgram();
};


#endif
