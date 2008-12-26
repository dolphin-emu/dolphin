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

#include "GLUtil.h"

#include <Cg/cg.h>
#include <Cg/cgGL.h>

#include <cmath>

#include "Statistics.h"
#include "Config.h"
#include "ImageWrite.h"
#include "Common.h"
#include "Render.h"
#include "VertexShader.h"
#include "PixelShaderManager.h"

PixelShaderCache::PSCache PixelShaderCache::pshaders;
PIXELSHADERUID PixelShaderCache::s_curuid;

static FRAGMENTSHADER* pShaderLast = NULL;
static int s_nMaxPixelInstructions;
static int s_nColorsChanged[2]; // 0 - regular colors, 1 - k colors
static int s_nIndTexMtxChanged = 0;
static bool s_bAlphaChanged;
static bool s_bZBiasChanged;
static bool s_bIndTexScaleChanged;
static float lastRGBAfull[2][4][4];
static u8 s_nTexDimsChanged;
static u32 lastAlpha = 0;
static u32 lastTexDims[8]={0};
static u32 lastZBias = 0;

// lower byte describes if a texture is nonpow2 or pow2
// next byte describes whether the repeat wrap mode is enabled for the s channel
// next byte is for t channel
static u32 s_texturemask = 0;

static int maptocoord[8]; // indexed by texture map, holds the texcoord associated with the map
static u32 maptocoord_mask = 0;
    
static GLuint s_ColorMatrixProgram = 0;

void SetPSConstant4f(int const_number, float f1, float f2, float f3, float f4) {
    glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, const_number, f1, f2, f3, f4);
}

void SetPSConstant4fv(int const_number, const float *f) {
	glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, const_number, f);
}

void PixelShaderCache::Init()
{
    GL_REPORT_ERRORD();

    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB, (GLint *)&s_nMaxPixelInstructions);
    
    int maxinst, maxattribs;
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, (GLint *)&maxinst);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB, (GLint *)&maxattribs);
    ERROR_LOG("pixel max_alu=%d, max_inst=%d, max_attrib=%d\n", s_nMaxPixelInstructions, maxinst, maxattribs);
    
    char pmatrixprog[1024];
    sprintf(pmatrixprog, "!!ARBfp1.0"
                        "TEMP R0;\n"
                        "TEMP R1;\n"
                        "TEX R0, fragment.texcoord[0], texture[0], RECT;\n"
                        "DP4 R1.w, R0, program.env[%d];\n"
                        "DP4 R1.z, R0, program.env[%d];\n"
                        "DP4 R1.x, R0, program.env[%d];\n"
                        "DP4 R1.y, R0, program.env[%d];\n"
                        "ADD result.color, R1, program.env[%d];\n"
                        "END\n", C_COLORMATRIX+3, C_COLORMATRIX+2, C_COLORMATRIX, C_COLORMATRIX+1, C_COLORMATRIX+4);
    glGenProgramsARB(1, &s_ColorMatrixProgram);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, s_ColorMatrixProgram);
    glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pmatrixprog), pmatrixprog);

    GLenum err = GL_NO_ERROR;
    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR) {
        ERROR_LOG("Failed to create color matrix fragment program\n");
        glDeleteProgramsARB(1, &s_ColorMatrixProgram);
		s_ColorMatrixProgram = 0;
    }
}

void PixelShaderCache::Shutdown()
{
    glDeleteProgramsARB(1, &s_ColorMatrixProgram);
	s_ColorMatrixProgram = 0;
    PSCache::iterator iter = pshaders.begin();
    for (; iter != pshaders.end(); iter++)
        iter->second.Destroy();
    pshaders.clear();
}

