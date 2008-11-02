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
#include "Config.h"

#include "GLUtil.h"

#include <Cg/cg.h>
#include <Cg/cgGL.h>

#include <math.h>

#include "Statistics.h"
#include "ImageWrite.h"
#include "Render.h"
#include "VertexShader.h"
#include "VertexShaderManager.h"
#include "VertexManager.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"

VertexShaderMngr::VSCache VertexShaderMngr::vshaders;
VERTEXSHADER* VertexShaderMngr::pShaderLast = NULL;

float GC_ALIGNED16(g_fProjectionMatrix[16]);

extern int A, B;
extern float AR;
extern int nBackbufferWidth, nBackbufferHeight;

// Internal Variables
static int s_nMaxVertexInstructions;

static float s_fMaterials[16];

// track changes
static bool bTexMatricesChanged[2], bPosNormalMatrixChanged, bProjectionChanged, bViewportChanged;
static int nMaterialsChanged;
static int nTransformMatricesChanged[2]; // min,max
static int nNormalMatricesChanged[2]; // min,max
static int nPostTransformMatricesChanged[2]; // min,max
static int nLightsChanged[2]; // min,max

void VertexShaderMngr::SetVSConstant4f(int const_number, float f1, float f2, float f3, float f4) {
    glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, const_number, f1, f2, f3, f4);
}

void VertexShaderMngr::SetVSConstant4fv(int const_number, const float *f) {
	glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, const_number, f);
}

void VertexShaderMngr::Init()
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

    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, (GLint *)&s_nMaxVertexInstructions);
}

void VertexShaderMngr::Shutdown()
{
    for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end(); iter++)
        iter->second.Destroy();
    vshaders.clear();
}

float VertexShaderMngr::GetPixelAspectRatio() {
	return xfregs.rawViewport[0] != 0 ? (float)Renderer::GetTargetWidth() / 640.0f : 1.0f;
}

VERTEXSHADER* VertexShaderMngr::GetShader(u32 components)
{
    DVSTARTPROFILE();
    VERTEXSHADERUID uid;
    GetVertexShaderId(uid, components);

    VSCache::iterator iter = vshaders.find(uid);

    if (iter != vshaders.end()) {
        iter->second.frameCount = frameCount;
        VSCacheEntry &entry = iter->second;
        if (&entry.shader != pShaderLast) {
            pShaderLast = &entry.shader;
        }
        return pShaderLast;
    }

    VSCacheEntry& entry = vshaders[uid];
    char *code = GenerateVertexShader(components, Renderer::GetZBufferTarget() != 0);

#if defined(_DEBUG) || defined(DEBUGFAST)
    if (g_Config.iLog & CONF_SAVESHADERS && code) {
        static int counter = 0;
        char szTemp[MAX_PATH];
		sprintf(szTemp, "%s/vs_%04i.txt", g_Config.texDumpPath, counter++);
        
        SaveData(szTemp, code);
    }
#endif

    if (!code || !VertexShaderMngr::CompileVertexShader(entry.shader, code)) {
        ERROR_LOG("failed to create vertex shader\n");
		return NULL;
	}

    //Make an entry in the table
    entry.frameCount=frameCount;

    pShaderLast = &entry.shader;
    INCSTAT(stats.numVertexShadersCreated);
    SETSTAT(stats.numVertexShadersAlive,vshaders.size());
    return pShaderLast;
}

void VertexShaderMngr::Cleanup()
{
    VSCache::iterator iter=vshaders.begin();
    while (iter != vshaders.end()) {
        VSCacheEntry &entry = iter->second;
        if (entry.frameCount < frameCount-200) {
            entry.Destroy();
#ifdef _WIN32
            iter = vshaders.erase(iter);
#else
			vshaders.erase(iter++);
#endif
        }
        else {
            ++iter;
		}
    }

//    static int frame = 0;
//    if( frame++ > 30 ) {
//        VSCache::iterator iter=vshaders.begin();
//        while(iter!=vshaders.end()) {
//            iter->second.Destroy();
//            ++iter;
//        }
//        vshaders.clear();
//    }

    SETSTAT(stats.numPixelShadersAlive,vshaders.size());
}

