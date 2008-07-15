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
#include <assert.h>

#include "VertexShader.h"
#include "PixelShader.h"

#define I_COLORS "color"
#define I_KCOLORS "k"
#define I_ALPHA "alphaRef"
#define I_TEXDIMS "texdim"
#define I_ZBIAS "czbias"
#define I_INDTEXSCALE "cindscale"
#define I_INDTEXMTX "cindmtx"

#define C_COLORS 0
#define C_KCOLORS (C_COLORS+4)
#define C_ALPHA (C_KCOLORS+4)
#define C_TEXDIMS (C_ALPHA+1)
#define C_ZBIAS (C_TEXDIMS+8)
#define C_INDTEXSCALE (C_ZBIAS+2)
#define C_INDTEXMTX (C_INDTEXSCALE+2)
#define C_ENVCONST_END (C_INDTEXMTX+6)

#define C_COLORMATRIX (C_INDTEXMTX+6)


void WriteStage(char *&p, int n);
void WrapNonPow2Tex(char* &p, const char* var, int texmap);
void WriteAlphaCompare(char *&p, int num, int comp);
bool WriteAlphaTest(char *&p);

PixelShaderMngr::PSCache PixelShaderMngr::pshaders;
FRAGMENTSHADER* PixelShaderMngr::pShaderLast = NULL;
PixelShaderMngr::PIXELSHADERUID PixelShaderMngr::s_curuid;

static int s_nMaxPixelInstructions;
static int s_nColorsChanged[2]; // 0 - regular colors, 1 - k colors
static int s_nTexDimsChanged[2], s_nIndTexMtxChanged = 0; //min, max
static bool s_bAlphaChanged, s_bZBiasChanged, s_bIndTexScaleChanged;
static float lastRGBAfull[2][4][4] = {0};
static u32 lastAlpha = 0;
static u32 lastTexDims[8]={0};
static u32 lastZBias = 0;

// lower byte describes if a texture is nonpow2 or pow2
// next byte describes whether the repeat wrap mode is enabled for the s channel
// next byte is for t channel
static u32 s_texturemask = 0;

static int maptocoord[8]; // indexed by texture map, holds the texcoord associated with the map
static u32 maptocoord_mask=0;
    
static GLuint s_ColorMatrixProgram=0;

void PixelShaderMngr::Init()
{
    s_nColorsChanged[0] = s_nColorsChanged[1] = 0;
    s_nTexDimsChanged[0] = s_nTexDimsChanged[1] = -1;
    s_nIndTexMtxChanged = 15;
    s_bAlphaChanged = s_bZBiasChanged = s_bIndTexScaleChanged = true;
    GL_REPORT_ERRORD();
    for(int i = 0; i < 8; ++i) maptocoord[i] = -1;
    maptocoord_mask = 0;
    memset(lastRGBAfull, 0, sizeof(lastRGBAfull));

    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB, &s_nMaxPixelInstructions);
    
    int maxinst, maxattribs;
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB, &maxinst);
    glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB, &maxattribs);
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
    glGenProgramsARB( 1, &s_ColorMatrixProgram );
    glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, s_ColorMatrixProgram );
    
    glProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pmatrixprog), pmatrixprog);

    GLenum err=GL_NO_ERROR;
    GL_REPORT_ERROR();
    if( err != GL_NO_ERROR ) {
        ERROR_LOG("Failed to create color matrix fragment program\n");
        
        SAFE_RELEASE_PROG(s_ColorMatrixProgram);
    }
}

void PixelShaderMngr::Shutdown()
{
    SAFE_RELEASE_PROG(s_ColorMatrixProgram);
    PSCache::iterator iter = pshaders.begin();
    for (;iter!=pshaders.end();iter++)
        iter->second.Destroy();
    pshaders.clear();
}

FRAGMENTSHADER* PixelShaderMngr::GetShader()
{
    DVSTARTPROFILE();
    PIXELSHADERUID uid;
    GetPixelShaderId(uid);

    PSCache::iterator iter = pshaders.find(uid);

    if (iter != pshaders.end()) {
        iter->second.frameCount=frameCount;
        PSCacheEntry &entry = iter->second;
        if (&entry.shader != pShaderLast)
        {
            pShaderLast = &entry.shader;
        }
        return pShaderLast;
    }

    PSCacheEntry& newentry = pshaders[uid];
    
    if (!GeneratePixelShader(newentry.shader)) {
        ERROR_LOG("failed to create pixel shader\n");
        return NULL;
    }
    
    //Make an entry in the table
    newentry.frameCount=frameCount;
    
    pShaderLast = &newentry.shader;
    INCSTAT(stats.numPixelShadersCreated);
    SETSTAT(stats.numPixelShadersAlive, pshaders.size());
    return pShaderLast;
}