FRAGMENTSHADER* PixelShaderCache::GetShader()
{
    DVSTARTPROFILE();
    PIXELSHADERUID uid;
    u32 zbufrender = (Renderer::GetZBufferTarget() && bpmem.zmode.updateenable) ? 1 : 0;
    u32 zBufRenderToCol0 = Renderer::GetRenderMode() != Renderer::RM_Normal;
    GetPixelShaderId(uid, s_texturemask, zbufrender, zBufRenderToCol0);

    PSCache::iterator iter = pshaders.find(uid);

    if (iter != pshaders.end()) {
        iter->second.frameCount = frameCount;
        PSCacheEntry &entry = iter->second;
        if (&entry.shader != pShaderLast)
        {
            pShaderLast = &entry.shader;
        }
        return pShaderLast;
    }

    PSCacheEntry& newentry = pshaders[uid];
    char *code = GeneratePixelShader(s_texturemask,
		                             Renderer::GetZBufferTarget() != 0,
									 Renderer::GetRenderMode() != Renderer::RM_Normal);

#if defined(_DEBUG) || defined(DEBUGFAST)
    if (g_Config.iLog & CONF_SAVESHADERS && code) {	
        static int counter = 0;
        char szTemp[MAX_PATH];
		sprintf(szTemp, "%s/ps_%04i.txt", g_Config.texDumpPath, counter++);
        
        SaveData(szTemp, code);
    }
#endif

	//	printf("Compiling pixel shader. size = %i\n", strlen(code));
    if (!code || !CompilePixelShader(newentry.shader, code)) {
        ERROR_LOG("failed to create pixel shader\n");
        return NULL;
    }

    //Make an entry in the table
    newentry.frameCount = frameCount;
    
    pShaderLast = &newentry.shader;
    INCSTAT(stats.numPixelShadersCreated);
    SETSTAT(stats.numPixelShadersAlive, pshaders.size());
    return pShaderLast;
}

void PixelShaderCache::Cleanup()
{
    PSCache::iterator iter = pshaders.begin();
    while (iter != pshaders.end()) {
        PSCacheEntry &entry = iter->second;
        if (entry.frameCount < frameCount - 400) {
            entry.Destroy();
#ifdef _WIN32
            iter = pshaders.erase(iter);
#else
			pshaders.erase(iter++);  // (this is gcc standard!)
#endif
        }
        else
            iter++;
    }
    SETSTAT(stats.numPixelShadersAlive,(int)pshaders.size());
}

bool PixelShaderCache::CompilePixelShader(FRAGMENTSHADER& ps, const char* pstrprogram)
{
    char stropt[64];
    sprintf(stropt, "MaxLocalParams=32,NumInstructionSlots=%d", s_nMaxPixelInstructions);
    const char* opts[] = {"-profileopts", stropt, "-O2", "-q", NULL};
    CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgfProf, "main", opts);
    if (!cgIsProgram(tempprog) || cgGetError() != CG_NO_ERROR) {
        ERROR_LOG("Failed to create ps %s:\n", cgGetLastListing(g_cgcontext));
        ERROR_LOG(pstrprogram);
        return false;
    }

    char *pcompiledprog = (char*)cgGetProgramString(tempprog, CG_COMPILED_PROGRAM);
    char *plocal = strstr(pcompiledprog, "program.local");
    while (plocal != NULL) {
        const char* penv = "  program.env";
        memcpy(plocal, penv, 13);
        plocal = strstr(plocal+13, "program.local");
    }

    if (Renderer::IsUsingATIDrawBuffers()) {
        // sometimes compilation can use ARB_draw_buffers, which would fail for ATI cards
		// replace the three characters ARB with ATI. TODO - check whether this is fixed in modern ATI drivers.
        char* poptions = strstr(pcompiledprog, "ARB_draw_buffers");
        if (poptions != NULL) {
            poptions[0] = 'A';
            poptions[1] = 'T';
            poptions[2] = 'I';
        }
    }

    glGenProgramsARB( 1, &ps.glprogid );
    glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, ps.glprogid );
    glProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pcompiledprog), pcompiledprog);

    GLenum err = GL_NO_ERROR;
    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR) {
        ERROR_LOG(pstrprogram);
        ERROR_LOG(pcompiledprog);
    }

    cgDestroyProgram(tempprog);
	// printf("Compiled pixel shader %i\n", ps.glprogid);

#if defined(_DEBUG) || defined(DEBUGFAST) 
    ps.strprog = pstrprogram;
#endif
    return true;
}