bool VertexShaderMngr::CompileVertexShader(VERTEXSHADER& vs, const char* pstrprogram)
{
    char stropt[64];
    sprintf(stropt, "MaxLocalParams=256,MaxInstructions=%d", s_nMaxVertexInstructions);
    const char *opts[] = {"-profileopts", stropt, "-O2", "-q", NULL};
    CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgvProf, "main", opts);
    if (!cgIsProgram(tempprog) || cgGetError() != CG_NO_ERROR) {
        ERROR_LOG("Failed to load vs %s:\n", cgGetLastListing(g_cgcontext));
        ERROR_LOG(pstrprogram);
        return false;
    }

    //ERROR_LOG(pstrprogram);
    //ERROR_LOG("id: %d\n", g_Config.iSaveTargetId);

    char* pcompiledprog = (char*)cgGetProgramString(tempprog, CG_COMPILED_PROGRAM);
    char* plocal = strstr(pcompiledprog, "program.local");

    while (plocal != NULL) {
        const char* penv = "  program.env";
        memcpy(plocal, penv, 13);
        plocal = strstr(plocal+13, "program.local");
    }

    glGenProgramsARB(1, &vs.glprogid);
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vs.glprogid);
    glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pcompiledprog), pcompiledprog);

    GLenum err = GL_NO_ERROR;
    GL_REPORT_ERROR();
    if( err != GL_NO_ERROR ) {
        ERROR_LOG(pstrprogram);
        ERROR_LOG(pcompiledprog);
    }

    cgDestroyProgram(tempprog);
	// printf("Compiled vertex shader %i\n", vs.glprogid);

#ifdef _DEBUG
    vs.strprog = pstrprogram;
#endif

    return true;
}

const u16 s_mtrltable[16][2] = {{0, 0}, {0, 1}, {1, 1}, {0, 2},
                                {2, 1}, {0, 3}, {1, 2}, {0, 3},
                                {3, 1}, {0, 4}, {1, 3}, {0, 4},
                                {2, 2}, {0, 4}, {1, 3}, {0, 4}};