void PixelShaderMngr::Cleanup()
{
    PSCache::iterator iter = pshaders.begin();
    while(iter != pshaders.end()) {
        PSCacheEntry &entry = iter->second;
        if (entry.frameCount<frameCount-200) {
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

bool PixelShaderMngr::CompilePixelShader(FRAGMENTSHADER& ps, const char* pstrprogram)
{
    CGerror cgerr = cgGetError();

    char stropt[64];
    sprintf(stropt, "MaxLocalParams=32,NumInstructionSlots=%d", s_nMaxPixelInstructions);
#ifdef _WIN32
    const char* opts[] = {"-profileopts",stropt,"-O2","-q",NULL};
#else
    const char* opts[] = {"-profileopts",stropt,"-q",NULL};
#endif
    CGprogram tempprog = cgCreateProgram(g_cgcontext, CG_SOURCE, pstrprogram, g_cgfProf, "main", opts);
    if (!cgIsProgram(tempprog) || cgGetError() != CG_NO_ERROR) {
        ERROR_LOG("Failed to create ps %s:\n", cgGetLastListing(g_cgcontext));
        ERROR_LOG(pstrprogram);
        return false;
    }

    char* pcompiledprog = (char*)cgGetProgramString(tempprog, CG_COMPILED_PROGRAM);
    char* plocal = strstr(pcompiledprog, "program.local");
    while( plocal != NULL ) {
        const char* penv = "  program.env";
        memcpy(plocal, penv, 13);
        plocal = strstr(plocal+13, "program.local");
    }

    if( Renderer::IsUsingATIDrawBuffers() ) {
        // sometimes compilation can use ARB_draw_buffers, which would fail for ATI cards
        char* poptions = strstr(pcompiledprog, "ARB_draw_buffers");
        if( poptions != NULL ) {
            poptions[0] = 'A';
            poptions[1] = 'T';
            poptions[2] = 'I';
        }
    }

    //ERROR_LOG(pcompiledprog);
    //ERROR_LOG(pstrprogram);

    glGenProgramsARB( 1, &ps.glprogid );
    glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, ps.glprogid );
    glProgramStringARB( GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, (GLsizei)strlen(pcompiledprog), pcompiledprog);

    GLenum err = GL_NO_ERROR;
    GL_REPORT_ERROR();
    if( err != GL_NO_ERROR ) {
        ERROR_LOG(pstrprogram);
        ERROR_LOG(pcompiledprog);
    }

    cgDestroyProgram(tempprog);

#ifdef _DEBUG
    ps.strprog = pstrprogram;
#endif
    return true;
}

void PixelShaderMngr::SetConstants(FRAGMENTSHADER& ps)
{
    for(int i = 0; i < 2; ++i) {
        if( s_nColorsChanged[i] ) {
            
            int baseind = i?C_KCOLORS:C_COLORS;
            for(int j = 0; j < 4; ++j) {
                if( s_nColorsChanged[i] & (1<<j) ) {
                    glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, baseind+j, &lastRGBAfull[i][j][0]);
                }
            }
            s_nColorsChanged[i] = 0;
        }
    }

    u32 newmask = 0;
    for(u32 i = 0; i < bpmem.genMode.numtevstages+1; ++i) {
        if( bpmem.tevorders[i/2].getEnable(i&1) ) {
            int texmap = bpmem.tevorders[i/2].getTexMap(i&1);
            maptocoord[texmap] = bpmem.tevorders[i/2].getTexCoord(i&1);
            newmask |= 1<<texmap;
            SetTexDimsChanged(i);
        }
    }
    
    if( maptocoord_mask != newmask ) {
        u32 changes = maptocoord_mask ^ newmask;
        for(int i = 0; i < 8; ++i) {
            if( changes&(1<<i) ) {
                SetTexDimsChanged(i);
            }
            if( !(newmask & (1<<i)) ) {
                maptocoord[i] = -1;
            }
        }
        maptocoord_mask = newmask;
    }

    if( s_nTexDimsChanged[0] >= 0 ) {
        float fdims[4];
        for(int i = s_nTexDimsChanged[0]; i <= s_nTexDimsChanged[1]; ++i) {
            if( s_texturemask & (1<<i) ) {
                if( maptocoord[i] >= 0 ) {
                    TCoordInfo& tc = bpmem.texcoords[maptocoord[i]];
                    fdims[0] = (float)(lastTexDims[i]&0xffff);
                    fdims[1] = (float)((lastTexDims[i]>>16)&0xfff);
                    fdims[2] = (float)(tc.s.scale_minus_1+1)/(float)(lastTexDims[i]&0xffff);
                    fdims[3] = (float)(tc.t.scale_minus_1+1)/(float)((lastTexDims[i]>>16)&0xfff);
                }
                else {
                    fdims[0] = (float)(lastTexDims[i]&0xffff);
                    fdims[1] = (float)((lastTexDims[i]>>16)&0xfff);
                    fdims[2] = 1.0f;
                    fdims[3] = 1.0f;
                }
            }
            else {
                if( maptocoord[i] >= 0 ) {
                    TCoordInfo& tc = bpmem.texcoords[maptocoord[i]];
                    fdims[0] = (float)(tc.s.scale_minus_1+1)/(float)(lastTexDims[i]&0xffff);
                    fdims[1] = (float)(tc.t.scale_minus_1+1)/(float)((lastTexDims[i]>>16)&0xfff);
                    fdims[2] = 1.0f/(float)(tc.s.scale_minus_1+1);
                    fdims[3] = 1.0f/(float)(tc.t.scale_minus_1+1);
                }
                else {
                    fdims[0] = 1.0f;
                    fdims[1] = 1.0f;
                    fdims[2] = 1.0f/(float)(lastTexDims[i]&0xffff);
                    fdims[3] = 1.0f/(float)((lastTexDims[i]>>16)&0xfff);
                }
            }

            PRIM_LOG("texdims%d: %f %f %f %f\n", i, fdims[0], fdims[1], fdims[2], fdims[3]);
            glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, C_TEXDIMS+i, fdims);
        }
        s_nTexDimsChanged[0] = s_nTexDimsChanged[1] = -1;
    }

    if( s_bAlphaChanged ) {
        glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, C_ALPHA, (lastAlpha&0xff)/255.0f, ((lastAlpha>>8)&0xff)/255.0f, 0, ((lastAlpha>>16)&0xff)/255.0f);
    }

    if( s_bZBiasChanged ) {
        u32 bits;
        float ffrac = 255.0f/256.0f;
        float ftemp[4];
        switch(bpmem.ztex2.type) {
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
        glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, C_ZBIAS, ftemp);
        glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, C_ZBIAS+1, 0, 0, 0, (float)( (((int)lastZBias<<8)>>8))/16777216.0f);
    }

    // indirect incoming texture scales, update all!
    if( s_bIndTexScaleChanged ) {
        float f[8];
        
        for(u32 i = 0; i < bpmem.genMode.numindstages; ++i) {
            int srctexmap = bpmem.tevindref.getTexMap(i);
            int texcoord = bpmem.tevindref.getTexCoord(i);
            TCoordInfo& tc = bpmem.texcoords[texcoord];
            
            f[2*i] = bpmem.texscale[i/2].getScaleS(i&1) * (float)(tc.s.scale_minus_1+1) / (float)(lastTexDims[srctexmap]&0xffff);
            f[2*i+1] = bpmem.texscale[i/2].getScaleT(i&1) * (float)(tc.t.scale_minus_1+1) / (float)((lastTexDims[srctexmap]>>16)&0xfff);

            PRIM_LOG("tex indscale%d: %f %f\n", i, f[2*i], f[2*i+1]);
        }

        glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, C_INDTEXSCALE, f);

        if( bpmem.genMode.numindstages > 2 )
            glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, C_INDTEXSCALE+1, &f[4]);
       
        s_bIndTexScaleChanged = false;
    }

    if( s_nIndTexMtxChanged ) {
        for(int i = 0; i < 3; ++i) {
            if( s_nIndTexMtxChanged & (1<<i) ) {
                int scale = ((u32)bpmem.indmtx[i].col0.s0<<0)|((u32)bpmem.indmtx[i].col1.s1<<2)|((u32)bpmem.indmtx[i].col2.s2<<4);
                float fscale = powf(2.0f,(float)(scale-17)) / 1024.0f;

                // xyz - static matrix
                //TODO w - dynamic matrix scale / 256...... somehow / 4 works better
                glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, C_INDTEXMTX+2*i,
                    bpmem.indmtx[i].col0.ma * fscale, bpmem.indmtx[i].col1.mc * fscale, bpmem.indmtx[i].col2.me * fscale, fscale * 256.0f);
                glProgramEnvParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, C_INDTEXMTX+2*i+1,
                    bpmem.indmtx[i].col0.mb * fscale, bpmem.indmtx[i].col1.md * fscale, bpmem.indmtx[i].col2.mf * fscale, fscale * 256.0f);

                PRIM_LOG("indmtx%d: scale=%f, mat=(%f %f %f; %f %f %f)\n", i,
                    1024.0f*fscale, bpmem.indmtx[i].col0.ma * fscale, bpmem.indmtx[i].col1.mc * fscale, bpmem.indmtx[i].col2.me * fscale,
                    bpmem.indmtx[i].col0.mb * fscale, bpmem.indmtx[i].col1.md * fscale, bpmem.indmtx[i].col2.mf * fscale, fscale);
            }
        }
        s_nIndTexMtxChanged = 0;
    }
}

void PixelShaderMngr::SetColorChanged(int type, int num)
{
    int r=bpmem.tevregs[num].low.a, a=bpmem.tevregs[num].low.b;
    int b=bpmem.tevregs[num].high.a, g=bpmem.tevregs[num].high.b;
    float* pf = &lastRGBAfull[type][num][0];
    pf[0] = (float)r/255.0f;
    pf[1] = (float)g/255.0f;
    pf[2] = (float)b/255.0f;
    pf[3] = (float)a/255.0f;
    s_nColorsChanged[type] |= 1 << num;
    PRIM_LOG("pixel %scolor%d: %f %f %f %f\n", type?"k":"", num, pf[0], pf[1], pf[2], pf[3]);
}

void PixelShaderMngr::SetAlpha(const AlphaFunc& alpha)
{
    if( (alpha.hex&0xffff) != lastAlpha ) {
        lastAlpha = (lastAlpha&~0xffff)|(alpha.hex&0xffff);
        s_bAlphaChanged = true;
    }
}

void PixelShaderMngr::SetDestAlpha(const ConstantAlpha& alpha)
{
    if( alpha.alpha != (lastAlpha>>16) ) {
        lastAlpha = (lastAlpha&~0xff0000)|((alpha.hex&0xff)<<16);
        s_bAlphaChanged = true;
    }
}

void PixelShaderMngr::SetTexDims(int texmapid, u32 width, u32 height, u32 wraps, u32 wrapt)
{
    u32 wh = width|(height<<16)|(wraps<<28)|(wrapt<<30);
    if( lastTexDims[texmapid] != wh ) {
        lastTexDims[texmapid] = wh;
        if( s_nTexDimsChanged[0] == -1 ) {
            s_nTexDimsChanged[0] = s_nTexDimsChanged[1] = texmapid;
        }
        else {
            if( s_nTexDimsChanged[0] > texmapid ) s_nTexDimsChanged[0] = texmapid;
            else if( s_nTexDimsChanged[1] < texmapid ) s_nTexDimsChanged[1] = texmapid;
        }
    }
}

