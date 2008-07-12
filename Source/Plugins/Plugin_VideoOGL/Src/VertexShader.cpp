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
#include <math.h>
#include "Render.h"
#include "VertexShader.h"
#include "BPStructs.h"
#include "VertexLoader.h"

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

VertexShaderMngr::VSCache VertexShaderMngr::vshaders;
VERTEXSHADER* VertexShaderMngr::pShaderLast = NULL;
TMatrixIndexA VertexShaderMngr::MatrixIndexA;
TMatrixIndexB VertexShaderMngr::MatrixIndexB;
float VertexShaderMngr::rawViewport[6] = {0};
float VertexShaderMngr::rawProjection[7] = {0};
float GC_ALIGNED16(g_fProjectionMatrix[16]);

static int s_nMaxVertexInstructions;

////////////////////////
// Internal Variables //
////////////////////////
XFRegisters xfregs;
static u32 xfmem[XFMEM_SIZE];
static float s_fMaterials[16];

// track changes
static bool bTexMatricesChanged[2], bPosNormalMatrixChanged, bProjectionChanged, bViewportChanged;
int nMaterialsChanged;
static int nTransformMatricesChanged[2]; // min,max
static int nNormalMatricesChanged[2]; // min,max
static int nPostTransformMatricesChanged[2]; // min,max
static int nLightsChanged[2]; // min,max

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

    glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, &s_nMaxVertexInstructions);
}

void VertexShaderMngr::Shutdown()
{
    VSCache::iterator iter = vshaders.begin();
    for (;iter!=vshaders.end();iter++)
        iter->second.Destroy();
    vshaders.clear();
}

VERTEXSHADER* VertexShaderMngr::GetShader(u32 components)
{
    DVSTARTPROFILE();
    VERTEXSHADERUID uid;
    GetVertexShaderId(uid, components);

    VSCache::iterator iter = vshaders.find(uid);

    if (iter != vshaders.end()) {
        iter->second.frameCount=frameCount;
        VSCacheEntry &entry = iter->second;
        if (&entry.shader != pShaderLast) {
            pShaderLast = &entry.shader;
        }
        return pShaderLast;
    }

    VSCacheEntry& entry = vshaders[uid];
        
    if (!GenerateVertexShader(entry.shader, components)) {
        ERROR_LOG("failed to create vertex shader\n");
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
#ifdef _WIN32
    const char* opts[] = {"-profileopts",stropt,"-O2", "-q", NULL};
#else
    const char* opts[] = {"-profileopts",stropt,"-q", NULL};
#endif
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

    while( plocal != NULL ) {
        const char* penv = "  program.env";
        memcpy(plocal, penv, 13);
        plocal = strstr(plocal+13, "program.local");
    }

    glGenProgramsARB( 1, &vs.glprogid );
    glBindProgramARB( GL_VERTEX_PROGRAM_ARB, vs.glprogid );
    glProgramStringARB( GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pcompiledprog), pcompiledprog);

    GLenum err = GL_NO_ERROR;
    GL_REPORT_ERROR();
    if( err != GL_NO_ERROR ) {
        ERROR_LOG(pstrprogram);
        ERROR_LOG(pcompiledprog);
    }

    cgDestroyProgram(tempprog);

#ifdef _DEBUG
    vs.strprog = pstrprogram;
#endif

    return true;
}

// TODO: this array is misdeclared. Why teh f** does it go through the compilers?
const u16 s_mtrltable[16][2] = {0, 0, 0, 1, 1, 1, 0, 2,
                                2, 1, 0, 3, 1, 2, 0, 3,
                                3, 1, 0, 4, 1, 3, 0, 4,
                                2, 2, 0, 4, 1, 3, 0, 4};