// =======================================================================================
// Syncs the shader constant buffers with xfmem
// ----------------
void VertexShaderMngr::SetConstants()
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

        // reversed gxsetviewport(xorig, yorig, width, height, nearz, farz)
        // [0] = width/2
        // [1] = height/2
        // [2] = 16777215 * (farz-nearz)
        // [3] = xorig + width/2 + 342
        // [4] = yorig + height/2 + 342
        // [5] = 16777215 * farz
		/*INFO_LOG("view: topleft=(%f,%f), wh=(%f,%f), z=(%f,%f)\n",
			rawViewport[3]-rawViewport[0]-342, rawViewport[4]+rawViewport[1]-342,
			2 * rawViewport[0], 2 * rawViewport[1],
			(rawViewport[5] - rawViewport[2]) / 16777215.0f, rawViewport[5] / 16777215.0f);*/

		// Keep aspect ratio at 4:3
		// rawViewport[0] = 320, rawViewport[1] = -240
		int scissorXOff = bpmem.scissorOffset.x * 2 - 342;
		int scissorYOff = bpmem.scissorOffset.y * 2 - 342;
		float fourThree = 4.0f / 3.0f;
		float ratio = AR /  fourThree;
		float wAdj, hAdj;
		float actualRatiow, actualRatioh;
		int overfl;
		int xoffs = 0, yoffs = 0;
		int wid, hei, actualWid, actualHei;
		int winw = nBackbufferWidth;
		int winh = nBackbufferHeight;
		if (g_Config.bKeepAR)
		{
			// Check if height or width is the limiting factor
			if (ratio > 1) // then we are to wide and have to limit the width
			{			
				wAdj = ratio;
				hAdj = 1;

				wid = ceil(fabs(2 * xfregs.rawViewport[0]) / wAdj);
				hei = ceil(fabs(2 * xfregs.rawViewport[1]) / hAdj);

				actualWid = ceil((float)winw / ratio);
				actualRatiow = (float)actualWid / (float)wid; // the picture versus the screen
				overfl = (winw - actualWid) / actualRatiow;
				xoffs = overfl / 2;
			}
			else // the window is to high, we have to limit the height
			{
				ratio = 1 / ratio;

				wAdj = 1;
				hAdj = ratio;

				wid = ceil(fabs(2 * xfregs.rawViewport[0]) / wAdj);
				hei = ceil(fabs(2 * xfregs.rawViewport[1]) / hAdj);

				actualHei = ceil((float)winh / ratio);
				actualRatioh = (float)actualHei / (float)hei; // the picture versus the screen
				overfl = (winh - actualHei) / actualRatioh;
				yoffs = overfl / 2;
			}
		}
		else
		{
			wid = ceil(fabs(2 * xfregs.rawViewport[0]));
			hei = ceil(fabs(2 * xfregs.rawViewport[1]));
		}

		if (g_Config.bStretchToFit)
		{
			glViewport(
				(int)(xfregs.rawViewport[3]-xfregs.rawViewport[0]-342-scissorXOff) + xoffs,
				Renderer::GetTargetHeight() - ((int)(xfregs.rawViewport[4]-xfregs.rawViewport[1]-342-scissorYOff)) + yoffs,
				wid, // width
				hei // height
				);
		}
		else
		{
			glViewport((int)(xfregs.rawViewport[3]-xfregs.rawViewport[0]-342-scissorXOff) * MValueX,
				Renderer::GetTargetHeight()-((int)(xfregs.rawViewport[4]-xfregs.rawViewport[1]-342-scissorYOff))  * MValueY,
				abs((int)(2 * xfregs.rawViewport[0])) * MValueX, abs((int)(2 * xfregs.rawViewport[1])) * MValueY);
		}

		// Standard depth range
		//glDepthRange(-(0.0f - (xfregs.rawViewport[5]-xfregs.rawViewport[2])/-16777215.0f), xfregs.rawViewport[5]/16777215.0f);

		// Metroid Prime 1 & 2 likes this
		//glDepthRange(-(0.0f - (xfregs.rawViewport[5]-xfregs.rawViewport[2])/16777215.0f), xfregs.rawViewport[5]/16777215.0f);
		glDepthRange((g_Config.bInvertDepth ? -1 : 1) * -(0.0f - (xfregs.rawViewport[5]-xfregs.rawViewport[2])/16777215.0f), xfregs.rawViewport[5]/16777215.0f);

		// FZero stage likes this (a sonic hack)
		// glDepthRange(-(0.0f - (xfregs.rawViewport[5]-xfregs.rawViewport[2])/-16777215.0f), xfregs.rawViewport[5]/16777215.0f);
    }

    if (bProjectionChanged) {
        bProjectionChanged = false;

        if (xfregs.rawProjection[6] == 0) {
            g_fProjectionMatrix[0] = xfregs.rawProjection[0];
            g_fProjectionMatrix[1] = 0.0f;
            g_fProjectionMatrix[2] = xfregs.rawProjection[1];
            g_fProjectionMatrix[3] = 0;
                        
            g_fProjectionMatrix[4] = 0.0f;
            g_fProjectionMatrix[5] = xfregs.rawProjection[2];
            g_fProjectionMatrix[6] = xfregs.rawProjection[3];
            g_fProjectionMatrix[7] = 0;
                        
            g_fProjectionMatrix[8] = 0.0f;
            g_fProjectionMatrix[9] = 0.0f;
            g_fProjectionMatrix[10] = xfregs.rawProjection[4];

			if((!g_Config.bProjectionHax1 && !g_Config.bProjectionHax2) || (g_Config.bProjectionHax1 && g_Config.bProjectionHax2)) {
			// Working bloom in ZTP
            g_fProjectionMatrix[11] = -(0.0f - xfregs.rawProjection[5]);  // Yes, it's important that it's done this way.
			// Working projection in PSO
			// g_fProjectionMatrix[11] = -(1.0f - rawProjection[5]); 
			}

			if(g_Config.bProjectionHax1 && !g_Config.bProjectionHax2) // Before R945
				g_fProjectionMatrix[11] = -(1.0f -  xfregs.rawProjection[5]); 

			if(!g_Config.bProjectionHax1 && g_Config.bProjectionHax2) // R844
				g_fProjectionMatrix[11] =  xfregs.rawProjection[5];

            g_fProjectionMatrix[12] = 0.0f;
            g_fProjectionMatrix[13] = 0.0f;
			// donkopunchstania: GC GPU rounds differently?
			// -(1 + epsilon) so objects are clipped as they are on the real HW
            g_fProjectionMatrix[14] = -1.00000011921f;
            g_fProjectionMatrix[15] = 0.0f;
        }
        else {
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
            g_fProjectionMatrix[10] = xfregs.rawProjection[4];

			if((!g_Config.bProjectionHax1 && !g_Config.bProjectionHax2) || (g_Config.bProjectionHax1 && g_Config.bProjectionHax2)) {
			// Working bloom in ZTP
            g_fProjectionMatrix[11] = -(-1.0f - xfregs.rawProjection[5]);  // Yes, it's important that it's done this way.
			// Working projection in PSO, working Super Monkey Ball
			// g_fProjectionMatrix[11] = -(0.0f - xfregs.rawProjection[5]);
			}

			if(g_Config.bProjectionHax1 && !g_Config.bProjectionHax2) // Before R945
				g_fProjectionMatrix[11] = -(0.0f - xfregs.rawProjection[5]);

			if(!g_Config.bProjectionHax1 && g_Config.bProjectionHax2) // R844
				g_fProjectionMatrix[11] = -xfregs.rawProjection[5];

            g_fProjectionMatrix[12] = 0;
            g_fProjectionMatrix[13] = 0;
            g_fProjectionMatrix[14] = 0.0f;
            g_fProjectionMatrix[15] = 1.0f;
        }

        PRIM_LOG("Projection: %f %f %f %f %f %f\n", xfregs.rawProjection[0], xfregs.rawProjection[1], xfregs.rawProjection[2], xfregs.rawProjection[3], xfregs.rawProjection[4], xfregs.rawProjection[5]);
        SetVSConstant4fv(C_PROJECTION,   &g_fProjectionMatrix[0]);
        SetVSConstant4fv(C_PROJECTION+1, &g_fProjectionMatrix[4]);
        SetVSConstant4fv(C_PROJECTION+2, &g_fProjectionMatrix[8]);
        SetVSConstant4fv(C_PROJECTION+3, &g_fProjectionMatrix[12]);
    }
}