void PixelShaderManager::Init()
{
    s_nColorsChanged[0] = s_nColorsChanged[1] = 0;
    s_nTexDimsChanged = 0;
    s_nIndTexMtxChanged = 15;
    s_bAlphaChanged = s_bZBiasChanged = s_bIndTexScaleChanged = true;
    for (int i = 0; i < 8; ++i)
		maptocoord[i] = -1;
	maptocoord_mask = 0;
    memset(lastRGBAfull, 0, sizeof(lastRGBAfull));
}

void PixelShaderManager::Shutdown()
{

}

void PixelShaderManager::SetConstants()
{
    for (int i = 0; i < 2; ++i) {
        if (s_nColorsChanged[i]) {
            int baseind = i ? C_KCOLORS : C_COLORS;
            for (int j = 0; j < 4; ++j) {
                if (s_nColorsChanged[i] & (1 << j)) {
                    SetPSConstant4fv(baseind+j, &lastRGBAfull[i][j][0]);
                }
            }
            s_nColorsChanged[i] = 0;
        }
    }

    u32 newmask = 0;
    for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages+1; ++i) {
        if (bpmem.tevorders[i/2].getEnable(i&1)) {
            int texmap = bpmem.tevorders[i/2].getTexMap(i&1);
            maptocoord[texmap] = bpmem.tevorders[i/2].getTexCoord(i&1);
            newmask |= 1 << texmap;
            SetTexDimsChanged(texmap);
        }
    }
    
    if (maptocoord_mask != newmask) {
        //u32 changes = maptocoord_mask ^ newmask;
        for (int i = 0; i < 8; ++i) {
            if (newmask & (1 << i)) {
                SetTexDimsChanged(i);
            }
			else {
                maptocoord[i] = -1;
            }
        }
        maptocoord_mask = newmask;
    }

    if (s_nTexDimsChanged) {
        for (int i = 0; i < 8; ++i) {
            if (s_nTexDimsChanged & (1<<i)) {
				SetPSTextureDims(i);
			}            
        }
        s_nTexDimsChanged = 0;
    }

    if (s_bAlphaChanged) {
        SetPSConstant4f(C_ALPHA, (lastAlpha&0xff)/255.0f, ((lastAlpha>>8)&0xff)/255.0f, 0, ((lastAlpha>>16)&0xff)/255.0f);
    }

    if (s_bZBiasChanged) {
        u32 bits;
        float ffrac = 255.0f/256.0f;
        float ftemp[4];
        switch (bpmem.ztex2.type) {
            case 0:
                bits = 8;
                ftemp[0] = ffrac/(256.0f*256.0f); ftemp[1] = ffrac/256.0f; ftemp[2] = ffrac; ftemp[3] = 0;
                break;
            case 1:
                bits = 16;
                ftemp[0] = 0; ftemp[1] = ffrac/(256.0f*256.0f); ftemp[2] = ffrac/256.0f; ftemp[3] = ffrac;
                break;
            case 2:
                bits = 24;
                ftemp[0] = ffrac/(256.0f*256.0f); ftemp[1] = ffrac/256.0f; ftemp[2] = ffrac; ftemp[3] = 0;
                break;
        }
        //ERROR_LOG("pixel=%x,%x, bias=%x\n", bpmem.zcontrol.pixel_format, bpmem.ztex2.type, lastZBias);
        SetPSConstant4fv(C_ZBIAS, ftemp);
        SetPSConstant4f(C_ZBIAS+1, 0, 0, 0, (float)( (((int)lastZBias<<8)>>8))/16777216.0f);
    }

    // indirect incoming texture scales, update all!
    if (s_bIndTexScaleChanged) {
		// set as two sets of vec4s, each containing S and T of two ind stages.
        float f[8];
        
        for (u32 i = 0; i < bpmem.genMode.numindstages; ++i) {
            int srctexmap = bpmem.tevindref.getTexMap(i);
            int texcoord = bpmem.tevindref.getTexCoord(i);
            TCoordInfo& tc = bpmem.texcoords[texcoord];
            
            f[2*i] = bpmem.texscale[i/2].getScaleS(i&1) *
				(float)(tc.s.scale_minus_1+1) / (float)(lastTexDims[srctexmap] & 0xffff);
            f[2*i+1] = bpmem.texscale[i/2].getScaleT(i&1) *
				(float)(tc.t.scale_minus_1+1) / (float)((lastTexDims[srctexmap] >> 16) & 0xfff);
			// Yes, the above should really be 0xfff. The top 4 bits are used for other stuff.
            PRIM_LOG("tex indscale%d: %f %f\n", i, f[2*i], f[2*i+1]);
        }

        SetPSConstant4fv(C_INDTEXSCALE, f);

        if (bpmem.genMode.numindstages > 2)
            SetPSConstant4fv(C_INDTEXSCALE+1, &f[4]);
       
        s_bIndTexScaleChanged = false;
    }

    if (s_nIndTexMtxChanged) {
        for (int i = 0; i < 3; ++i) {
            if (s_nIndTexMtxChanged & (1 << i)) {
                int scale = ((u32)bpmem.indmtx[i].col0.s0 << 0) |
					        ((u32)bpmem.indmtx[i].col1.s1 << 2) |
					        ((u32)bpmem.indmtx[i].col2.s2 << 4);
                float fscale = powf(2.0f, (float)(scale - 17)) / 1024.0f;

                // xyz - static matrix
                //TODO w - dynamic matrix scale / 256...... somehow / 4 works better
                SetPSConstant4f(C_INDTEXMTX+2*i,
                    bpmem.indmtx[i].col0.ma * fscale,
					bpmem.indmtx[i].col1.mc * fscale,
					bpmem.indmtx[i].col2.me * fscale,
					fscale * 256.0f);
                SetPSConstant4f(C_INDTEXMTX+2*i+1,
                    bpmem.indmtx[i].col0.mb * fscale,
					bpmem.indmtx[i].col1.md * fscale,
					bpmem.indmtx[i].col2.mf * fscale,
					fscale * 256.0f);

                PRIM_LOG("indmtx%d: scale=%f, mat=(%f %f %f; %f %f %f)\n", i,
                    1024.0f*fscale, bpmem.indmtx[i].col0.ma * fscale, bpmem.indmtx[i].col1.mc * fscale, bpmem.indmtx[i].col2.me * fscale,
                    bpmem.indmtx[i].col0.mb * fscale, bpmem.indmtx[i].col1.md * fscale, bpmem.indmtx[i].col2.mf * fscale, fscale);
            }
        }
        s_nIndTexMtxChanged = 0;
    }
}

