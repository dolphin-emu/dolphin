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

#include "Common.h"
#include "MathUtil.h"
#include "Profiler.h"

#include <cmath>

#include "Statistics.h"

#include "VertexShaderGen.h"
#include "VertexShaderManager.h"
#include "BPMemory.h"
#include "CPMemory.h"
#include "XFMemory.h"
#include "VideoCommon.h"

// Temporary ugly declaration.
namespace VertexManager
{
	void Flush();
}

static float s_fMaterials[16];

// track changes
static bool bTexMatricesChanged[2], bPosNormalMatrixChanged, bProjectionChanged, bViewportChanged;
static int nMaterialsChanged;
static int nTransformMatricesChanged[2]; // min,max
static int nNormalMatricesChanged[2]; // min,max
static int nPostTransformMatricesChanged[2]; // min,max
static int nLightsChanged[2]; // min,max

static Matrix33 s_viewRotationMatrix;
static Matrix33 s_viewInvRotationMatrix;
static float s_fViewTranslationVector[3];
static float s_fViewRotation[2];

void UpdateViewport();

void VertexShaderManager::Init()
{
    nTransformMatricesChanged[0] = nTransformMatricesChanged[1] = -1;
    nNormalMatricesChanged[0] = nNormalMatricesChanged[1] = -1;
    nPostTransformMatricesChanged[0] = nPostTransformMatricesChanged[1] = -1;
    nLightsChanged[0] = nLightsChanged[1] = -1;
    bTexMatricesChanged[0] = bTexMatricesChanged[1] = false;
    bPosNormalMatrixChanged = bProjectionChanged = bViewportChanged = false;
    nMaterialsChanged = 0;

    memset(&xfregs, 0, sizeof(xfregs));
    memset(xfmem, 0, sizeof(xfmem));

    ResetView();    
}

void VertexShaderManager::Shutdown()
{

}