void VertexShaderMngr::InvalidateXFRange(int start, int end)
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

void VertexShaderMngr::SetTexMatrixChangedA(u32 Value)
{
    if (MatrixIndexA.Hex != Value) {
        VertexManager::Flush();
        if (MatrixIndexA.PosNormalMtxIdx != (Value&0x3f))
            bPosNormalMatrixChanged = true;
        bTexMatricesChanged[0] = true;
        MatrixIndexA.Hex = Value;
    }
}

void VertexShaderMngr::SetTexMatrixChangedB(u32 Value)
{
    if (MatrixIndexB.Hex != Value) {
        VertexManager::Flush();
        bTexMatricesChanged[1] = true;
        MatrixIndexB.Hex = Value;
    }
}

void VertexShaderMngr::SetViewport(float* _Viewport)
{
    // Workaround for paper mario, yep this is bizarre.
    for (size_t i = 0; i < ARRAYSIZE(xfregs.rawViewport); ++i) {
        if (*(u32*)(_Viewport + i) == 0x7f800000)  // invalid fp number
            return;
    }
    memcpy(xfregs.rawViewport, _Viewport, sizeof(xfregs.rawViewport));
    bViewportChanged = true;
}

void VertexShaderMngr::SetViewportChanged()
{
    bViewportChanged = true;
}