void PixelShaderMngr::SetZTetureBias(u32 bias)
{
    if( lastZBias != bias ) {
        s_bZBiasChanged = true;
        lastZBias = bias;
    }
}

void PixelShaderMngr::SetIndTexScaleChanged()
{
    s_bIndTexScaleChanged = true;
}

void PixelShaderMngr::SetIndMatrixChanged(int matrixidx)
{
    s_nIndTexMtxChanged |= 1 << matrixidx;
}

void PixelShaderMngr::SetGenModeChanged()
{
}

void PixelShaderMngr::SetTevCombinerChanged(int id)
{
}

void PixelShaderMngr::SetTevKSelChanged(int id)
{
}

void PixelShaderMngr::SetTevOrderChanged(int id)
{
}

void PixelShaderMngr::SetTevIndirectChanged(int id)
{
}

void PixelShaderMngr::SetZTetureOpChanged()
{
    s_bZBiasChanged = true;
}

void PixelShaderMngr::SetTexturesUsed(u32 nonpow2tex)
{
    if( s_texturemask != nonpow2tex ) {
        u32 mask = s_texturemask ^ nonpow2tex;
        for(int i = 0; i < 8; ++i) {
            if( mask & (0x10101<<i) ) {
                if( s_nTexDimsChanged[0] > i ) s_nTexDimsChanged[0] = i;
                else if( s_nTexDimsChanged[1] < i ) s_nTexDimsChanged[1] = i;
            }
        }
        s_texturemask = nonpow2tex;
    }
}

void PixelShaderMngr::SetTexDimsChanged(int texmapid)
{
    if( s_nTexDimsChanged[0] > texmapid ) s_nTexDimsChanged[0] = texmapid;
    else if( s_nTexDimsChanged[1] < texmapid ) s_nTexDimsChanged[1] = texmapid;
    SetIndTexScaleChanged();
}

void PixelShaderMngr::SetColorMatrix(const float* pmatrix, const float* pfConstAdd)
{
    glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, C_COLORMATRIX, pmatrix);
    glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, C_COLORMATRIX+1, pmatrix+4);
    glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, C_COLORMATRIX+2, pmatrix+8);
    glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, C_COLORMATRIX+3, pmatrix+12);
    glProgramEnvParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, C_COLORMATRIX+4, pfConstAdd);
}

GLuint PixelShaderMngr::GetColorMatrixProgram()
{
    return s_ColorMatrixProgram;
}

//   old tev->pixelshader notes
//
//   color for this stage (alpha, color) is given by bpmem.tevorders[0].colorchan0
//   konstant for this stage (alpha, color) is given by bpmem.tevksel   
//   inputs are given by bpmem.combiners[0].colorC.a/b/c/d     << could be current chan color
//   according to GXTevColorArg table above
//   output is given by .outreg
//   tevtemp is set according to swapmodetables and 
const float epsilon = 1.0f/255.0f;

const char *tevKSelTableC[] = // KCSEL
{
    "1.0f,1.0f,1.0f",            //1   = 0x00
    "0.875,0.875,0.875",//7_8 = 0x01
    "0.75,0.75,0.75",	//3_4 = 0x02
    "0.625,0.625,0.625",//5_8 = 0x03
    "0.5,0.5,0.5",      //1_2 = 0x04
    "0.375,0.375,0.375",//3_8 = 0x05
    "0.25,0.25,0.25",   //1_4 = 0x06
    "0.125,0.125,0.125",//1_8 = 0x07
    "ERROR", //0x08
    "ERROR", //0x09
    "ERROR", //0x0a
    "ERROR", //0x0b
    I_KCOLORS"[0].rgb",//K0 = 0x0C
    I_KCOLORS"[1].rgb",//K1 = 0x0D
    I_KCOLORS"[2].rgb",//K2 = 0x0E
    I_KCOLORS"[3].rgb",//K3 = 0x0F
    I_KCOLORS"[0].rrr",//K0_R = 0x10
    I_KCOLORS"[1].rrr",//K1_R = 0x11
    I_KCOLORS"[2].rrr",//K2_R = 0x12
    I_KCOLORS"[3].rrr",//K3_R = 0x13
    I_KCOLORS"[0].ggg",//K0_G = 0x14 
    I_KCOLORS"[1].ggg",//K1_G = 0x15
    I_KCOLORS"[2].ggg",//K2_G = 0x16
    I_KCOLORS"[3].ggg",//K3_G = 0x17
    I_KCOLORS"[0].bbb",//K0_B = 0x18
    I_KCOLORS"[1].bbb",//K1_B = 0x19
    I_KCOLORS"[2].bbb",//K2_B = 0x1A
    I_KCOLORS"[3].bbb",//K3_B = 0x1B
    I_KCOLORS"[0].aaa",//K0_A = 0x1C
    I_KCOLORS"[1].aaa",//K1_A = 0x1D
    I_KCOLORS"[2].aaa",//K2_A = 0x1E
    I_KCOLORS"[3].aaa",//K3_A = 0x1F
};

const char *tevKSelTableA[] = // KASEL
{
    "1.0f",    //1   = 0x00
    "0.875f",//7_8 = 0x01
    "0.75f",	//3_4 = 0x02
    "0.625f",//5_8 = 0x03
    "0.5f",  //1_2 = 0x04
    "0.375f",//3_8 = 0x05
    "0.25f", //1_4 = 0x06
    "0.125f",//1_8 = 0x07
    "ERROR", //0x08
    "ERROR", //0x09
    "ERROR", //0x0a
    "ERROR", //0x0b
    "ERROR", //0x0c
    "ERROR", //0x0d
    "ERROR", //0x0e
    "ERROR", //0x0f
    I_KCOLORS"[0].r",//K0_R = 0x10
    I_KCOLORS"[1].r",//K1_R = 0x11
    I_KCOLORS"[2].r",//K2_R = 0x12
    I_KCOLORS"[3].r",//K3_R = 0x13
    I_KCOLORS"[0].g",//K0_G = 0x14
    I_KCOLORS"[1].g",//K1_G = 0x15
    I_KCOLORS"[2].g",//K2_G = 0x16
    I_KCOLORS"[3].g",//K3_G = 0x17
    I_KCOLORS"[0].b",//K0_B = 0x18
    I_KCOLORS"[1].b",//K1_B = 0x19
    I_KCOLORS"[2].b",//K2_B = 0x1A
    I_KCOLORS"[3].b",//K3_B = 0x1B
    I_KCOLORS"[0].a",//K0_A = 0x1C
    I_KCOLORS"[1].a",//K1_A = 0x1D
    I_KCOLORS"[2].a",//K2_A = 0x1E
    I_KCOLORS"[3].a",//K3_A = 0x1F
};

const char *tevScaleTable[] = // CS
{
    "1.0f",   //SCALE_1
    "2.0f", //SCALE_2
    "4.0f", //SCALE_4
    "0.5f",//DIVIDE_2
};

const char *tevBiasTable[] = // TB
{
    "",      //ZERO,
    "+0.5f",  //ADDHALF,
    "-0.5f",  //SUBHALF,
    "",
};

const char *tevOpTable[] = { // TEV
    "+",      //TEVOP_ADD = 0,
    "-",      //TEVOP_SUB = 1,
};

const char *tevCompOpTable[] = { ">", "==" };

#define TEVCMP_R8    0
#define TEVCMP_GR16  1
#define TEVCMP_BGR24 2
#define TEVCMP_RGB8  3