// =======================================================================================
// Syncs the shader constant buffers with xfmem
// ----------------
void VertexShaderManager::SetConstants(bool Hack_hack1 ,float Hack_value1 ,bool Hack_hack2 ,float Hack_value2 ,bool freeLook)
{
    //nTransformMatricesChanged[0] = 0; nTransformMatricesChanged[1] = 256;
    //nNormalMatricesChanged[0] = 0; nNormalMatricesChanged[1] = 96;
    //nPostTransformMatricesChanged[0] = 0; nPostTransformMatricesChanged[1] = 256;
    //nLightsChanged[0] = 0; nLightsChanged[1] = 0x80;
    //bPosNormalMatrixChanged = true;
    //bTexMatricesChanged[0] = bTexMatricesChanged[1] = true;
    //bProjectionChanged = true;
//    bPosNormalMatrixChanged = bTexMatricesChanged[0] = bTexMatricesChanged[1] = true;
//    nMaterialsChanged = 15;

    if (nTransformMatricesChanged[0] >= 0) {
        int startn = nTransformMatricesChanged[0]/4;
        int endn = (nTransformMatricesChanged[1]+3)/4;
        const float* pstart = (const float*)&xfmem[startn*4];
        for(int i = startn; i < endn; ++i, pstart += 4)
            SetVSConstant4fv(C_TRANSFORMMATRICES+i, pstart);
        nTransformMatricesChanged[0] = nTransformMatricesChanged[1] = -1;
    }
    if (nNormalMatricesChanged[0] >= 0) {
        int startn = nNormalMatricesChanged[0]/3;
        int endn = (nNormalMatricesChanged[1]+2)/3;
        const float* pnstart = (const float*)&xfmem[XFMEM_NORMALMATRICES+3*startn];

        for(int i = startn; i < endn; ++i, pnstart += 3)
            SetVSConstant4fv(C_NORMALMATRICES+i, pnstart);

        nNormalMatricesChanged[0] = nNormalMatricesChanged[1] = -1;
    }

    if (nPostTransformMatricesChanged[0] >= 0) {
        int startn = nPostTransformMatricesChanged[0]/4;
        int endn = (nPostTransformMatricesChanged[1]+3)/4;
        const float* pstart = (const float*)&xfmem[XFMEM_POSTMATRICES + startn*4];
        for(int i = startn; i < endn; ++i, pstart += 4)
            SetVSConstant4fv(C_POSTTRANSFORMMATRICES + i, pstart);
    }

    if (nLightsChanged[0] >= 0) {
        // lights don't have a 1 to 1 mapping, the color component needs to be converted to 4 floats
        int istart = nLightsChanged[0] / 0x10;
        int iend = (nLightsChanged[1] + 15) / 0x10;
        const float* xfmemptr = (const float*)&xfmem[0x10*istart + XFMEM_LIGHTS];

        for (int i = istart; i < iend; ++i) {
            u32 color = *(const u32*)(xfmemptr + 3);
            SetVSConstant4f(C_LIGHTS + 5*i,
                ((color >> 24) & 0xFF)/255.0f,
				((color >> 16) & 0xFF)/255.0f,
				((color >> 8)  & 0xFF)/255.0f,
				((color)       & 0xFF)/255.0f);
            xfmemptr += 4;
            for (int j = 0; j < 4; ++j, xfmemptr += 3) {
				if (j == 1 &&
					fabs(xfmemptr[0]) < 0.00001f &&
					fabs(xfmemptr[1]) < 0.00001f &&
					fabs(xfmemptr[2]) < 0.00001f) {
                    // dist attenuation, make sure not equal to 0!!!
                    SetVSConstant4f(C_LIGHTS+5*i+j+1, 0.00001f, xfmemptr[1], xfmemptr[2], 0);
                }
                else
                    SetVSConstant4fv(C_LIGHTS+5*i+j+1, xfmemptr);
            }
        }

        nLightsChanged[0] = nLightsChanged[1] = -1;
    }

    if (nMaterialsChanged) {
        for (int i = 0; i < 4; ++i) {
            if (nMaterialsChanged & (1 << i))
                SetVSConstant4fv(C_MATERIALS + i, &s_fMaterials[4*i]);
        }
        nMaterialsChanged = 0;
    }

    if (bPosNormalMatrixChanged) {
        bPosNormalMatrixChanged = false;

        float* pos = (float*)xfmem + MatrixIndexA.PosNormalMtxIdx * 4;
        float* norm = (float*)xfmem + XFMEM_NORMALMATRICES + 3 * (MatrixIndexA.PosNormalMtxIdx & 31);

        SetVSConstant4fv(C_POSNORMALMATRIX, pos);
        SetVSConstant4fv(C_POSNORMALMATRIX+1, pos+4);
        SetVSConstant4fv(C_POSNORMALMATRIX+2, pos+8);
        SetVSConstant4fv(C_POSNORMALMATRIX+3, norm);
        SetVSConstant4fv(C_POSNORMALMATRIX+4, norm+3);
        SetVSConstant4fv(C_POSNORMALMATRIX+5, norm+6);
    }

    if (bTexMatricesChanged[0]) {
        bTexMatricesChanged[0] = false;
        float* fptrs[] = {
			(float*)xfmem + MatrixIndexA.Tex0MtxIdx * 4, (float*)xfmem + MatrixIndexA.Tex1MtxIdx * 4,
            (float*)xfmem + MatrixIndexA.Tex2MtxIdx * 4, (float*)xfmem + MatrixIndexA.Tex3MtxIdx * 4
		};

        for (int i = 0; i < 4; ++i) {
            SetVSConstant4fv(C_TEXMATRICES+3*i, fptrs[i]);
            SetVSConstant4fv(C_TEXMATRICES+3*i+1, fptrs[i]+4);
            SetVSConstant4fv(C_TEXMATRICES+3*i+2, fptrs[i]+8);
        }
    }

    if (bTexMatricesChanged[1]) {
        bTexMatricesChanged[1] = false;

        float* fptrs[] = {(float*)xfmem + MatrixIndexB.Tex4MtxIdx * 4, (float*)xfmem + MatrixIndexB.Tex5MtxIdx * 4,
            (float*)xfmem + MatrixIndexB.Tex6MtxIdx * 4, (float*)xfmem + MatrixIndexB.Tex7MtxIdx * 4 };

        for (int i = 0; i < 4; ++i) {
            SetVSConstant4fv(C_TEXMATRICES+3*i+12, fptrs[i]);
            SetVSConstant4fv(C_TEXMATRICES+3*i+12+1, fptrs[i]+4);
            SetVSConstant4fv(C_TEXMATRICES+3*i+12+2, fptrs[i]+8);
        }
    }

    if (bViewportChanged) {
        bViewportChanged = false;

		// This is so implementation-dependent that we can't have it here.
		UpdateViewport();
    }

    if (bProjectionChanged) {
        bProjectionChanged = false;
		static float GC_ALIGNED16(g_fProjectionMatrix[16]);


        if (xfregs.rawProjection[6] == 0) { // Perspective
            g_fProjectionMatrix[0] = xfregs.rawProjection[0];
            g_fProjectionMatrix[1] = 0.0f;
            g_fProjectionMatrix[2] = xfregs.rawProjection[1];
            g_fProjectionMatrix[3] = 0.0f;

            g_fProjectionMatrix[4] = 0.0f;
            g_fProjectionMatrix[5] = xfregs.rawProjection[2];
            g_fProjectionMatrix[6] = xfregs.rawProjection[3];
            g_fProjectionMatrix[7] = 0.0f;

            g_fProjectionMatrix[8] = 0.0f;
            g_fProjectionMatrix[9] = 0.0f;
            g_fProjectionMatrix[10] = xfregs.rawProjection[4];

			g_fProjectionMatrix[11] = xfregs.rawProjection[5];
 			
            g_fProjectionMatrix[12] = 0.0f;
            g_fProjectionMatrix[13] = 0.0f;
			// donkopunchstania: GC GPU rounds differently?
			// -(1 + epsilon) so objects are clipped as they are on the real HW
            g_fProjectionMatrix[14] = -1.00000011921f;
            g_fProjectionMatrix[15] = 0.0f;

			SETSTAT_FT(stats.gproj_0, g_fProjectionMatrix[0]);
			SETSTAT_FT(stats.gproj_1, g_fProjectionMatrix[1]);
			SETSTAT_FT(stats.gproj_2, g_fProjectionMatrix[2]);
			SETSTAT_FT(stats.gproj_3, g_fProjectionMatrix[3]);
			SETSTAT_FT(stats.gproj_4, g_fProjectionMatrix[4]);
			SETSTAT_FT(stats.gproj_5, g_fProjectionMatrix[5]);
			SETSTAT_FT(stats.gproj_6, g_fProjectionMatrix[6]);
			SETSTAT_FT(stats.gproj_7, g_fProjectionMatrix[7]);
			SETSTAT_FT(stats.gproj_8, g_fProjectionMatrix[8]);
			SETSTAT_FT(stats.gproj_9, g_fProjectionMatrix[9]);
			SETSTAT_FT(stats.gproj_10, g_fProjectionMatrix[10]);
			SETSTAT_FT(stats.gproj_11, g_fProjectionMatrix[11]);
			SETSTAT_FT(stats.gproj_12, g_fProjectionMatrix[12]);
			SETSTAT_FT(stats.gproj_13, g_fProjectionMatrix[13]);
			SETSTAT_FT(stats.gproj_14, g_fProjectionMatrix[14]);
			SETSTAT_FT(stats.gproj_15, g_fProjectionMatrix[15]);
        }
        else { // Orthographic Projection
            g_fProjectionMatrix[0] = xfregs.rawProjection[0];
            g_fProjectionMatrix[1] = 0.0f;
            g_fProjectionMatrix[2] = 0.0f;
            g_fProjectionMatrix[3] = xfregs.rawProjection[1];

            g_fProjectionMatrix[4] = 0.0f;
            g_fProjectionMatrix[5] = xfregs.rawProjection[2];
            g_fProjectionMatrix[6] = 0.0f;
            g_fProjectionMatrix[7] = xfregs.rawProjection[3];

            g_fProjectionMatrix[8] = 0.0f;
            g_fProjectionMatrix[9] = 0.0f;
            g_fProjectionMatrix[10] = (Hack_hack1 ? -(Hack_value1 + xfregs.rawProjection[4]) : xfregs.rawProjection[4]);   
            g_fProjectionMatrix[11] = (Hack_hack2 ? -(Hack_value2 + xfregs.rawProjection[5]) : xfregs.rawProjection[5]) + (0.0f);
            
            g_fProjectionMatrix[12] = 0.0f;
            g_fProjectionMatrix[13] = 0.0f;
            g_fProjectionMatrix[14] = 0.0f;
            g_fProjectionMatrix[15] = 1.0f;

			SETSTAT_FT(stats.g2proj_0, g_fProjectionMatrix[0]);
			SETSTAT_FT(stats.g2proj_1, g_fProjectionMatrix[1]);
			SETSTAT_FT(stats.g2proj_2, g_fProjectionMatrix[2]);
			SETSTAT_FT(stats.g2proj_3, g_fProjectionMatrix[3]);
			SETSTAT_FT(stats.g2proj_4, g_fProjectionMatrix[4]);
			SETSTAT_FT(stats.g2proj_5, g_fProjectionMatrix[5]);
			SETSTAT_FT(stats.g2proj_6, g_fProjectionMatrix[6]);
			SETSTAT_FT(stats.g2proj_7, g_fProjectionMatrix[7]);
			SETSTAT_FT(stats.g2proj_8, g_fProjectionMatrix[8]);
			SETSTAT_FT(stats.g2proj_9, g_fProjectionMatrix[9]);
			SETSTAT_FT(stats.g2proj_10, g_fProjectionMatrix[10]);
			SETSTAT_FT(stats.g2proj_11, g_fProjectionMatrix[11]);
			SETSTAT_FT(stats.g2proj_12, g_fProjectionMatrix[12]);
			SETSTAT_FT(stats.g2proj_13, g_fProjectionMatrix[13]);
			SETSTAT_FT(stats.g2proj_14, g_fProjectionMatrix[14]);
			SETSTAT_FT(stats.g2proj_15, g_fProjectionMatrix[15]);
			SETSTAT_FT(stats.proj_0, xfregs.rawProjection[0]);
			SETSTAT_FT(stats.proj_1, xfregs.rawProjection[1]);
			SETSTAT_FT(stats.proj_2, xfregs.rawProjection[2]);
			SETSTAT_FT(stats.proj_3, xfregs.rawProjection[3]);
			SETSTAT_FT(stats.proj_4, xfregs.rawProjection[4]);
			SETSTAT_FT(stats.proj_5, xfregs.rawProjection[5]);
			SETSTAT_FT(stats.proj_6, xfregs.rawProjection[6]);
        }

        PRIM_LOG("Projection: %f %f %f %f %f %f\n", xfregs.rawProjection[0], xfregs.rawProjection[1], xfregs.rawProjection[2], xfregs.rawProjection[3], xfregs.rawProjection[4], xfregs.rawProjection[5]);

        if (freeLook) {
            Matrix44 mtxA;
            Matrix44 mtxB;
            Matrix44 viewMtx;

            Matrix44::Translate(mtxA, s_fViewTranslationVector);
            Matrix44::LoadMatrix33(mtxB, s_viewRotationMatrix);
            Matrix44::Multiply(mtxB, mtxA, viewMtx); // view = rotation x translation
            Matrix44::Set(mtxB, g_fProjectionMatrix);
            Matrix44::Multiply(mtxB, viewMtx, mtxA); // mtxA = projection x view

            SetVSConstant4fv(C_PROJECTION,   &mtxA.data[0]);
            SetVSConstant4fv(C_PROJECTION+1, &mtxA.data[4]);
            SetVSConstant4fv(C_PROJECTION+2, &mtxA.data[8]);
            SetVSConstant4fv(C_PROJECTION+3, &mtxA.data[12]);
        }
        else {
		    SetVSConstant4fv(C_PROJECTION,   &g_fProjectionMatrix[0]);
            SetVSConstant4fv(C_PROJECTION+1, &g_fProjectionMatrix[4]);
            SetVSConstant4fv(C_PROJECTION+2, &g_fProjectionMatrix[8]);
            SetVSConstant4fv(C_PROJECTION+3, &g_fProjectionMatrix[12]);
        }
    }
}