void VertexShaderMngr::SetProjection(float* _pProjection, int constantIndex)
{
    memcpy(xfregs.rawProjection, _pProjection, sizeof(xfregs.rawProjection));
    bProjectionChanged = true;
}

float* VertexShaderMngr::GetPosNormalMat()
{
    return (float*)xfmem + MatrixIndexA.PosNormalMtxIdx * 4;
}

// Mash together all the inputs that contribute to the code of a generated vertex shader into
// a unique identifier, basically containing all the bits. Yup, it's a lot ....
void VertexShaderMngr::GetVertexShaderId(VERTEXSHADERUID& vid, u32 components)
{
	u32 zbufrender = (bpmem.ztex2.op == ZTEXTURE_ADD) || Renderer::GetZBufferTarget() != 0;
	vid.values[0] = components | 
		(xfregs.numTexGens << 23) |
		(xfregs.nNumChans << 27) |
		((u32)xfregs.bEnableDualTexTransform << 29) |
		(zbufrender << 30);

	for (int i = 0; i < 2; ++i) {
		vid.values[1+i] = xfregs.colChans[i].color.enablelighting ? 
			(u32)xfregs.colChans[i].color.hex : 
			(u32)xfregs.colChans[i].color.matsource;
		vid.values[1+i] |= (xfregs.colChans[i].alpha.enablelighting ? 
			(u32)xfregs.colChans[i].alpha.hex : 
			(u32)xfregs.colChans[i].alpha.matsource) << 15;
	}

	// fog
	vid.values[1] |= (((u32)bpmem.fog.c_proj_fsel.fsel & 3) << 30);
	vid.values[2] |= (((u32)bpmem.fog.c_proj_fsel.fsel >> 2) << 30);

	u32* pcurvalue = &vid.values[3];
	for (int i = 0; i < xfregs.numTexGens; ++i) {
		TexMtxInfo tinfo = xfregs.texcoords[i].texmtxinfo;
		if (tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP)
			tinfo.hex &= 0x7ff;
		if (tinfo.texgentype != XF_TEXGEN_REGULAR)
			tinfo.projection = 0;

		u32 val = ((tinfo.hex >> 1) & 0x1ffff);
		if (xfregs.bEnableDualTexTransform && tinfo.texgentype == XF_TEXGEN_REGULAR) {
			// rewrite normalization and post index
			val |= ((u32)xfregs.texcoords[i].postmtxinfo.index << 17) | ((u32)xfregs.texcoords[i].postmtxinfo.normalize << 23);
		}

		switch (i & 3) {
			case 0: pcurvalue[0] |= val; break;
			case 1: pcurvalue[0] |= val << 24; pcurvalue[1] = val >> 8; ++pcurvalue; break;
			case 2: pcurvalue[0] |= val << 16; pcurvalue[1] = val >> 16; ++pcurvalue; break;
			case 3: pcurvalue[0] |= val << 8; ++pcurvalue; break;
		}
	}
}