void PixelShaderManager::SetPSTextureDims(int texid)
{
	float fdims[4];
	if (s_texturemask & (1<<texid)) {
		if (maptocoord[texid] >= 0) {
			TCoordInfo& tc = bpmem.texcoords[maptocoord[texid]];
			fdims[0] = (float)(lastTexDims[texid]&0xffff);
			fdims[1] = (float)((lastTexDims[texid]>>16)&0xfff);
			fdims[2] = (float)(tc.s.scale_minus_1+1)/(float)(lastTexDims[texid]&0xffff);
			fdims[3] = (float)(tc.t.scale_minus_1+1)/(float)((lastTexDims[texid]>>16)&0xfff);
		}
		else {
			fdims[0] = (float)(lastTexDims[texid]&0xffff);
			fdims[1] = (float)((lastTexDims[texid]>>16)&0xfff);
			fdims[2] = 1.0f;
			fdims[3] = 1.0f;
		}
	}
	else {
		if (maptocoord[texid] >= 0) {
			TCoordInfo& tc = bpmem.texcoords[maptocoord[texid]];
			fdims[0] = (float)(tc.s.scale_minus_1+1)/(float)(lastTexDims[texid]&0xffff);
			fdims[1] = (float)(tc.t.scale_minus_1+1)/(float)((lastTexDims[texid]>>16)&0xfff);
			fdims[2] = 1.0f/(float)(tc.s.scale_minus_1+1);
			fdims[3] = 1.0f/(float)(tc.t.scale_minus_1+1);
		}
		else {
			fdims[0] = 1.0f;
			fdims[1] = 1.0f;
			fdims[2] = 1.0f/(float)(lastTexDims[texid]&0xffff);
			fdims[3] = 1.0f/(float)((lastTexDims[texid]>>16)&0xfff);
		}
	}

	PRIM_LOG("texdims%d: %f %f %f %f\n", texid, fdims[0], fdims[1], fdims[2], fdims[3]);
	SetPSConstant4fv(C_TEXDIMS + texid, fdims);
}