void VertexShaderManager::InvalidateXFRange(int start, int end)
{
    if (((u32)start >= (u32)MatrixIndexA.PosNormalMtxIdx*4 &&
		 (u32)start <  (u32)MatrixIndexA.PosNormalMtxIdx*4 + 12) ||
        ((u32)start >= XFMEM_NORMALMATRICES + ((u32)MatrixIndexA.PosNormalMtxIdx & 31)*3 &&
		 (u32)start <  XFMEM_NORMALMATRICES + ((u32)MatrixIndexA.PosNormalMtxIdx & 31)*3 + 9)) {
        bPosNormalMatrixChanged = true;
    }

    if (((u32)start >= (u32)MatrixIndexA.Tex0MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex0MtxIdx*4+12) ||
        ((u32)start >= (u32)MatrixIndexA.Tex1MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex1MtxIdx*4+12) ||
        ((u32)start >= (u32)MatrixIndexA.Tex2MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex2MtxIdx*4+12) ||
        ((u32)start >= (u32)MatrixIndexA.Tex3MtxIdx*4 && (u32)start < (u32)MatrixIndexA.Tex3MtxIdx*4+12)) {
        bTexMatricesChanged[0] = true;
    }

    if (((u32)start >= (u32)MatrixIndexB.Tex4MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex4MtxIdx*4+12) ||
        ((u32)start >= (u32)MatrixIndexB.Tex5MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex5MtxIdx*4+12) ||
        ((u32)start >= (u32)MatrixIndexB.Tex6MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex6MtxIdx*4+12) ||
        ((u32)start >= (u32)MatrixIndexB.Tex7MtxIdx*4 && (u32)start < (u32)MatrixIndexB.Tex7MtxIdx*4+12)) {
        bTexMatricesChanged[1] = true;
    }

    if (start < XFMEM_POSMATRICES_END) {
        if (nTransformMatricesChanged[0] == -1) {
            nTransformMatricesChanged[0] = start;
            nTransformMatricesChanged[1] = end>XFMEM_POSMATRICES_END?XFMEM_POSMATRICES_END:end;
        }
        else {
            if (nTransformMatricesChanged[0] > start) nTransformMatricesChanged[0] = start;
            if (nTransformMatricesChanged[1] < end) nTransformMatricesChanged[1] = end>XFMEM_POSMATRICES_END?XFMEM_POSMATRICES_END:end;
        }
    }

    if (start < XFMEM_NORMALMATRICES_END && end > XFMEM_NORMALMATRICES) {
        int _start = start < XFMEM_NORMALMATRICES ? 0 : start-XFMEM_NORMALMATRICES;
        int _end = end < XFMEM_NORMALMATRICES_END ? end-XFMEM_NORMALMATRICES : XFMEM_NORMALMATRICES_END-XFMEM_NORMALMATRICES;

        if (nNormalMatricesChanged[0] == -1 ) {
            nNormalMatricesChanged[0] = _start;
            nNormalMatricesChanged[1] = _end;
        }
        else {
            if (nNormalMatricesChanged[0] > _start) nNormalMatricesChanged[0] = _start;
            if (nNormalMatricesChanged[1] < _end) nNormalMatricesChanged[1] = _end;
        }
    }

    if (start < XFMEM_POSTMATRICES_END && end > XFMEM_POSTMATRICES) {
        int _start = start < XFMEM_POSTMATRICES ? XFMEM_POSTMATRICES : start-XFMEM_POSTMATRICES;
        int _end = end < XFMEM_POSTMATRICES_END ? end-XFMEM_POSTMATRICES : XFMEM_POSTMATRICES_END-XFMEM_POSTMATRICES;

        if (nPostTransformMatricesChanged[0] == -1 ) {
            nPostTransformMatricesChanged[0] = _start;
            nPostTransformMatricesChanged[1] = _end;
        }
        else {
            if (nPostTransformMatricesChanged[0] > _start) nPostTransformMatricesChanged[0] = _start;
            if (nPostTransformMatricesChanged[1] < _end) nPostTransformMatricesChanged[1] = _end;
        }
    }

    if (start < XFMEM_LIGHTS_END && end > XFMEM_LIGHTS) {
        int _start = start < XFMEM_LIGHTS ? XFMEM_LIGHTS : start-XFMEM_LIGHTS;
        int _end = end < XFMEM_LIGHTS_END ? end-XFMEM_LIGHTS : XFMEM_LIGHTS_END-XFMEM_LIGHTS;

        if (nLightsChanged[0] == -1 ) {
            nLightsChanged[0] = _start;
            nLightsChanged[1] = _end;
        }
        else {
            if (nLightsChanged[0] > _start) nLightsChanged[0] = _start;
            if (nLightsChanged[1] < _end)   nLightsChanged[1] = _end;
        }
    }
}

