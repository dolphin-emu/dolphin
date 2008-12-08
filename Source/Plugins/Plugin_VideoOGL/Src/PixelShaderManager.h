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

#include <map>

#include "BPMemory.h"

struct FRAGMENTSHADER
{
    FRAGMENTSHADER() : glprogid(0) { }
    GLuint glprogid; // opengl program id
#if defined(_DEBUG) || defined(DEBUGFAST) 
	std::string strprog;
#endif
};

class PIXELSHADERUID
{
public:
	u32 values[4+32+6+11];
	u16 tevstages, indstages;

	PIXELSHADERUID() {
		memset(values, 0, (4+32+6+11) * 4);
		tevstages = indstages = 0;
	}
	PIXELSHADERUID(const PIXELSHADERUID& r)
	{
		tevstages = r.tevstages;
		indstages = r.indstages;
		int N = tevstages + indstages + 3;
		_assert_(N <= 4+32+6+11);
		for (int i = 0; i < N; ++i) 
			values[i] = r.values[i];
	}
	int GetNumValues() const {
		return tevstages + indstages + 3; // numTevStages*3/2+1
	}
	bool operator <(const PIXELSHADERUID& _Right) const
	{
		if (values[0] < _Right.values[0])
			return true;
		else if (values[0] > _Right.values[0])
			return false;
		int N = GetNumValues();
		for (int i = 1; i < N; ++i) {
			if (values[i] < _Right.values[i])
				return true;
			else if (values[i] > _Right.values[i])
				return false;
		}
		return false;
	}
	bool operator ==(const PIXELSHADERUID& _Right) const
	{
		if (values[0] != _Right.values[0])
			return false;
		int N = GetNumValues();
		for (int i = 1; i < N; ++i) {
			if (values[i] != _Right.values[i])
				return false;
		}
		return true;
	}
};


class PixelShaderMngr
{
    struct PSCacheEntry
    {
        FRAGMENTSHADER shader;
        int frameCount;
        PSCacheEntry() : frameCount(0) {}
		~PSCacheEntry() {}
        void Destroy() {
			// printf("Destroying ps %i\n", shader.glprogid);
            glDeleteProgramsARB(1, &shader.glprogid);
			shader.glprogid = 0;
        }
    };

    typedef std::map<PIXELSHADERUID, PSCacheEntry> PSCache;

    static FRAGMENTSHADER* pShaderLast; // last used shader
    static PSCache pshaders;

    static void GetPixelShaderId(PIXELSHADERUID&);
    static PIXELSHADERUID s_curuid; // the current pixel shader uid (progressively changed as memory is written)

	static void SetPSConstant4f(int const_number, float f1, float f2, float f3, float f4);
	static void SetPSConstant4fv(int const_number, const float *f);

	static void SetPSTextureDims(int texid);

public:
    static void Init();
    static void Cleanup();
    static void Shutdown();
    static FRAGMENTSHADER* GetShader();
    static bool CompilePixelShader(FRAGMENTSHADER& ps, const char* pstrprogram);

    static void SetConstants(); // sets pixel shader constants

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