void PixelShaderManager::SetColorChanged(int type, int num)
{
    int r = bpmem.tevregs[num].low.a;
	int a = bpmem.tevregs[num].low.b;
    int b = bpmem.tevregs[num].high.a;
	int g = bpmem.tevregs[num].high.b;
    float *pf = &lastRGBAfull[type][num][0];
    pf[0] = (float)r / 255.0f;
    pf[1] = (float)g / 255.0f;
    pf[2] = (float)b / 255.0f;
    pf[3] = (float)a / 255.0f;
    s_nColorsChanged[type] |= 1 << num;
    PRIM_LOG("pixel %scolor%d: %f %f %f %f\n", type?"k":"", num, pf[0], pf[1], pf[2], pf[3]);
}

void PixelShaderManager::SetAlpha(const AlphaFunc& alpha)
{
    if ((alpha.hex & 0xffff) != lastAlpha) {
        lastAlpha = (lastAlpha & ~0xffff) | (alpha.hex & 0xffff);
        s_bAlphaChanged = true;
    }
}

void PixelShaderManager::SetDestAlpha(const ConstantAlpha& alpha)
{
    if (alpha.alpha != (lastAlpha >> 16)) {
        lastAlpha = (lastAlpha & ~0xff0000) | ((alpha.hex & 0xff) << 16);
        s_bAlphaChanged = true;
    }
}

void PixelShaderManager::SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt)
{
    u32 wh = width | (height << 16) | (wraps << 28) | (wrapt << 30);
    if (lastTexDims[texmapid] != wh) {
        lastTexDims[texmapid] = wh;
		s_nTexDimsChanged |= 1 << texmapid;        
    }
}

void PixelShaderManager::SetZTextureBias(u32 bias)
{
    if (lastZBias != bias) {
        s_bZBiasChanged = true;
        lastZBias = bias;
    }
}

void PixelShaderManager::SetIndTexScaleChanged()
{
    s_bIndTexScaleChanged = true;
}

void PixelShaderManager::SetIndMatrixChanged(int matrixidx)
{
    s_nIndTexMtxChanged |= 1 << matrixidx;
}

void PixelShaderManager::SetGenModeChanged()
{
}

void PixelShaderManager::SetTevCombinerChanged(int id)
{
}

void PixelShaderManager::SetTevKSelChanged(int id)
{
}

void PixelShaderManager::SetTevOrderChanged(int id)
{
}

void PixelShaderManager::SetTevIndirectChanged(int id)
{
}

void PixelShaderManager::SetZTextureOpChanged()
{
    s_bZBiasChanged = true;
}

void PixelShaderManager::SetTexturesUsed(u32 nonpow2tex)
{
    if (s_texturemask != nonpow2tex) {
        for (int i = 0; i < 8; ++i) {
            if (nonpow2tex & (0x10101 << i)) {
				// this check was previously implicit, but should it be here?
				if (s_nTexDimsChanged )
					s_nTexDimsChanged |= 1 << i;				
            }
        }
        s_texturemask = nonpow2tex;
    }
}

void PixelShaderManager::SetTexDimsChanged(int texmapid)
{
    // this check was previously implicit, but should it be here?
	if (s_nTexDimsChanged)
		s_nTexDimsChanged |= 1 << texmapid;	

    SetIndTexScaleChanged();
}

void PixelShaderManager::SetColorMatrix(const float* pmatrix, const float* pfConstAdd)
{
    SetPSConstant4fv(C_COLORMATRIX, pmatrix);
    SetPSConstant4fv(C_COLORMATRIX+1, pmatrix+4);
    SetPSConstant4fv(C_COLORMATRIX+2, pmatrix+8);
    SetPSConstant4fv(C_COLORMATRIX+3, pmatrix+12);
    SetPSConstant4fv(C_COLORMATRIX+4, pfConstAdd);
}

GLuint PixelShaderManager::GetColorMatrixProgram()
{
    return s_ColorMatrixProgram;
}