const char *tevCInputTable[] = // CC
{
    "prev.rgb",         //CPREV,
    "prev.aaa",           //APREV,
    "c0.rgb",           //C0,
    "c0.aaa",             //A0,
    "c1.rgb",           //C1,
    "c1.aaa",             //A1,
    "c2.rgb",           //C2,
    "c2.aaa",             //A2,
    "textemp.rgb",      //TEXC,
    "textemp.aaa",        //TEXA,
    "rastemp.rgb",    //RASC,
    "rastemp.aaa",      //RASA,
    "float3(1.0f,1.0f,1.0f)",              //ONE,
    "float3(.5f,.5f,.5f)",              //HALF,
    "konsttemp.rgb",    //KONST,
    "float3(0.0f,0.0f,0.0f)",              //ZERO
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
};
const char *tevCInputTable2[] = // CC
{
    "prev",         //CPREV,
    "(prev.aaa)",           //APREV,
    "c0",           //C0,
    "(c0.aaa)",             //A0,
    "c1",           //C1,
    "(c1.aaa)",             //A1,
    "c2",           //C2,
    "(c2.aaa)",             //A2,
    "textemp",      //TEXC,
    "(textemp.aaa)",        //TEXA,
    "rastemp",    //RASC,
    "(rastemp.aaa)",      //RASA,
    "float3(1.0f,1.0f,1.0f)",              //ONE,
    "float3(.5f,.5f,.5f)",              //HALF,
    "konsttemp", //"konsttemp.rgb",    //KONST,
    "float3(0.0f,0.0f,0.0f)",              //ZERO
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
    "PADERROR",	"PADERROR",	"PADERROR",	"PADERROR",
};

const char *tevAInputTable[] = // CA
{
    "prev.a",           //APREV,
    "c0.a",             //A0,
    "c1.a",             //A1,
    "c2.a",             //A2,
    "textemp.a",        //TEXA,
    "rastemp.a",        //RASA,
    "konsttemp.a",      //KONST
    "0.0", //ZERO
    "PADERROR", "PADERROR", "PADERROR", "PADERROR",
    "PADERROR", "PADERROR", "PADERROR", "PADERROR",
    "PADERROR", "PADERROR", "PADERROR", "PADERROR",
    "PADERROR", "PADERROR", "PADERROR",
};	

const char *tevAInputTable2[] = // CA
{
    "prev",           //APREV,
    "c0",             //A0,
    "c1",             //A1,
    "c2",             //A2,
    "textemp",        //TEXA,
    "rastemp",        //RASA,
    "konsttemp",      //KONST,  (hw1 had quarter)
    "float4(0,0,0,0)", //ZERO
    "PADERROR", "PADERROR", "PADERROR", "PADERROR",
    "PADERROR", "PADERROR", "PADERROR", "PADERROR",
    "PADERROR", "PADERROR", "PADERROR", "PADERROR",
    "PADERROR", "PADERROR", "PADERROR", "PADERROR",
};	

const char *tevRasTable[] =
{
    "colors[0]",
    "colors[1]",
    "ERROR", //2
    "ERROR", //3
    "ERROR", //4
    "alphabump", // use bump alpha
    "(alphabump*(255.0f/248.0f))", //normalized
    "float4(0,0,0,0)", // zero
};

const char *tevTexFunc[] = { "tex2D", "texRECT" };

const char *tevCOutputTable[] = { "prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb" };
const char *tevAOutputTable[] = { "prev.a", "c0.a", "c1.a", "c2.a" };
const char* tevIndAlphaSel[] = {"", "x", "y", "z"};
const char* tevIndAlphaScale[] = {"", "*32","*16","*8"};
const char* tevIndBiasField[] = {"", "x", "y", "xy", "z", "xz", "yz", "xyz"}; // indexed by bias
const char* tevIndBiasAdd[] = {"-128.0f", "1.0f", "1.0f", "1.0f" }; // indexed by fmt
const char* tevIndWrapStart[] = {"0", "256", "128", "64", "32", "16", "0.001" };
const char* tevIndFmtScale[] = {"255.0f", "31.0f", "15.0f", "8.0f" };

void PixelShaderMngr::GetPixelShaderId(PixelShaderMngr::PIXELSHADERUID& uid)
{
    u32 projtexcoords = 0;
    for (u32 i = 0; i < bpmem.genMode.numtevstages+1; i++) {
        if( bpmem.tevorders[i/2].getEnable(i&1) ) {
            int texcoord = bpmem.tevorders[i/2].getTexCoord(i&1);
            if( xfregs.texcoords[texcoord].texmtxinfo.projection )
                projtexcoords |= 1<<texcoord;
        }
    }

    u32 zbufrender = (Renderer::GetZBufferTarget()&&bpmem.zmode.updateenable)?1:0;
    u32 zBufRenderToCol0 = Renderer::GetRenderMode()!=Renderer::RM_Normal;
    uid.values[0] = (u32)bpmem.genMode.numtevstages|((u32)bpmem.genMode.numindstages<<4)|((u32)bpmem.genMode.numtexgens<<7)
        |((u32)bpmem.dstalpha.enable<<11)|((u32)((bpmem.alphaFunc.hex>>16)&0xff)<<12)|(projtexcoords<<20)|((u32)bpmem.ztex2.op<<28)
        |(zbufrender<<30)|(zBufRenderToCol0<<31);

    s_curuid.values[0] = (s_curuid.values[0]&~0x0ff00000)|(projtexcoords<<20);

    // swap table
    for(int i = 0; i < 8; i += 2)
        ((u8*)&uid.values[1])[i/2] = (bpmem.tevksel[i].hex&0xf)|((bpmem.tevksel[i+1].hex&0xf)<<4);

    uid.values[2] = s_texturemask;
    int hdr = 3;

    u32* pcurvalue = &uid.values[hdr];
    for(u32 i = 0; i < bpmem.genMode.numtevstages+1; ++i) {
        TevStageCombiner::ColorCombiner &cc = bpmem.combiners[i].colorC;
        TevStageCombiner::AlphaCombiner &ac = bpmem.combiners[i].alphaC;

        u32 val0 = cc.hex&0xffffff;
        u32 val1 = ac.hex&0xffffff;
        val0 |= bpmem.tevksel[i/2].getKC(i&1)<<24;
        val1 |= bpmem.tevksel[i/2].getKA(i&1)<<24;

        pcurvalue[0] = val0;
        pcurvalue[1] = val1;
        pcurvalue+=2;
    }

    for(u32 i = 0; i < (bpmem.genMode.numtevstages+1)/2; ++i) {
        u32 val0, val1;
        if( bpmem.tevorders[i].hex&0x40 ) val0 = bpmem.tevorders[i].hex&0x3ff;
        else val0 = bpmem.tevorders[i].hex&0x380;
        if( bpmem.tevorders[i].hex&0x40000 ) val1 = (bpmem.tevorders[i].hex&0x3ff000)>>12;
        else val1 = (bpmem.tevorders[i].hex&0x380000)>>12;

        switch(i % 3) {
            case 0: pcurvalue[0] = val0|(val1<<10); break;
            case 1: pcurvalue[0] |= val0<<20; pcurvalue[1] = val1; pcurvalue++; break;
            case 2: pcurvalue[1] |= (val0<<10)|(val1<<20); pcurvalue++; break;
        }
    }

    if( (bpmem.genMode.numtevstages+1)&1 ) { // odd
        u32 val0;
        if( bpmem.tevorders[bpmem.genMode.numtevstages/2].hex&0x40 ) val0 = bpmem.tevorders[bpmem.genMode.numtevstages/2].hex&0x3ff;
        else val0 = bpmem.tevorders[bpmem.genMode.numtevstages/2].hex&0x380;

        switch(bpmem.genMode.numtevstages % 3) {
            case 0: pcurvalue[0] = val0; break;
            case 1: pcurvalue[0] |= val0<<20; break;
            case 2: pcurvalue[1] |= (val0<<10); pcurvalue++; break;
        }
    }

    if( (bpmem.genMode.numtevstages % 3) != 2 )
        ++pcurvalue;

    uid.tevstages = (u32)(pcurvalue-&uid.values[0]-hdr);

    for(u32 i = 0; i < bpmem.genMode.numindstages; ++i) {
        u32 val = bpmem.tevind[i].hex&0x1fffff; // 21 bits
        switch(i%3) {
            case 0: pcurvalue[0] = val; break;
            case 1: pcurvalue[0] |= val<<21; pcurvalue[1] = val>>11; ++pcurvalue; break;
            case 2: pcurvalue[0] |= val<<10; ++pcurvalue; break;
        }
    }

    uid.indstages = (u32)(pcurvalue-&uid.values[0]-2-uid.tevstages);
}