/// syncs the shader constant buffers with xfmem
void VertexShaderMngr::SetConstants(VERTEXSHADER& vs)
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
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_TRANSFORMMATRICES+i, pstart);
        nTransformMatricesChanged[0] = nTransformMatricesChanged[1] = -1;
    }
    if (nNormalMatricesChanged[0] >= 0) {
        int startn = nNormalMatricesChanged[0]/3;
        int endn = (nNormalMatricesChanged[1]+2)/3;
        const float* pnstart = (const float*)&xfmem[XFMEM_NORMALMATRICES+3*startn];

        for(int i = startn; i < endn; ++i, pnstart += 3)
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_NORMALMATRICES+i, pnstart);

        nNormalMatricesChanged[0] = nNormalMatricesChanged[1] = -1;
    }

    if (nPostTransformMatricesChanged[0] >= 0) {
        int startn = nPostTransformMatricesChanged[0]/4;
        int endn = (nPostTransformMatricesChanged[1]+3)/4;
        const float* pstart = (const float*)&xfmem[XFMEM_POSTMATRICES+startn*4];
        for(int i = startn; i < endn; ++i, pstart += 4)
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_POSTTRANSFORMMATRICES+i, pstart);
    }

    if (nLightsChanged[0] >= 0) {
        // lights don't have a 1 to 1 mapping, the color component needs to be converted to 4 floats
        int istart = nLightsChanged[0]/0x10;
        int iend = (nLightsChanged[1]+15)/0x10;
        const float* xfmemptr = (const float*)&xfmem[0x10*istart+XFMEM_LIGHTS];
        
        for(int i = istart; i < iend; ++i) {
            u32 color = *(const u32*)(xfmemptr+3);
            glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, C_LIGHTS+5*i,
                ((color>>24)&0xFF)/255.0f, ((color>>16)&0xFF)/255.0f, ((color>>8)&0xFF)/255.0f, ((color)&0xFF)/255.0f);
            xfmemptr += 4;
            for(int j = 0; j < 4; ++j, xfmemptr += 3) {
				if( j == 1 && fabs(xfmemptr[0]) < 0.00001f && fabs(xfmemptr[1]) < 0.00001f && fabs(xfmemptr[2]) < 0.00001f) {
                    // dist atten, make sure not equal to 0!!!
                    glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, C_LIGHTS+5*i+j+1, 0.00001f, xfmemptr[1], xfmemptr[2], 0);
                }
                else
                    glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_LIGHTS+5*i+j+1, xfmemptr);
            }
        }

        nLightsChanged[0] = nLightsChanged[1] = -1;
    }

    if (nMaterialsChanged) {
        for(int i = 0; i < 4; ++i) {
            if( nMaterialsChanged&(1<<i) )
                glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_MATERIALS+i, &s_fMaterials[4*i]);
        }
        nMaterialsChanged = 0;
    }

    if (bPosNormalMatrixChanged) {
        bPosNormalMatrixChanged = false;

        float* pos = (float*)xfmem + MatrixIndexA.PosNormalMtxIdx * 4;
        float* norm = (float*)xfmem + XFMEM_NORMALMATRICES + 3 * (MatrixIndexA.PosNormalMtxIdx & 31);

        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_POSNORMALMATRIX, pos);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_POSNORMALMATRIX+1, pos+4);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_POSNORMALMATRIX+2, pos+8);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_POSNORMALMATRIX+3, norm);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_POSNORMALMATRIX+4, norm+3);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_POSNORMALMATRIX+5, norm+6);
    }

    if (bTexMatricesChanged[0]) {
        bTexMatricesChanged[0] = false;

        float* fptrs[] = {(float*)xfmem + MatrixIndexA.Tex0MtxIdx * 4, (float*)xfmem + MatrixIndexA.Tex1MtxIdx * 4,
            (float*)xfmem + MatrixIndexA.Tex2MtxIdx * 4, (float*)xfmem + MatrixIndexA.Tex3MtxIdx * 4 };
        
        for(int i = 0; i < 4; ++i) {
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_TEXMATRICES+3*i, fptrs[i]);
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_TEXMATRICES+3*i+1, fptrs[i]+4);
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_TEXMATRICES+3*i+2, fptrs[i]+8);
        }
    }

    if (bTexMatricesChanged[1]) {
        bTexMatricesChanged[1] = false;

        float* fptrs[] = {(float*)xfmem + MatrixIndexB.Tex4MtxIdx * 4, (float*)xfmem + MatrixIndexB.Tex5MtxIdx * 4,
            (float*)xfmem + MatrixIndexB.Tex6MtxIdx * 4, (float*)xfmem + MatrixIndexB.Tex7MtxIdx * 4 };
        
        for(int i = 0; i < 4; ++i) {
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_TEXMATRICES+3*i+12, fptrs[i]);
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_TEXMATRICES+3*i+12+1, fptrs[i]+4);
            glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_TEXMATRICES+3*i+12+2, fptrs[i]+8);
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
        INFO_LOG("view: topleft=(%f,%f), wh=(%f,%f), z=(%f,%f)\n",rawViewport[3]-rawViewport[0]-342,rawViewport[4]+rawViewport[1]-342,
            2 * rawViewport[0], 2 * rawViewport[1], (rawViewport[5]-rawViewport[2])/16777215.0f, rawViewport[5]/16777215.0f);
        glViewport((int)(rawViewport[3]-rawViewport[0]-342)<<g_AAx,Renderer::GetTargetHeight()-((int)(rawViewport[4]-rawViewport[1]-342)<<g_AAy), abs((int)(2 * rawViewport[0])), abs((int)(2 * rawViewport[1])));
        glDepthRange((rawViewport[5]-rawViewport[2])/16777215.0f, rawViewport[5]/16777215.0f);
    }

    if (bProjectionChanged) {
        bProjectionChanged = false;

        if (rawProjection[6] == 0) {
            g_fProjectionMatrix[0] = rawProjection[0];
            g_fProjectionMatrix[1] = 0.0f;
            g_fProjectionMatrix[2] = rawProjection[1];
            g_fProjectionMatrix[3] = 0;//-0.5f/Renderer::GetTargetWidth();	
                        
            g_fProjectionMatrix[4] = 0.0f;
            g_fProjectionMatrix[5] = rawProjection[2];
            g_fProjectionMatrix[6] = rawProjection[3];
            g_fProjectionMatrix[7] = 0;//+0.5f/Renderer::GetTargetHeight();
                        
            g_fProjectionMatrix[8] = 0.0f;
            g_fProjectionMatrix[9] = 0.0f;
            g_fProjectionMatrix[10] = -(1-rawProjection[4]);
            g_fProjectionMatrix[11] = rawProjection[5]; 
                        
            g_fProjectionMatrix[12] = 0.0f;
            g_fProjectionMatrix[13] = 0.0f;
            g_fProjectionMatrix[14] = -1.0f;
            g_fProjectionMatrix[15] = 0.0f;
        }
        else {
            g_fProjectionMatrix[0] = rawProjection[0];
            g_fProjectionMatrix[1] = 0.0f;
            g_fProjectionMatrix[2] = 0.0f;
            g_fProjectionMatrix[3] = rawProjection[1];

            g_fProjectionMatrix[4] = 0.0f;
            g_fProjectionMatrix[5] = rawProjection[2];
            g_fProjectionMatrix[6] = 0.0f;
            g_fProjectionMatrix[7] = rawProjection[3];

            g_fProjectionMatrix[8] = 0.0f;
            g_fProjectionMatrix[9] = 0.0f;
            g_fProjectionMatrix[10] = rawProjection[4];
            g_fProjectionMatrix[11] = -(-1 - rawProjection[5]);

            g_fProjectionMatrix[12] = 0;
            g_fProjectionMatrix[13] = 0;
            g_fProjectionMatrix[14] = 0.0f;
            g_fProjectionMatrix[15] = 1.0f;
        }

        PRIM_LOG("Projection: %f %f %f %f %f %f\n",rawProjection[0], rawProjection[1], rawProjection[2], rawProjection[3], rawProjection[4], rawProjection[5]);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_PROJECTION, &g_fProjectionMatrix[0]);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_PROJECTION+1, &g_fProjectionMatrix[4]);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_PROJECTION+2, &g_fProjectionMatrix[8]);
        glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, C_PROJECTION+3, &g_fProjectionMatrix[12]);
    }
}