// LoadXFReg 0x10
void LoadXFReg(u32 transferSize, u32 baseAddress, u32 *pData)
{	
    u32 address = baseAddress;
    for (int i = 0; i < (int)transferSize; i++)
    {
        address = baseAddress + i;

        // Setup a Matrix
        if (address < 0x1000)
        {
            VertexManager::Flush();
			VertexShaderMngr::InvalidateXFRange(address, address + transferSize);
            //PRIM_LOG("xfmem write: 0x%x-0x%x\n", address, address+transferSize);

            u32* p1 = &xfmem[address];
            memcpy_gc(p1, &pData[i], transferSize*4);
            i += transferSize;
        }
        else if (address<0x2000)
        {
            u32 data = pData[i];	
            switch (address)
            {
            case 0x1000: // error
                break;
            case 0x1001: // diagnostics
                break;
            case 0x1002: // internal state 0
                break;
            case 0x1003: // internal state 1
                break;
            case 0x1004: // xf_clock
                break;
            case 0x1005: // clipdisable
                if (data & 1) { // disable clipping detection
                }
                if (data & 2) { // disable trivial rejection
                }
                if (data & 4) { // disable cpoly clipping acceleration
                }
                break;
            case 0x1006: //SetGPMetric
                break;
            case 0x1008: //__GXXfVtxSpecs, wrote 0004
                xfregs.hostinfo = *(INVTXSPEC*)&data;
                break;
            case 0x1009: //GXSetNumChans (no)
				if ((u32)xfregs.nNumChans != (data&3)) {
                    VertexManager::Flush();
                    xfregs.nNumChans = data&3;
                }
                break;
            case 0x100a: //GXSetChanAmbientcolor
                if (xfregs.colChans[0].ambColor != data) {
                    VertexManager::Flush();
                    nMaterialsChanged |= 1;
                    xfregs.colChans[0].ambColor = data;
                    s_fMaterials[0] = ((data>>24)&0xFF)/255.0f; 
                    s_fMaterials[1] = ((data>>16)&0xFF)/255.0f; 
                    s_fMaterials[2] = ((data>>8)&0xFF)/255.0f;
                    s_fMaterials[3] = ((data)&0xFF)/255.0f;
                }
                break; 
            case 0x100b: //GXSetChanAmbientcolor
                if (xfregs.colChans[1].ambColor != data) {
                    VertexManager::Flush();
                    nMaterialsChanged |= 2;
                    xfregs.colChans[1].ambColor = data;
                    s_fMaterials[4] = ((data>>24)&0xFF)/255.0f; 
                    s_fMaterials[5] = ((data>>16)&0xFF)/255.0f; 
                    s_fMaterials[6] = ((data>>8)&0xFF)/255.0f;
                    s_fMaterials[7] = ((data)&0xFF)/255.0f;
                }
                break;
            case 0x100c: //GXSetChanMatcolor (rgba)
                if (xfregs.colChans[0].matColor != data) {
                    VertexManager::Flush();
                    nMaterialsChanged |= 4;
                    xfregs.colChans[0].matColor = data;
                    s_fMaterials[8] = ((data>>24)&0xFF)/255.0f; 
                    s_fMaterials[9] = ((data>>16)&0xFF)/255.0f; 
                    s_fMaterials[10] = ((data>>8)&0xFF)/255.0f;
                    s_fMaterials[11] = ((data)&0xFF)/255.0f;
                }
                break;
            case 0x100d: //GXSetChanMatcolor (rgba)
                if (xfregs.colChans[1].matColor != data) {
                    VertexManager::Flush();
                    nMaterialsChanged |= 8;
                    xfregs.colChans[1].matColor = data;
                    s_fMaterials[12] = ((data>>24)&0xFF)/255.0f; 
                    s_fMaterials[13] = ((data>>16)&0xFF)/255.0f; 
                    s_fMaterials[14] = ((data>>8)&0xFF)/255.0f;
                    s_fMaterials[15] = ((data)&0xFF)/255.0f;
                }
                break;

            case 0x100e: // color0
                if (xfregs.colChans[0].color.hex != (data&0x7fff) ) {
                    VertexManager::Flush();
                    xfregs.colChans[0].color.hex = data;
                }
                break;
            case 0x100f: // color1
                if (xfregs.colChans[1].color.hex != (data&0x7fff) ) {
                    VertexManager::Flush();
                    xfregs.colChans[1].color.hex = data;
                }
                break;
            case 0x1010: // alpha0
                if (xfregs.colChans[0].alpha.hex != (data&0x7fff) ) {
                    VertexManager::Flush();
                    xfregs.colChans[0].alpha.hex = data;
                }
                break;
            case 0x1011: // alpha1
                if (xfregs.colChans[1].alpha.hex != (data&0x7fff) ) {
                    VertexManager::Flush();
                    xfregs.colChans[1].alpha.hex = data;
                }
                break;
            case 0x1012: // dual tex transform
                if (xfregs.bEnableDualTexTransform != (data&1)) {
                    VertexManager::Flush();
                    xfregs.bEnableDualTexTransform = data&1;
                }
                break;

            case 0x1013:
            case 0x1014:
            case 0x1015:
            case 0x1016:
            case 0x1017:
                DEBUG_LOG("xf addr: %x=%x\n", address, data);
                break;
            case 0x1018:
                //_assert_msg_(GX_XF, 0, "XF matrixindex0");
				VertexShaderMngr::SetTexMatrixChangedA(data); //?
                break;
            case 0x1019:
                //_assert_msg_(GX_XF, 0, "XF matrixindex1");
                VertexShaderMngr::SetTexMatrixChangedB(data); //?
                break;

            case 0x101a: 
                VertexManager::Flush();
                VertexShaderMngr::SetViewport((float*)&pData[i]);
                i += 6;
                break;

            case 0x101c: // paper mario writes 16777216.0f, 1677721.75
                break;
            case 0x101f: // paper mario writes 16777216.0f, 5033165.0f  
                break;

            case 0x1020: 
                VertexManager::Flush();
                VertexShaderMngr::SetProjection((float*)&pData[i]);
                i += 7;
                return;

            case 0x103f: // GXSetNumTexGens
                if ((u32)xfregs.numTexGens != data) {
                    VertexManager::Flush();
                    xfregs.numTexGens = data;
                }
                break;

            case 0x1040: xfregs.texcoords[0].texmtxinfo.hex = data; break;
            case 0x1041: xfregs.texcoords[1].texmtxinfo.hex = data; break;
            case 0x1042: xfregs.texcoords[2].texmtxinfo.hex = data; break;
            case 0x1043: xfregs.texcoords[3].texmtxinfo.hex = data; break;
            case 0x1044: xfregs.texcoords[4].texmtxinfo.hex = data; break;
            case 0x1045: xfregs.texcoords[5].texmtxinfo.hex = data; break;
            case 0x1046: xfregs.texcoords[6].texmtxinfo.hex = data; break;
            case 0x1047: xfregs.texcoords[7].texmtxinfo.hex = data; break;

            case 0x1048:
            case 0x1049:
            case 0x104a:
            case 0x104b:
            case 0x104c:
            case 0x104d:
            case 0x104e:
            case 0x104f:
                DEBUG_LOG("xf addr: %x=%x\n", address, data);
                break;
            case 0x1050: xfregs.texcoords[0].postmtxinfo.hex = data; break;
            case 0x1051: xfregs.texcoords[1].postmtxinfo.hex = data; break;
            case 0x1052: xfregs.texcoords[2].postmtxinfo.hex = data; break;
            case 0x1053: xfregs.texcoords[3].postmtxinfo.hex = data; break;
            case 0x1054: xfregs.texcoords[4].postmtxinfo.hex = data; break;
            case 0x1055: xfregs.texcoords[5].postmtxinfo.hex = data; break;
            case 0x1056: xfregs.texcoords[6].postmtxinfo.hex = data; break;
            case 0x1057: xfregs.texcoords[7].postmtxinfo.hex = data; break;

            default:
                DEBUG_LOG("xf addr: %x=%x\n", address, data);
                break;
            }
        }
        else if (address >= 0x4000)
        {
            // MessageBox(NULL, "1", "1", MB_OK);
            //4010 __GXSetGenMode
        }
    }
}

// TODO - verify that it is correct. Seems to work, though.
void LoadIndexedXF(u32 val, int array)
{
    int index = val >> 16;
    int address = val & 0xFFF; //check mask
    int size = ((val >> 12) & 0xF) + 1;
    //load stuff from array to address in xf mem

    VertexManager::Flush();
    VertexShaderMngr::InvalidateXFRange(address, address+size);
    //PRIM_LOG("xfmem iwrite: 0x%x-0x%x\n", address, address+size);

    for (int i = 0; i < size; i++)
        xfmem[address + i] = Memory_Read_U32(arraybases[array] + arraystrides[array]*index + i*4);
}