#define WRITE p+=sprintf

const char *swapColors = "rgba";
char swapModeTable[4][5];

void BuildSwapModeTable()
{
    //bpmem.tevregs[0].
    for (int i=0; i<4; i++)
    {
        swapModeTable[i][0]=swapColors[bpmem.tevksel[i*2].swap1];
        swapModeTable[i][1]=swapColors[bpmem.tevksel[i*2].swap2];
        swapModeTable[i][2]=swapColors[bpmem.tevksel[i*2+1].swap1];
        swapModeTable[i][3]=swapColors[bpmem.tevksel[i*2+1].swap2];
        swapModeTable[i][4]=0;
    }
}

static char text[16384];
bool PixelShaderMngr::GeneratePixelShader(FRAGMENTSHADER& ps)
{
    DVSTARTPROFILE();

    BuildSwapModeTable();
    int numStages = bpmem.genMode.numtevstages + 1;
    int numTexgen = bpmem.genMode.numtexgens;

    char *p = text;
    WRITE(p,"//Pixel Shader for TEV stages\n");
    WRITE(p,"//%i TEV stages, %i texgens, %i IND stages\n",
        numStages,numTexgen,bpmem.genMode.numindstages);

    bool bRenderZ = Renderer::GetZBufferTarget() != 0 && bpmem.zmode.updateenable;
    bool bOutputZ = bpmem.ztex2.op != ZTEXTURE_DISABLE;
    bool bInputZ = bpmem.ztex2.op==ZTEXTURE_ADD || bRenderZ;

    bool bRenderZToCol0 = Renderer::GetRenderMode()!=Renderer::RM_Normal; // output z and alpha to color0
    assert( !bRenderZToCol0 || bRenderZ );

    int ztexcoord = -1;
    if( bInputZ )
        ztexcoord = numTexgen == 0 ? 0 : numTexgen-1;

    int nIndirectStagesUsed = 0;
    if( bpmem.genMode.numindstages > 0 ) {
        for(int i = 0; i < numStages; ++i) {
            if( bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages ) {
                nIndirectStagesUsed |= 1<<bpmem.tevind[i].bt;
            }
        }
    }

    // samplers
    if( s_texturemask ) {
        WRITE(p,"uniform samplerRECT ");
        bool bfirst = true;
        for(int i = 0; i < 8; ++i) {
            if( s_texturemask & (1<<i) ) {
                WRITE(p, "%s samp%d : register(s%d)", bfirst?"":",", i, i);
                bfirst = false;
            }
        }
        WRITE(p, ";\n");
    }

    if( s_texturemask != 0xff ) {
        WRITE(p,"uniform sampler2D ");
        bool bfirst = true;
        for(int i = 0; i < 8; ++i) {
            if( !(s_texturemask & (1<<i)) ) {
                WRITE(p, "%s samp%d : register(s%d)", bfirst?"":",",i, i);
                bfirst = false;
            }
        }
        WRITE(p, ";\n");
    }

    WRITE(p, "\n");

    WRITE(p,"uniform float4 "I_COLORS"[4] : register(c%d);\n", C_COLORS);
    WRITE(p, "uniform float4 "I_KCOLORS"[4] : register(c%d);\n", C_KCOLORS);
    WRITE(p, "uniform float4 "I_ALPHA"[1] : register(c%d);\n", C_ALPHA);
    WRITE(p, "uniform float4 "I_TEXDIMS"[8] : register(c%d);\n", C_TEXDIMS);
    WRITE(p, "uniform float4 "I_ZBIAS"[2] : register(c%d);\n", C_ZBIAS);
    WRITE(p, "uniform float4 "I_INDTEXSCALE"[2] : register(c%d);\n", C_INDTEXSCALE);
    WRITE(p, "uniform float4 "I_INDTEXMTX"[6] : register(c%d);\n", C_INDTEXMTX);

    WRITE(p,"void main(\n");

    WRITE(p, "out half4 ocol0 : COLOR0,\n");
    if( bRenderZ && !bRenderZToCol0 )
        WRITE(p, "out half4 ocol1 : COLOR1,\n");

    if( bOutputZ )
        WRITE(p,"  out float depth : DEPTH,\n");

    // if zcoord might come from vertex shader in texcoord
    if( bInputZ ) {
        if (numTexgen) {
            for(int i = 0; i < numTexgen; ++i)
                WRITE(p,"  in float%d uv%d : TEXCOORD%d, \n", i==ztexcoord?4:3, i,i);
        }
        else
            WRITE(p,"  in float4 uv0 : TEXCOORD0,"); //HACK
    }
    else {
        if (numTexgen) {
            for(int i = 0; i < numTexgen; ++i)
                WRITE(p,"  in float3 uv%d : TEXCOORD%d,\n",i,i);
        }
        else
            WRITE(p,"  in float3 uv0 : TEXCOORD0,\n"); //HACK
    }

    WRITE(p, "  in float4 colors[2] : COLOR0){\n");

    char* pmainstart = p;

    WRITE(p,"float4 c0="I_COLORS"[1],c1="I_COLORS"[2],c2="I_COLORS"[3],prev=float4(0.0f,0.0f,0.0f,0.0f),textemp,rastemp,konsttemp=float4(0.0f,0.0f,0.0f,0.0f);\n"
            "float3 comp16 = float3(1,255,0), comp24 = float3(1,255,255*255);\n"
            "float4 alphabump=0;\n"
            "float3 tevcoord;\n"
            "float2 wrappedcoord, tempcoord;\n");

    //if( bOutputZ ) WRITE(p,"  float depth;\n");
//    WRITE(p, "return 1;}\n");
//    return PixelShaderMngr::CompilePixelShader(ps, text);

    // indirect texture map lookup
    for(u32 i = 0; i < bpmem.genMode.numindstages; ++i) {
        if( nIndirectStagesUsed & (1<<i) ) {
            // perform indirect texture map lookup
            // note that we have to scale by the regular texture map's coordinates since this is a texRECT call
            // (and we have to match with the game's texscale calls)
            int texcoord = bpmem.tevindref.getTexCoord(i);
            if( s_texturemask & (1<<bpmem.tevindref.getTexMap(i)) ) {
                // TODO - Buggy? "Too many arguments for format"
				WRITE(p, "float2 induv%d=uv%d.xy * "I_INDTEXSCALE"[%d].%s;\n", i, texcoord, i/2, (i&1)?"zw":"xy",bpmem.tevindref.getTexMap(i));

                char str[16];
                sprintf(str, "induv%d", i);
                WrapNonPow2Tex(p, str, bpmem.tevindref.getTexMap(i));
                WRITE(p,"float3 indtex%d=texRECT(samp%d,induv%d.xy).abg;\n", i, bpmem.tevindref.getTexMap(i), i);
            }
            else {
                WRITE(p,"float3 indtex%d=tex2D(samp%d,uv%d.xy*"I_INDTEXSCALE"[%d].%s).abg;\n", i, bpmem.tevindref.getTexMap(i), texcoord, i/2, (i&1)?"zw":"xy");
            }
        }
    }

    for (int i=0; i<numStages; i++)
        WriteStage(p,i); //build the equation for this stage

    if( bOutputZ ) {
        // use the texture input of the last texture stage (textemp), hopefully this has been read and is in correct format...
        if( bpmem.ztex2.op == ZTEXTURE_ADD ) {
            WRITE(p, "depth = frac(dot("I_ZBIAS"[0].xyzw, textemp.xyzw) + "I_ZBIAS"[1].w + uv%d.w);\n", ztexcoord);
        }
        else {
            _assert_(bpmem.ztex2.op == ZTEXTURE_REPLACE);
            WRITE(p, "depth = frac(dot("I_ZBIAS"[0].xyz, textemp.xyz) + "I_ZBIAS"[0].w);\n");
        }
    }

    //if( bpmem.genMode.numindstages ) WRITE(p, "prev.rg = indtex0.xy;\nprev.b = 0;\n");
    
    if( !WriteAlphaTest(p) ) {
        // alpha test will always fail, so restart the shader and just make it an empty function
        p = pmainstart;
        WRITE(p, "ocol0 = 0;\n");
    }
    else {
        if( !bRenderZToCol0 ) {
            if (bpmem.dstalpha.enable)
                WRITE(p,"  ocol0 = float4(prev.rgb,"I_ALPHA"[0].w);\n");
            else
                WRITE(p,"  ocol0 = prev;\n");
        }
    }

    if( bRenderZ ) {
        // write depth as color
        if( bRenderZToCol0 ) {
            if( bOutputZ )
                WRITE(p, "ocol0.xyz = frac(float3(256.0f*256.0f, 256.0f, 1.0f) * depth);\n");
            else
                WRITE(p, "ocol0.xyz = frac(float3(256.0f*256.0f, 256.0f, 1.0f) * uv%d.w);\n", ztexcoord);
            WRITE(p, "ocol0.w = prev.w;\n");
        }
        else {
            if( bOutputZ )
                WRITE(p, "ocol1 = frac(float4(256.0f*256.0f, 256.0f, 1.0f, 0.0f) * depth);\n");
            else
                WRITE(p, "ocol1 = frac(float4(256.0f*256.0f, 256.0f, 1.0f, 0.0f) * uv%d.w);\n", ztexcoord);
        }
    }

    WRITE(p,"}\n\0");
    
    return PixelShaderMngr::CompilePixelShader(ps, text);
}