void VertexShaderManager::SetTexMatrixChangedA(u32 Value)
{
    if (MatrixIndexA.Hex != Value) {
        VertexManager::Flush();
        if (MatrixIndexA.PosNormalMtxIdx != (Value&0x3f))
            bPosNormalMatrixChanged = true;
        bTexMatricesChanged[0] = true;
        MatrixIndexA.Hex = Value;
    }
}

void VertexShaderManager::SetTexMatrixChangedB(u32 Value)
{
    if (MatrixIndexB.Hex != Value) {
        VertexManager::Flush();
        bTexMatricesChanged[1] = true;
        MatrixIndexB.Hex = Value;
    }
}

void VertexShaderManager::SetViewport(float* _Viewport)
{
    // Workaround for paper mario, yep this is bizarre.
    for (size_t i = 0; i < ARRAYSIZE(xfregs.rawViewport); ++i) {
        if (*(u32*)(_Viewport + i) == 0x7f800000)  // invalid fp number
            return;
    }
    memcpy(xfregs.rawViewport, _Viewport, sizeof(xfregs.rawViewport));
    bViewportChanged = true;
}

void VertexShaderManager::SetViewportChanged()
{
    bViewportChanged = true;
}

void VertexShaderManager::SetProjection(float* _pProjection, int constantIndex)
{
    memcpy(xfregs.rawProjection, _pProjection, sizeof(xfregs.rawProjection));
    bProjectionChanged = true;
}