void VertexShaderMngr::InvalidateXFRange(int start, int end)
{
    if( ((u32)start >= MatrixIndexA.PosNormalMtxIdx*4 && (u32)start < MatrixIndexA.PosNormalMtxIdx*4+12) ||
        ((u32)start >= XFMEM_NORMALMATRICES+(MatrixIndexA.PosNormalMtxIdx&31)*3 && (u32)start < XFMEM_NORMALMATRICES+(MatrixIndexA.PosNormalMtxIdx&31)*3+9) ) {
        bPosNormalMatrixChanged = true;
    }

    if (((u32)start >= MatrixIndexA.Tex0MtxIdx*4 && (u32)start < MatrixIndexA.Tex0MtxIdx*4+12) ||
        ((u32)start >= MatrixIndexA.Tex1MtxIdx*4 && (u32)start < MatrixIndexA.Tex1MtxIdx*4+12) ||
        ((u32)start >= MatrixIndexA.Tex2MtxIdx*4 && (u32)start < MatrixIndexA.Tex2MtxIdx*4+12) ||
        ((u32)start >= MatrixIndexA.Tex3MtxIdx*4 && (u32)start < MatrixIndexA.Tex3MtxIdx*4+12)) {
        bTexMatricesChanged[0] = true;
    }

    if (((u32)start >= MatrixIndexB.Tex4MtxIdx*4 && (u32)start < MatrixIndexB.Tex4MtxIdx*4+12) ||
        ((u32)start >= MatrixIndexB.Tex5MtxIdx*4 && (u32)start < MatrixIndexB.Tex5MtxIdx*4+12) ||
        ((u32)start >= MatrixIndexB.Tex6MtxIdx*4 && (u32)start < MatrixIndexB.Tex6MtxIdx*4+12) ||
        ((u32)start >= MatrixIndexB.Tex7MtxIdx*4 && (u32)start < MatrixIndexB.Tex7MtxIdx*4+12)) {
        bTexMatricesChanged[1] = true;
    }

    if (start < XFMEM_POSMATRICES_END ) {
        if (nTransformMatricesChanged[0] == -1) {
            nTransformMatricesChanged[0] = start;
            nTransformMatricesChanged[1] = end>XFMEM_POSMATRICES_END?XFMEM_POSMATRICES_END:end;
        }
        else {
            if (nTransformMatricesChanged[0] > start) nTransformMatricesChanged[0] = start;
            if (nTransformMatricesChanged[1] < end) nTransformMatricesChanged[1] = end>XFMEM_POSMATRICES_END?XFMEM_POSMATRICES_END:end;
        }
    }
    
    if (start < XFMEM_NORMALMATRICES_END && end > XFMEM_NORMALMATRICES ) {
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

    if (start < XFMEM_POSTMATRICES_END && end > XFMEM_POSTMATRICES ) {
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
            if (nLightsChanged[1] < _end) nLightsChanged[1] = _end;
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
    // check for paper mario
    for (size_t i = 0; i < ARRAYSIZE(rawViewport); ++i) {
        if( *(u32*)(_Viewport + i) == 0x7f800000 )
            return; // invalid number
    }

    memcpy(rawViewport, _Viewport, sizeof(rawViewport));
    bViewportChanged = true;
}

void VertexShaderMngr::SetViewportChanged()
{
    bViewportChanged = true;
}

void VertexShaderMngr::SetProjection(float* _pProjection, int constantIndex)
{
    memcpy(rawProjection, _pProjection, sizeof(rawProjection));
    bProjectionChanged = true;
}

size_t VertexShaderMngr::SaveLoadState(char *ptr, BOOL save)
{
    BEGINSAVELOAD;
    
    SAVELOAD(&xfregs,sizeof(xfregs));
    SAVELOAD(xfmem,XFMEM_SIZE*sizeof(u32));
    SAVELOAD(rawViewport,sizeof(rawViewport));
    SAVELOAD(rawProjection,sizeof(rawProjection));
    SAVELOAD(&MatrixIndexA,sizeof(TMatrixIndexA));
    SAVELOAD(&MatrixIndexB,sizeof(TMatrixIndexB));
    
    if (!save) {
        // invalidate all
        InvalidateXFRange(0,0x1000);
    }

    ENDSAVELOAD;
}

// LoadXFReg 0x10
void VertexShaderMngr::LoadXFReg(u32 transferSize, u32 baseAddress, u32 *pData)
{	

    u32 address = baseAddress;
    for (int i=0; i<(int)transferSize; i++)
    {
        address = baseAddress + i;

        // Setup a Matrix
        if (address < 0x1000)
        {
            VertexManager::Flush();
            InvalidateXFRange(address, address+transferSize);
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
                if (xfregs.nNumChans != (data&3) ) {
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
                if (xfregs.numTexGens != data) {
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
        else if (address>=0x4000)
        {
            // MessageBox(NULL, "1", "1", MB_OK);
            //4010 __GXSetGenMode
        }
    }
}

// Check docs for this sucker...
void VertexShaderMngr::LoadIndexedXF(u32 val, int array)
{
    int index = val>>16;
    int address = val&0xFFF; //check mask
    int size = ((val>>12)&0xF)+1;
    //load stuff from array to address in xf mem

    VertexManager::Flush();
    InvalidateXFRange(address, address+size);
    //PRIM_LOG("xfmem iwrite: 0x%x-0x%x\n", address, address+size);

    for (int i = 0; i < size; i++)
        xfmem[address + i] = Memory_Read_U32(arraybases[array] + arraystrides[array]*index + i*4);
}

float* VertexShaderMngr::GetPosNormalMat()
{
    return (float*)xfmem + MatrixIndexA.PosNormalMtxIdx * 4;
}

void VertexShaderMngr::GetVertexShaderId(VERTEXSHADERUID& id, u32 components)
{
    u32 zbufrender = (bpmem.ztex2.op==ZTEXTURE_ADD)||Renderer::GetZBufferTarget()!=0;
    id.values[0] = components|(xfregs.numTexGens<<23)|(xfregs.nNumChans<<27)|((u32)xfregs.bEnableDualTexTransform<<29)|(zbufrender<<30);

    for(int i = 0; i < 2; ++i) {
        id.values[1+i] = xfregs.colChans[i].color.enablelighting?(u32)xfregs.colChans[i].color.hex:(u32)xfregs.colChans[i].color.matsource;
        id.values[1+i] |= (xfregs.colChans[i].alpha.enablelighting?(u32)xfregs.colChans[i].alpha.hex:(u32)xfregs.colChans[i].alpha.matsource)<<15;
    }
    // fog
    id.values[1] |= (((u32)bpmem.fog.c_proj_fsel.fsel&3)<<30);
    id.values[2] |= (((u32)bpmem.fog.c_proj_fsel.fsel>>2)<<30);

    u32* pcurvalue = &id.values[3];

    for(int i = 0; i < xfregs.numTexGens; ++i) {
        TexMtxInfo tinfo = xfregs.texcoords[i].texmtxinfo;
        if( tinfo.texgentype != XF_TEXGEN_EMBOSS_MAP )
            tinfo.hex &= 0x7ff;
        if( tinfo.texgentype != XF_TEXGEN_REGULAR )
            tinfo.projection = 0;

        u32 val = ((tinfo.hex>>1)&0x1ffff);
        if( xfregs.bEnableDualTexTransform && tinfo.texgentype == XF_TEXGEN_REGULAR ) {
            // rewrite normalization and post index
            val |= ((u32)xfregs.texcoords[i].postmtxinfo.index<<17)|((u32)xfregs.texcoords[i].postmtxinfo.normalize<<23);
        }

        switch(i & 3) {
            case 0: pcurvalue[0] |= val; break;
            case 1: pcurvalue[0] |= val<<24; pcurvalue[1] = val>>8; ++pcurvalue; break;
            case 2: pcurvalue[0] |= val<<16; pcurvalue[1] = val>>16; ++pcurvalue; break;
            case 3: pcurvalue[0] |= val<<8; ++pcurvalue; break;
        }
    }
}

static char text[16384];
#define WRITE p+=sprintf

#define LIGHTS_POS ""

bool VertexShaderMngr::GenerateVertexShader(VERTEXSHADER& vs, u32 components)
{
    DVSTARTPROFILE();

    _assert_( bpmem.genMode.numtexgens == xfregs.numTexGens);
    _assert_( bpmem.genMode.numcolchans == xfregs.nNumChans);
    
    u32 lightMask = 0;
    if( xfregs.nNumChans > 0 )
        lightMask |= xfregs.colChans[0].color.GetFullLightMask() | xfregs.colChans[0].alpha.GetFullLightMask();
    if( xfregs.nNumChans > 1 )
        lightMask |= xfregs.colChans[1].color.GetFullLightMask() | xfregs.colChans[1].alpha.GetFullLightMask();

    bool bOutputZ = bpmem.ztex2.op==ZTEXTURE_ADD || Renderer::GetZBufferTarget()!=0;
    int ztexcoord = -1;

    char *p = text;
    WRITE(p,"//Vertex Shader: comp:%x, \n", components);
    WRITE(p,"typedef struct {\n"
        "  float4 T0, T1, T2;\n"
        "  float4 N0, N1, N2;\n"
        "} s_"I_POSNORMALMATRIX";\n\n"
        "typedef struct {\n"
        "  float4 t;\n"
        "} FLT4;\n"
        "typedef struct {\n"
        "  FLT4 T[24];\n"
        "} s_"I_TEXMATRICES";\n\n"
        "typedef struct {\n"
        "  FLT4 T[64];\n"
        "} s_"I_TRANSFORMMATRICES";\n\n"
        "typedef struct {\n"
        "  FLT4 T[32];\n"
        "} s_"I_NORMALMATRICES";\n\n"
        "typedef struct {\n"
        "  FLT4 T[64];\n"
        "} s_"I_POSTTRANSFORMMATRICES";\n\n"
        "typedef struct {\n"
        "  float4 col;\n"
        "  float4 cosatt;\n"
        "  float4 distatt;\n"
        "  float4 pos;\n"
        "  float4 dir;\n"
        "} Light;\n\n"
        "typedef struct {\n"
        "  Light lights[8];\n"
        "} s_"I_LIGHTS";\n\n"
        "typedef struct {\n"
        "  float4 C0, C1, C2, C3;\n"
        "} s_"I_MATERIALS";\n\n"
        "typedef struct {\n"
        "  float4 T0,T1,T2,T3;\n"
        "} s_"I_PROJECTION";\n"
        "typedef struct {\n"
        "  float4 params;\n" // a, b, c, b_shift
        "} s_"I_FOGPARAMS";\n\n");

    WRITE(p,"struct VS_OUTPUT {\n");
    WRITE(p,"  float4 pos : POSITION;\n");
    WRITE(p,"  float4 colors[2] : COLOR0;\n");

    // if outputting Z, embed the Z coordinate in the w component of a texture coordinate
    // if number of tex gens occupies all the texture coordinates, use the last tex coord
    // otherwise use the next available tex coord
    for(int i = 0; i < xfregs.numTexGens; ++i) {
        WRITE(p,"  float%d tex%d : TEXCOORD%d;\n", (i==(xfregs.numTexGens-1)&&bOutputZ)?4:3, i, i);
    }
    if( bOutputZ && xfregs.numTexGens == 0 ) {
        ztexcoord = 0;
        WRITE(p,"  float4 tex%d : TEXCOORD%d;\n", ztexcoord, ztexcoord);
    }
    else if( bOutputZ )
        ztexcoord = xfregs.numTexGens-1;

    WRITE(p,"};\n");
    WRITE(p,"\n");

    // uniforms
    
    // bool bTexMtx = ((components & VertexLoader::VB_HAS_TEXMTXIDXALL)<<VertexLoader::VB_HAS_UVTEXMTXSHIFT)!=0; unused TODO: keep?

    WRITE(p, "uniform s_"I_TRANSFORMMATRICES" "I_TRANSFORMMATRICES" : register(c%d);\n", C_TRANSFORMMATRICES);
    WRITE(p, "uniform s_"I_TEXMATRICES" "I_TEXMATRICES" : register(c%d);\n", C_TEXMATRICES); // also using tex matrices
    WRITE(p, "uniform s_"I_NORMALMATRICES" "I_NORMALMATRICES" : register(c%d);\n", C_NORMALMATRICES);
    WRITE(p, "uniform s_"I_POSNORMALMATRIX" "I_POSNORMALMATRIX" : register(c%d);\n", C_POSNORMALMATRIX);
    WRITE(p, "uniform s_"I_POSTTRANSFORMMATRICES" "I_POSTTRANSFORMMATRICES" : register(c%d);\n", C_POSTTRANSFORMMATRICES);
    WRITE(p, "uniform s_"I_LIGHTS" "I_LIGHTS" : register(c%d);\n", C_LIGHTS);
    WRITE(p, "uniform s_"I_MATERIALS" "I_MATERIALS" : register(c%d);\n", C_MATERIALS);
    WRITE(p, "uniform s_"I_PROJECTION" "I_PROJECTION" : register(c%d);\n", C_PROJECTION);
    WRITE(p, "uniform s_"I_FOGPARAMS" "I_FOGPARAMS" : register(c%d);\n", C_FOGPARAMS);

    WRITE(p,"VS_OUTPUT main(\n");
    
    // inputs
    if (components & VertexLoader::VB_HAS_NRM0)
        WRITE(p,"  float3 rawnorm0 : NORMAL,\n");
    if (components & VertexLoader::VB_HAS_NRM1)
        WRITE(p,"  float3 rawnorm1 : ATTR%d,\n", SHADER_NORM1_ATTRIB);
    if (components & VertexLoader::VB_HAS_NRM2)
        WRITE(p,"  float3 rawnorm2 : ATTR%d,\n", SHADER_NORM2_ATTRIB);
    if (components & VertexLoader::VB_HAS_COL0)
        WRITE(p,"  float4 color0 : COLOR0,\n");
    if (components & VertexLoader::VB_HAS_COL1)
        WRITE(p,"  float4 color1 : COLOR1,\n");
    for (int i = 0; i < 8; ++i) {
        u32 hastexmtx = (components & (VertexLoader::VB_HAS_TEXMTXIDX0<<i));
        if ( (components & (VertexLoader::VB_HAS_UV0<<i)) || hastexmtx )
            WRITE(p,"  float%d tex%d : TEXCOORD%d,\n", hastexmtx ? 3 : 2, i,i);
    }
    if (components & VertexLoader::VB_HAS_POSMTXIDX)
        WRITE(p, "  half posmtx : ATTR%d,\n", SHADER_POSMTX_ATTRIB);

    WRITE(p,"  float4 rawpos : POSITION) {\n");
    WRITE(p, "VS_OUTPUT o;\n");

    // transforms
    if ( components & VertexLoader::VB_HAS_POSMTXIDX) {
        WRITE(p, "float4 pos = float4(dot("I_TRANSFORMMATRICES".T[posmtx].t, rawpos), dot("I_TRANSFORMMATRICES".T[posmtx+1].t, rawpos), dot("I_TRANSFORMMATRICES".T[posmtx+2].t, rawpos),1);\n");
        
        if (components & VertexLoader::VB_HAS_NRMALL) {
            WRITE(p, "int normidx = posmtx >= 32 ? (posmtx-32) : posmtx;\n");
            WRITE(p,"float3 N0 = "I_NORMALMATRICES".T[normidx].t.xyz, N1 = "I_NORMALMATRICES".T[normidx+1].t.xyz, N2 = "I_NORMALMATRICES".T[normidx+2].t.xyz;\n");
        }

        if (components & VertexLoader::VB_HAS_NRM0)
            WRITE(p,"half3 _norm0 = half3(dot(N0, rawnorm0), dot(N1, rawnorm0), dot(N2, rawnorm0));\n"
                    "half3 norm0 = normalize(_norm0);\n");
        if (components & VertexLoader::VB_HAS_NRM1)
            WRITE(p,"half3 _norm1 = half3(dot(N0, rawnorm1), dot(N1, rawnorm1), dot(N2, rawnorm1));\n");
                    //"half3 norm1 = normalize(_norm1);\n");
        if (components & VertexLoader::VB_HAS_NRM2)
            WRITE(p,"half3 _norm2 = half3(dot(N0, rawnorm2), dot(N1, rawnorm2), dot(N2, rawnorm2));\n");
                    //"half3 norm2 = normalize(_norm2);\n");
    }
    else {
        WRITE(p, "float4 pos = float4(dot("I_POSNORMALMATRIX".T0, rawpos), dot("I_POSNORMALMATRIX".T1, rawpos), dot("I_POSNORMALMATRIX".T2, rawpos), 1);\n");
        if (components & VertexLoader::VB_HAS_NRM0)
            WRITE(p,"half3 _norm0 = half3(dot("I_POSNORMALMATRIX".N0.xyz, rawnorm0), dot("I_POSNORMALMATRIX".N1.xyz, rawnorm0), dot("I_POSNORMALMATRIX".N2.xyz, rawnorm0));\n"
                    "half3 norm0 = normalize(_norm0);\n");
        if (components & VertexLoader::VB_HAS_NRM1)
            WRITE(p,"half3 _norm1 = half3(dot("I_POSNORMALMATRIX".N0.xyz, rawnorm1), dot("I_POSNORMALMATRIX".N1.xyz, rawnorm1), dot("I_POSNORMALMATRIX".N2.xyz, rawnorm1));\n");
                    //"half3 norm1 = normalize(_norm1);\n");
        if (components & VertexLoader::VB_HAS_NRM2)
            WRITE(p,"half3 _norm2 = half3(dot("I_POSNORMALMATRIX".N0.xyz, rawnorm2), dot("I_POSNORMALMATRIX".N1.xyz, rawnorm2), dot("I_POSNORMALMATRIX".N2.xyz, rawnorm2));\n");
                    //"half3 norm2 = normalize(_norm2);\n");
    }

    if (!(components & VertexLoader::VB_HAS_NRM0))
        WRITE(p,"half3 _norm0 = half3(0,0,0), norm0= half3(0,0,0);\n");

    WRITE(p,"o.pos = float4(dot("I_PROJECTION".T0, pos), dot("I_PROJECTION".T1, pos), dot("I_PROJECTION".T2, pos), dot("I_PROJECTION".T3, pos));\n");

    WRITE(p, "half4 mat, lacc;\n"
    "half3 ldir, h;\n"
    "half dist, dist2, attn;\n");

    // lights/colors
    for (int j=0; j<xfregs.nNumChans; j++) {

        // bool bColorAlphaSame = xfregs.colChans[j].color.hex == xfregs.colChans[j].alpha.hex;  unused
        const LitChannel& color = xfregs.colChans[j].color;
        const LitChannel& alpha = xfregs.colChans[j].alpha;

        WRITE(p,"{\n");

        if (color.matsource) {// from vertex
            if (components & (VertexLoader::VB_HAS_COL0<<j) )
                WRITE(p,"mat = color%d;\n", j);
            else WRITE(p,"mat = half4(1,1,1,1);\n");
        }
        else // from color
            WRITE(p,"mat = "I_MATERIALS".C%d;\n", j+2);

        if( color.enablelighting ) {
            if (color.ambsource) {// from vertex
                if (components & (VertexLoader::VB_HAS_COL0<<j) )
                    WRITE(p,"lacc = color%d;\n", j);
                else WRITE(p,"lacc = half4(0.0f,0.0f,0.0f,0.0f);\n");
            }
            else // from color
                WRITE(p,"lacc = "I_MATERIALS".C%d;\n", j);
        }

        // check if alpha is different
        if (alpha.matsource != color.matsource) {
            if (alpha.matsource ) {// from vertex
                if (components & (VertexLoader::VB_HAS_COL0<<j) )
                    WRITE(p,"mat.w = color%d.w;\n", j);
                else WRITE(p,"mat.w = 1;\n");
            }
            else // from color
                WRITE(p,"mat.w = "I_MATERIALS".C%d.w;\n", j+2);
        }

        if (alpha.enablelighting && alpha.ambsource != color.ambsource) {
            if (alpha.ambsource) {// from vertex
                if (components & (VertexLoader::VB_HAS_COL0<<j) )
                    WRITE(p,"lacc.w = color%d.w;\n", j);
                else WRITE(p,"lacc.w = 0;\n");
            }
            else // from color
                WRITE(p,"lacc.w = "I_MATERIALS".C%d.w;\n", j);
        }
        
        if( color.enablelighting && alpha.enablelighting && (color.GetFullLightMask() != alpha.GetFullLightMask() || color.lightparams != alpha.lightparams) ) {

            // both have lighting, except not using the same lights
            int mask = 0; // holds already computed lights

            if( color.lightparams == alpha.lightparams && (color.GetFullLightMask() & alpha.GetFullLightMask()) ) {
                // if lights are shared, compute those first
                mask = color.GetFullLightMask() & alpha.GetFullLightMask();
                for(int i = 0; i < 8; ++i) {
                    if( mask&(1<<i))
                        p = GenerateLightShader(p, i, color, "lacc", 3);
                }
            }

            // no shared lights
            for(int i = 0; i < 8; ++i) {
                if( !(mask&(1<<i)) && (color.GetFullLightMask() & (1<<i)) )
                    p = GenerateLightShader(p, i, color, "lacc", 1);
                if( !(mask&(1<<i)) && (alpha.GetFullLightMask() & (1<<i)) )
                    p = GenerateLightShader(p, i, alpha, "lacc", 2);
            }
        }
        else if( color.enablelighting || alpha.enablelighting) {
            // either one is enabled
            int coloralpha = (int)color.enablelighting|((int)alpha.enablelighting<<1);
            for(int i = 0; i < 8; ++i) {
                if( color.GetFullLightMask() & (1<<i) )
                    p = GenerateLightShader(p, i, color.enablelighting?color:alpha, "lacc", coloralpha);
            }
        }

        if (color.enablelighting != alpha.enablelighting) {
            if( color.enablelighting )
                WRITE(p, "o.colors[%d].xyz = mat.xyz * clamp(lacc.xyz,float3(0.0f,0.0f,0.0f),float3(1.0f,1.0f,1.0f));\n"
                    "o.colors[%d].w = mat.w;\n", j, j);
            else
                WRITE(p, "o.colors[%d].xyz = mat.xyz;\n"
                    "o.colors[%d].w = mat.w  * clamp(lacc.w,0.0f,1.0f);\n", j, j);
        }
        else {
            if( alpha.enablelighting )
                WRITE(p, "o.colors[%d] = mat * clamp(lacc,float4(0.0f,0.0f,0.0f,0.0f), float4(1.0f,1.0f,1.0f,1.0f));\n", j);
            else WRITE(p, "o.colors[%d] = mat;\n", j);
        }
        WRITE(p, "}\n");
    }

    // zero left over channels
    for(int i = xfregs.nNumChans; i < 2; ++i) WRITE(p, "o.colors[%d] = 0;\n", i);

    // transform texcoords
    for(int i = 0; i < xfregs.numTexGens; ++i) {

        TexMtxInfo& texinfo = xfregs.texcoords[i].texmtxinfo;

        WRITE(p, "{\n");
        switch(texinfo.sourcerow) {
        case XF_SRCGEOM_INROW:
            _assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
            WRITE(p, "float4 coord = rawpos;\n"); // pos.w is 1
            break;
        case XF_SRCNORMAL_INROW:
            if (components & VertexLoader::VB_HAS_NRM0) {
                _assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
                WRITE(p, "float4 coord = float4(rawnorm0.xyz, 1.0);\n");
            }
            else WRITE(p, "float4 coord = 0;\n");
            break;
        case XF_SRCCOLORS_INROW:
            _assert_( texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC0 || texinfo.texgentype == XF_TEXGEN_COLOR_STRGBC1 );
            break;
        case XF_SRCBINORMAL_T_INROW:
            if (components & VertexLoader::VB_HAS_NRM1) {
                _assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
                WRITE(p, "float4 coord = float4(rawnorm1.xyz, 1.0);\n");
            }
            else WRITE(p, "float4 coord = 0;\n");
            break;
        case XF_SRCBINORMAL_B_INROW:
            if (components & VertexLoader::VB_HAS_NRM2) {
                _assert_( texinfo.inputform == XF_TEXINPUT_ABC1 );
                WRITE(p, "float4 coord = float4(rawnorm2.xyz, 1.0);\n");
            }
            else WRITE(p, "float4 coord = 0;\n");
            break;
        default:
            _assert_(texinfo.sourcerow <= XF_SRCTEX7_INROW);
            if ( components & (VertexLoader::VB_HAS_UV0<<(texinfo.sourcerow - XF_SRCTEX0_INROW)) )
                WRITE(p, "float4 coord = float4(tex%d.x, tex%d.y, 1.0f, 1.0f);\n", texinfo.sourcerow - XF_SRCTEX0_INROW, texinfo.sourcerow - XF_SRCTEX0_INROW);
            else
                WRITE(p, "float4 coord = float4(0.0f, 0.0f, 1.0f, 1.0f);\n");
            break;
        }

        // firs transformation
        switch(texinfo.texgentype) {
            case XF_TEXGEN_REGULAR:
                if( components & (VertexLoader::VB_HAS_TEXMTXIDX0<<i) ) {
                    if (texinfo.projection == XF_TEXPROJ_STQ )
                        WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z].t), dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z+1].t), dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z+2].t));\n", i, i, i, i);
                    else {
                        WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z].t), dot(coord, "I_TRANSFORMMATRICES".T[tex%d.z+1].t), 1);\n", i, i, i);
                    }
                }
                else {
                    if (texinfo.projection == XF_TEXPROJ_STQ )
                        WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TEXMATRICES".T[%d].t), dot(coord, "I_TEXMATRICES".T[%d].t), dot(coord, "I_TEXMATRICES".T[%d].t));\n", i, 3*i, 3*i+1, 3*i+2);
                    else
                        WRITE(p, "o.tex%d.xyz = float3(dot(coord, "I_TEXMATRICES".T[%d].t), dot(coord, "I_TEXMATRICES".T[%d].t), 1);\n", i, 3*i, 3*i+1);
                }
                break;
            case XF_TEXGEN_EMBOSS_MAP: // calculate tex coords into bump map

                if( components & (VertexLoader::VB_HAS_NRM1|VertexLoader::VB_HAS_NRM2) ) {
                    // transform the light dir into tangent space
                    WRITE(p, "ldir = normalize("I_LIGHTS".lights[%d].pos.xyz - pos.xyz);\n", texinfo.embosslightshift); 
                    WRITE(p, "o.tex%d.xyz = o.tex%d.xyz + float3(dot(ldir, _norm1), dot(ldir, _norm2), 0.0f);\n", i, texinfo.embosssourceshift);
                }
                else _assert_(0); // should have normals

                break;
            case XF_TEXGEN_COLOR_STRGBC0:
                _assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
                WRITE(p, "o.tex%d.xyz = float3(o.colors[0].x, o.colors[0].y, 1);\n", i);
                break;
            case XF_TEXGEN_COLOR_STRGBC1:
                _assert_(texinfo.sourcerow == XF_SRCCOLORS_INROW);
                WRITE(p, "o.tex%d.xyz = float3(o.colors[1].x, o.colors[1].y, 1);\n", i);
                break;
        }

        if(xfregs.bEnableDualTexTransform && texinfo.texgentype == XF_TEXGEN_REGULAR ) { // only works for regular tex gen types?
            if (xfregs.texcoords[i].postmtxinfo.normalize)
                WRITE(p, "o.tex%d.xyz = normalize(o.tex%d.xyz);\n", i, i);

            //multiply by postmatrix
            int postidx = xfregs.texcoords[i].postmtxinfo.index;
            WRITE(p, "float4 P0 = "I_POSTTRANSFORMMATRICES".T[%d].t;\n"
                "float4 P1 = "I_POSTTRANSFORMMATRICES".T[%d].t;\n"
                "float4 P2 = "I_POSTTRANSFORMMATRICES".T[%d].t;\n",
                postidx&0x3f, (postidx+1)&0x3f, (postidx+2)&0x3f);
            WRITE(p, "o.tex%d.xyz = float3(dot(P0.xyz, o.tex%d.xyz) + P0.w, dot(P1.xyz, o.tex%d.xyz) + P1.w, dot(P2.xyz, o.tex%d.xyz) + P2.w);\n", i, i, i, i);
        }

        WRITE(p, "}\n");
    }

    if( ztexcoord >= 0 )
        WRITE(p, "o.tex%d.w = o.pos.z/o.pos.w;\n", ztexcoord);