void WriteStage(char *&p, int n)
{
    char *rasswap = swapModeTable[bpmem.combiners[n].alphaC.rswap];
    char *texswap = swapModeTable[bpmem.combiners[n].alphaC.tswap];

    int colchan = bpmem.tevorders[n/2].getColorChan(n&1);

    int texcoord = bpmem.tevorders[n/2].getTexCoord(n&1);
    int texfun = xfregs.texcoords[texcoord].texmtxinfo.projection;
    bool bHasIndStage = bpmem.tevind[n].IsActive() && bpmem.tevind[n].bt < bpmem.genMode.numindstages;

    if( bHasIndStage ) {
        // perform the indirect op on the incoming regular coordinates using indtex%d as the offset coords
        bHasIndStage = true;
        int texmap = bpmem.tevorders[n/2].getEnable(n&1) ? bpmem.tevorders[n/2].getTexMap(n&1) : bpmem.tevindref.getTexMap(bpmem.tevind[n].bt);

        if( bpmem.tevind[n].bs != ITBA_OFF )
            // write the bump alpha
            WRITE(p, "alphabump = %s (indtex%d.%s %s);\n", bpmem.tevind[n].fmt==ITF_8?"":"frac", bpmem.tevind[n].bt, 
                tevIndAlphaSel[bpmem.tevind[n].bs], tevIndAlphaScale[bpmem.tevind[n].fmt]);

        // bias
        WRITE(p, "float3 indtevcrd%d = indtex%d;\n", n, bpmem.tevind[n].bt);
        WRITE(p, "indtevcrd%d.xy *= %s;\n", n, tevIndFmtScale[bpmem.tevind[n].fmt]);
        if( bpmem.tevind[n].bias != ITB_NONE )
            WRITE(p, "indtevcrd%d.%s += %s;\n", n, tevIndBiasField[bpmem.tevind[n].bias], tevIndBiasAdd[bpmem.tevind[n].fmt]);

        // multiply by offset matrix and scale
        if( bpmem.tevind[n].mid != 0 ) {
            if( bpmem.tevind[n].mid <= 3 ) {
                int mtxidx = 2*(bpmem.tevind[n].mid-1);
                WRITE(p, "float2 indtevtrans%d = float2(dot("I_INDTEXMTX"[%d].xyz, indtevcrd%d), dot("I_INDTEXMTX"[%d].xyz, indtevcrd%d));\n",
                    n, mtxidx, n, mtxidx+1, n);
            }
            else if( bpmem.tevind[n].mid <= 5 ) { // s matrix
                int mtxidx = 2*(bpmem.tevind[n].mid-5);
                WRITE(p, "float2 indtevtrans%d = "I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.xx;\n", n, mtxidx, texcoord, n);
            }
            else if( bpmem.tevind[n].mid <= 9 ) { // t matrix
                int mtxidx = 2*(bpmem.tevind[n].mid-9);
                WRITE(p, "float2 indtevtrans%d = "I_INDTEXMTX"[%d].ww * uv%d.xy * indtevcrd%d.yy;\n", n, mtxidx, texcoord, n);
            }
            else {
                WRITE(p, "float2 indtevtrans%d = 0;\n", n, n);
            }
        }
        else {
            WRITE(p, "float2 indtevtrans%d = 0;\n", n, n);
        }

        // wrapping
        if( !bpmem.tevorders[n/2].getEnable(n&1) || (s_texturemask & (1<<texmap)) ) {
            // non pow2

            if( bpmem.tevind[n].sw != ITW_OFF || bpmem.tevind[n].tw != ITW_OFF ) {
                if( bpmem.tevind[n].sw == ITW_0 ) {
                    if( bpmem.tevind[n].tw == ITW_0 ) {
                        // zero out completely
                        WRITE(p, "wrappedcoord = float2(0.0f,0.0f);\n");
                    }
                    else {
                        WRITE(p, "wrappedcoord.x = fmod( (uv%d.x+%s)*"I_TEXDIMS"[%d].x*"I_TEXDIMS"[%d].z, %s);\n"
                            "wrappedcoord.y = 0;\n", texcoord, tevIndWrapStart[bpmem.tevind[n].sw], texmap, texmap, tevIndWrapStart[bpmem.tevind[n].sw]);
                    }
                }
                else if( bpmem.tevind[n].tw == ITW_0 ) {
                    WRITE(p, "wrappedcoord.y = fmod( (uv%d.y+%s)*"I_TEXDIMS"[%d].y*"I_TEXDIMS"[%d].w, %s);\n"
                        "wrappedcoord.x = 0;\n", texcoord, tevIndWrapStart[bpmem.tevind[n].tw], texmap, texmap, tevIndWrapStart[bpmem.tevind[n].tw]);
                }
                else {
                    WRITE(p, "wrappedcoord = fmod( (uv%d.xy+float2(%s,%s))*"I_TEXDIMS"[%d].xy*"I_TEXDIMS"[%d].zw, float2(%s,%s));\n", texcoord,
                        tevIndWrapStart[bpmem.tevind[n].sw], tevIndWrapStart[bpmem.tevind[n].tw],texmap,texmap,
                        tevIndWrapStart[bpmem.tevind[n].sw], tevIndWrapStart[bpmem.tevind[n].tw]);
                }
            }
            else {
                WRITE(p, "wrappedcoord = uv%d.xy*"I_TEXDIMS"[%d].xy;\n", texcoord, texmap);
            }
        }
        else {
            // pow of 2
            WRITE(p, "indtevtrans%d.xy *= "I_TEXDIMS"[%d].xy * "I_TEXDIMS"[%d].zw;\n", n, texmap, texmap);

            // mult by bitdepth / tex dimensions
            if( bpmem.tevind[n].sw != ITW_OFF || bpmem.tevind[n].tw != ITW_OFF ) {
                if( bpmem.tevind[n].sw == ITW_0 ) {
                    if( bpmem.tevind[n].tw == ITW_0 ) {
                        // zero out completely
                        WRITE(p, "wrappedcoord = float2(0.0f,0.0f);\n");
                    }
                    else {
                        WRITE(p, "wrappedcoord.x = "I_TEXDIMS"[%d].x * fmod( uv%d.x+%s, "I_TEXDIMS"[%d].z*%s);\n"
                            "wrappedcoord.y = 0;\n", texmap, texcoord, tevIndWrapStart[bpmem.tevind[n].sw], texmap, tevIndWrapStart[bpmem.tevind[n].sw]);
                    }
                }
                else if( bpmem.tevind[n].tw == ITW_0 ) {
                    WRITE(p, "wrappedcoord.y = "I_TEXDIMS"[%d].y * fmod( uv%d.y+%s, "I_TEXDIMS"[%d].w*%s);\n"
                        "wrappedcoord.x = 0;\n", texmap, texcoord, tevIndWrapStart[bpmem.tevind[n].tw], texmap, tevIndWrapStart[bpmem.tevind[n].tw]);
                }
                else {
                    // have to add an offset or else might get negative values!
                    WRITE(p, "wrappedcoord = "I_TEXDIMS"[%d].xy * fmod( uv%d.xy+float2(%s,%s), "I_TEXDIMS"[%d].zw*float2(%s,%s));\n", texmap, texcoord,
                        tevIndWrapStart[bpmem.tevind[n].sw], tevIndWrapStart[bpmem.tevind[n].tw], texmap, 
                        tevIndWrapStart[bpmem.tevind[n].sw], tevIndWrapStart[bpmem.tevind[n].tw]);
                }
            }
            else {
                WRITE(p, "wrappedcoord = uv%d.xy;\n", texcoord);
            }
        }

        if( bpmem.tevind[n].fb_addprev ) {
            // add previous tevcoord

            if( texfun == XF_TEXPROJ_STQ ) {
                WRITE(p,"tevcoord.xy += wrappedcoord/uv%d.z + indtevtrans%d;\n", texcoord, n);
                //WRITE(p,"tevcoord.z += uv%d.z;\n", texcoord);
            }
            else {
                WRITE(p,"tevcoord.xy += wrappedcoord + indtevtrans%d;\n", n);
            }
        }
        else {
            WRITE(p,"tevcoord.xy = wrappedcoord/uv%d.z + indtevtrans%d;\n", texcoord, n);
            //if( texfun == XF_TEXPROJ_STQ )
            //    WRITE(p,"tevcoord.z = uv%d.z;\n", texcoord);
        }
    }

    WRITE(p,"rastemp=%s.%s;\n",tevRasTable[bpmem.tevorders[n/2].getColorChan(n&1)],rasswap);

    if (bpmem.tevorders[n/2].getEnable(n&1)) {
        
        int texmap = bpmem.tevorders[n/2].getTexMap(n&1);
        if(!bHasIndStage) {
            // calc tevcord
            if( s_texturemask & (1<<texmap) ) {
                // nonpow2
                if( texfun == XF_TEXPROJ_STQ )
                    WRITE(p,"tevcoord.xy = uv%d.xy / uv%d.z;\n", texcoord, texcoord);
                else
                    WRITE(p,"tevcoord.xy = uv%d.xy;\n", texcoord);
                WrapNonPow2Tex(p, "tevcoord", texmap);
            }
            else {
                if( texfun == XF_TEXPROJ_STQ ) {
                    WRITE(p,"tevcoord.xy = "I_TEXDIMS"[%d].xy * uv%d.xy / uv%d.z;\n", texmap, texcoord, texcoord);
                }
                else {
                    WRITE(p,"tevcoord.xy = "I_TEXDIMS"[%d].xy * uv%d.xy;\n", texmap, texcoord);
                }
            }
        }
        else if( s_texturemask & (1<<texmap) ) {
            // if non pow 2, have to manually repeat
            //WrapNonPow2Tex(p, "tevcoord", texmap);
            bool bwraps = !!(s_texturemask & (0x100<<texmap));
            bool bwrapt = !!(s_texturemask & (0x10000<<texmap));
                
            if( bwraps || bwrapt ) {
                const char* field = bwraps ? (bwrapt ? "xy" : "x") : "y";
                WRITE(p, "tevcoord.%s = fmod(tevcoord.%s+32*"I_TEXDIMS"[%d].%s,"I_TEXDIMS"[%d].%s);\n", field, field, texmap, field, texmap, field);
            }
        }

        if( s_texturemask & (1<<texmap) )
            WRITE(p,"textemp=texRECT(samp%d,tevcoord.xy).%s;\n", texmap, texswap);
        else
            WRITE(p,"textemp=tex2D(samp%d,tevcoord.xy).%s;\n", texmap, texswap);
    }
    else
        WRITE(p,"textemp=float4(1,1,1,1);\n");

    int kc = bpmem.tevksel[n/2].getKC(n&1);
    int ka = bpmem.tevksel[n/2].getKA(n&1);

    TevStageCombiner::ColorCombiner &cc = bpmem.combiners[n].colorC;
    TevStageCombiner::AlphaCombiner &ac = bpmem.combiners[n].alphaC;

    bool bCKonst = cc.a == TEVCOLORARG_KONST || cc.b == TEVCOLORARG_KONST || cc.c == TEVCOLORARG_KONST || cc.d == TEVCOLORARG_KONST;
    bool bAKonst = ac.a == TEVALPHAARG_KONST || ac.b == TEVALPHAARG_KONST || ac.c == TEVALPHAARG_KONST || ac.d == TEVALPHAARG_KONST;
    if( bCKonst || bAKonst )
        WRITE(p,"konsttemp=float4(%s,%s);\n",tevKSelTableC[kc],tevKSelTableA[ka]);  

    WRITE(p,"%s= ", tevCOutputTable[cc.dest]); 

    // combine the color channel
    if (cc.bias != 3) {  // if not compare
        //normal color combiner goes here
        WRITE(p,"   %s*(%s%s",tevScaleTable[cc.shift],tevCInputTable[cc.d],tevOpTable[cc.op]);
        WRITE(p,"lerp(%s,%s,%s) %s);\n",
            tevCInputTable[cc.a],tevCInputTable[cc.b],
            tevCInputTable[cc.c],tevBiasTable[cc.bias]);
    }
    else {
        int cmp = (cc.shift<<1)|cc.op|8; // comparemode stored here
        switch(cmp) {
        case TEVCMP_R8_GT:
        case TEVCMP_RGB8_GT: // per component compares
            WRITE(p,"   %s + ((%s.%s > %s.%s) ? %s : float3(0.0f,0.0f,0.0f));\n",
                tevCInputTable[cc.d],tevCInputTable2[cc.a], cmp==TEVCMP_R8_GT?"r":"rgb", tevCInputTable2[cc.b], cmp==TEVCMP_R8_GT?"r":"rgb", tevCInputTable[cc.c]);
            break;
        case TEVCMP_R8_EQ:
        case TEVCMP_RGB8_EQ:
            WRITE(p,"   %s + (abs(%s.r - %s.r)<%f ? %s : float3(0.0f,0.0f,0.0f));\n",
                tevCInputTable[cc.d],tevCInputTable2[cc.a], tevCInputTable2[cc.b],epsilon,tevCInputTable[cc.c]);
            break;
        
        case TEVCMP_GR16_GT: // 16 bit compares: 255*g+r (probably used for ztextures, so make sure in ztextures, g is the most significant byte)
        case TEVCMP_BGR24_GT: // 24 bit compares: 255*255*b+255*g+r
            WRITE(p,"   %s + (( dot(%s.rgb-%s.rgb, comp%s) > 0) ? %s : float3(0.0f,0.0f,0.0f));\n",
                tevCInputTable[cc.d],tevCInputTable2[cc.a], tevCInputTable2[cc.b], cmp==TEVCMP_GR16_GT?"16":"24", tevCInputTable[cc.c]);
            break;
        case TEVCMP_GR16_EQ:
        case TEVCMP_BGR24_EQ:
            WRITE(p,"   %s + (abs(dot(%s.rgb - %s.rgb, comp%s))<%f ? %s : float3(0.0f,0.0f,0.0f));\n",
                tevCInputTable[cc.d],tevCInputTable2[cc.a], tevCInputTable2[cc.b],cmp==TEVCMP_GR16_GT?"16":"24",epsilon,tevCInputTable[cc.c]);
            break;
        default:
            WRITE(p,"float3(0.0f,0.0f,0.0f);\n");
            break;
        }
    }
    
    if( cc.clamp )
        WRITE(p, "%s = clamp(%s,0.0f,1.0f);\n", tevCOutputTable[cc.dest],tevCOutputTable[cc.dest]);

    // combine the alpha channel
    WRITE(p,"%s= ", tevAOutputTable[ac.dest]);

    if (ac.bias != 3) { // if not compare
        //normal alpha combiner goes here
        WRITE(p,"   %s*(%s%s",tevScaleTable[ac.shift],tevAInputTable[ac.d],tevOpTable[ac.op]);
        WRITE(p,"lerp(%s,%s,%s) %s)\n",
            tevAInputTable[ac.a],tevAInputTable[ac.b],
            tevAInputTable[ac.c],tevBiasTable[ac.bias]);
    }
    else {
        //compare alpha combiner goes here
        int cmp = (ac.shift<<1)|ac.op|8; // comparemode stored here
        switch(cmp) {
        case TEVCMP_R8_GT:
        case TEVCMP_A8_GT:
            WRITE(p,"   %s + ((%s.%s > %s.%s) ? %s : 0)\n",
                tevAInputTable[ac.d],tevAInputTable2[ac.a], cmp==TEVCMP_R8_GT?"r":"a", tevAInputTable2[ac.b], cmp==TEVCMP_R8_GT?"r":"a", tevAInputTable[ac.c]);
            break;
        case TEVCMP_R8_EQ:
        case TEVCMP_A8_EQ:
            WRITE(p,"   %s + (abs(%s.r - %s.r)<%f ? %s : 0)\n",
                tevAInputTable[ac.d],tevAInputTable2[ac.a], tevAInputTable2[ac.b],epsilon,tevAInputTable[ac.c]);
            break;
        
        case TEVCMP_GR16_GT: // 16 bit compares: 255*g+r (probably used for ztextures, so make sure in ztextures, g is the most significant byte)
        case TEVCMP_BGR24_GT: // 24 bit compares: 255*255*b+255*g+r
            WRITE(p,"   %s + (( dot(%s.rgb-%s.rgb, comp%s) > 0) ? %s : 0)\n",
                tevAInputTable[ac.d],tevAInputTable2[ac.a], tevAInputTable2[ac.b], cmp==TEVCMP_GR16_GT?"16":"24", tevAInputTable[ac.c]);
            break;
        case TEVCMP_GR16_EQ:
        case TEVCMP_BGR24_EQ:
            WRITE(p,"   %s + (abs(dot(%s.rgb - %s.rgb, comp%s))<%f ? %s : 0)\n",
                tevAInputTable[ac.d],tevAInputTable2[ac.a], tevAInputTable2[ac.b],cmp==TEVCMP_GR16_GT?"16":"24",epsilon,tevAInputTable[ac.c]);
            break;
        default:
            WRITE(p,"0)\n");
            break;
        }
    }

    WRITE(p,";\n");

    if( ac.clamp )
        WRITE(p, "%s = clamp(%s,0.0f,1.0f);\n", tevAOutputTable[ac.dest],tevAOutputTable[ac.dest]);
    WRITE(p, "\n");
}

void WrapNonPow2Tex(char* &p, const char* var, int texmap)
{
    _assert_(s_texturemask & (1<<texmap));
    bool bwraps = !!(s_texturemask & (0x100<<texmap));
    bool bwrapt = !!(s_texturemask & (0x10000<<texmap));
        
    if( bwraps || bwrapt ) {
        const char* field = bwraps ? (bwrapt ? "xy" : "x") : "y";
        const char* wrapfield = bwraps ? (bwrapt ? "zw" : "z") : "w";
        WRITE(p, "%s.%s = "I_TEXDIMS"[%d].%s*frac(%s.%s*"I_TEXDIMS"[%d].%s+32);\n", var, field, texmap, field, var, field, texmap, wrapfield);

        if( !bwraps )
            WRITE(p, "%s.x *= "I_TEXDIMS"[%d].x * "I_TEXDIMS"[%d].z;\n", var, texmap, texmap);
        if( !bwrapt )
            WRITE(p, "%s.y *= "I_TEXDIMS"[%d].y * "I_TEXDIMS"[%d].w;\n", var, texmap, texmap);
    }
    else {
        WRITE(p, "%s.xy *= "I_TEXDIMS"[%d].xy * "I_TEXDIMS"[%d].zw;\n", var, texmap, texmap);
    }
}

const char *alphaRef[2] = 
{
    I_ALPHA"[0].x",
    I_ALPHA"[0].y"
};

void WriteAlphaCompare(char *&p, int num, int comp)
{
    switch(comp) {
    case ALPHACMP_ALWAYS:  WRITE(p,"(false)");	break;
    case ALPHACMP_NEVER:   WRITE(p,"(true)");	break;
    case ALPHACMP_LEQUAL:  WRITE(p,"(prev.a > %s)",alphaRef[num]);	break;
    case ALPHACMP_LESS:    WRITE(p,"(prev.a >= %s+%f)",alphaRef[num],epsilon*0.5f);break;
    case ALPHACMP_GEQUAL:  WRITE(p,"(prev.a < %s)",alphaRef[num]);	break;
    case ALPHACMP_GREATER: WRITE(p,"(prev.a <= %s - %f)",alphaRef[num],epsilon*0.5f);break;
    case ALPHACMP_EQUAL:   WRITE(p,"(abs(prev.a-%s)>%f)",alphaRef[num],epsilon*2); break;
    case ALPHACMP_NEQUAL:  WRITE(p,"(abs(prev.a-%s)<%f)",alphaRef[num],epsilon*2); break;
    }
}

bool WriteAlphaTest(char *&p)
{
    u32 op = bpmem.alphaFunc.logic;
    u32 comp[2] = {bpmem.alphaFunc.comp0,bpmem.alphaFunc.comp1};

    //first kill all the simple cases
    switch(op) {
    case 0: // and
        if (comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_ALWAYS) return true;
        if (comp[0] == ALPHACMP_NEVER || comp[1] == ALPHACMP_NEVER) {
            WRITE(p, "discard;\n");
            return false;
        }
        break;
    case 1: // or
        if (comp[0] == ALPHACMP_ALWAYS || comp[1] == ALPHACMP_ALWAYS) return true;
        if (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_NEVER) {
            WRITE(p, "discard;\n");
            return false;
        }
        break;
    case 2: // xor
        if ( (comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_NEVER) || (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_ALWAYS) ) return true;
        if ( (comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_ALWAYS) || (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_NEVER) ) {
            WRITE(p, "discard;\n");
            return false;
        }
        break;
    case 3: // xnor
        if ( (comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_NEVER) || (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_ALWAYS) ) {
            WRITE(p, "discard;\n");
            return false;
        }
        if ( (comp[0] == ALPHACMP_ALWAYS && comp[1] == ALPHACMP_ALWAYS) || (comp[0] == ALPHACMP_NEVER && comp[1] == ALPHACMP_NEVER) )
            return true;
        break;
    }

    bool bFirst = false;
    WRITE(p, "discard( ");
    WriteAlphaCompare(p, 0, bpmem.alphaFunc.comp0);
    
    // negated because testing the inverse condition
    switch(bpmem.alphaFunc.logic) {
    case 0: WRITE(p, " || "); break; // and
    case 1: WRITE(p, " && "); break; // or
    case 2: WRITE(p, " == "); break; // xor
    case 3: WRITE(p, " != "); break; // xnor
    }
    WriteAlphaCompare(p, 1, bpmem.alphaFunc.comp1);
    WRITE(p, ");\n");
    return true;
}