void VertexShaderManager::SetMaterialColor(int index, u32 data)
{
	int ind = index * 4;

	nMaterialsChanged  |= (1 << index);

	s_fMaterials[ind++] = ((data>>24)&0xFF)/255.0f;
	s_fMaterials[ind++] = ((data>>16)&0xFF)/255.0f;
	s_fMaterials[ind++] = ((data>>8)&0xFF)/255.0f;
	s_fMaterials[ind]   = ((data)&0xFF)/255.0f;
}

void VertexShaderManager::TranslateView(float x, float y)
{
    float result[3];
    float vector[3] = { x,0,y };

    Matrix33::Multiply(s_viewInvRotationMatrix, vector, result);

    for(int i = 0; i < 3; i++) {
        s_fViewTranslationVector[i] += result[i];
    }

    bProjectionChanged = true;
}

void VertexShaderManager::RotateView(float x, float y)
{
    s_fViewRotation[0] += x;
    s_fViewRotation[1] += y;

    Matrix33 mx;
    Matrix33 my;
    Matrix33::RotateX(mx, s_fViewRotation[1]);
    Matrix33::RotateY(my, s_fViewRotation[0]);
    Matrix33::Multiply(mx, my, s_viewRotationMatrix);

    // reverse rotation
    Matrix33::RotateX(mx, -s_fViewRotation[1]);
    Matrix33::RotateY(my, -s_fViewRotation[0]);
    Matrix33::Multiply(my, mx, s_viewInvRotationMatrix);

    bProjectionChanged = true;
}

void VertexShaderManager::ResetView()
{
    memset(s_fViewTranslationVector, 0, sizeof(s_fViewTranslationVector));
    Matrix33::LoadIdentity(s_viewRotationMatrix);
    Matrix33::LoadIdentity(s_viewInvRotationMatrix);
    s_fViewRotation[0] = s_fViewRotation[1] = 0.0f;

    bProjectionChanged = true;
}