//    if( bpmem.fog.c_proj_fsel.fsel != 0 ) {
//        switch(bpmem.fog.c_proj_fsel.fsel) {
//            case 1: // linear
//                break;
//            case 4: // exp
//                break;
//            case 5: // exp2
//                break;
//            case 6: // backward exp
//                break;
//            case 7: // backward exp2
//                break;
//        }
//
//        WRITE(p, "o.fog = o.pos.z/o.pos.w;\n");
//    }

    WRITE(p,"return o;\n}\n\0");

    return VertexShaderMngr::CompileVertexShader(vs, text);
}

// coloralpha - 1 if color, 2 if alpha
char* VertexShaderMngr::GenerateLightShader(char* p, int index, const LitChannel& chan, const char* dest, int coloralpha)
{
    const char* swizzle = "xyzw";
    if( coloralpha == 1 ) swizzle = "xyz";
    else if( coloralpha == 2 ) swizzle = "w";

    if( !(chan.attnfunc&1) ) {
        // atten disabled
        switch(chan.diffusefunc) {
            case LIGHTDIF_NONE:
                WRITE(p, "%s.%s += "I_LIGHTS".lights[%d].col.%s;\n", dest, swizzle, index, swizzle);
                break;
            case LIGHTDIF_SIGN:
            case LIGHTDIF_CLAMP:
                WRITE(p, "ldir = normalize("I_LIGHTS".lights[%d].pos.xyz - pos.xyz);\n", index);
                WRITE(p, "%s.%s += %sdot(ldir, norm0)) * "I_LIGHTS".lights[%d].col.%s;\n",
                    dest, swizzle, chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0f," :"(", index, swizzle);
                break;
            default: _assert_(0);
        }
    }
    else { // spec and spot
        WRITE(p, "ldir = "I_LIGHTS".lights[%d].pos.xyz - pos.xyz;\n", index);

        if( chan.attnfunc == 3 ) { // spot
            WRITE(p, "dist2 = dot(ldir, ldir);\n"
                "dist = sqrt(dist2);\n"
                "ldir = ldir / dist;\n"
                "attn = max(0.0f, dot(ldir, "I_LIGHTS".lights[%d].dir.xyz));\n",index);
            WRITE(p, "attn = max(0.0f, dot("I_LIGHTS".lights[%d].cosatt.xyz, half3(1, attn, attn*attn))) / dot("I_LIGHTS".lights[%d].distatt.xyz, half3(1,dist,dist2));\n", index, index);
        }
        else if( chan.attnfunc == 1) { // specular
            WRITE(p, "attn = dot(norm0, "I_LIGHTS".lights[%d].pos.xyz) > 0 ? max(0.0f, dot(norm0, "I_LIGHTS".lights[%d].dir.xyz)) : 0;\n", index, index);
            WRITE(p, "ldir = half3(1,attn,attn*attn);\n");
            WRITE(p, "attn = max(0.0f, dot("I_LIGHTS".lights[%d].cosatt.xyz, ldir)) / dot("I_LIGHTS".lights[%d].distatt.xyz, ldir);\n", index, index);
        }

        switch(chan.diffusefunc) {
            case LIGHTDIF_NONE:
                WRITE(p, "%s.%s += attn * "I_LIGHTS".lights[%d].col.%s;\n", dest, swizzle, index, swizzle);
                break;
            case LIGHTDIF_SIGN:
            case LIGHTDIF_CLAMP:
                WRITE(p, "%s.%s += attn * %sdot(ldir, norm0)) * "I_LIGHTS".lights[%d].col.%s;\n",
                    dest, swizzle, chan.diffusefunc != LIGHTDIF_SIGN ? "max(0.0f," :"(", index, swizzle);
                break;
            default: _assert_(0);
        }
    }
    WRITE(p, "\n");

    return p;
}
