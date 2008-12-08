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

#ifndef _XFMEMORY_H
#define _XFMEMORY_H

#include "Common.h"

/////////////
// Lighting
/////////////

#define XF_TEXPROJ_ST   0
#define XF_TEXPROJ_STQ  1

#define XF_TEXINPUT_AB11 0
#define XF_TEXINPUT_ABC1 1

#define XF_TEXGEN_REGULAR 0
#define XF_TEXGEN_EMBOSS_MAP 1 // used when bump mapping
#define XF_TEXGEN_COLOR_STRGBC0 2
#define XF_TEXGEN_COLOR_STRGBC1 3

#define XF_SRCGEOM_INROW 0 // input is abc
#define XF_SRCNORMAL_INROW 1 // input is abc
#define XF_SRCCOLORS_INROW 2
#define XF_SRCBINORMAL_T_INROW 3 // input is abc
#define XF_SRCBINORMAL_B_INROW 4 // input is abc
#define XF_SRCTEX0_INROW   5
#define XF_SRCTEX1_INROW   6
#define XF_SRCTEX2_INROW   7
#define XF_SRCTEX3_INROW   8
#define XF_SRCTEX4_INROW   9
#define XF_SRCTEX5_INROW   10
#define XF_SRCTEX6_INROW   11
#define XF_SRCTEX7_INROW   12

#define GX_SRC_REG 0
#define GX_SRC_VTX 1

struct Light
{
    u32 useless[3]; 
    u32 color;    //rgba
    float a0;  //attenuation
    float a1; 
    float a2; 
    float k0;  //k stuff
    float k1; 
    float k2; 
    union
    {
        struct {
			float dpos[3];
            float ddir[3]; // specular lights only
        };
        struct {
            float sdir[3];
            float shalfangle[3]; // specular lights only
        };
    };
};

#define LIGHTDIF_NONE  0
#define LIGHTDIF_SIGN  1
#define LIGHTDIF_CLAMP 2

#define LIGHTATTN_SPEC 0 // specular attenuation
#define LIGHTATTN_SPOT 1 // distance/spotlight attenuation
#define LIGHTATTN_NONE 2
#define LIGHTATTN_DIR 3

union LitChannel
{
    struct
    {
        unsigned matsource      : 1;
        unsigned enablelighting : 1;
        unsigned lightMask0_3   : 4;
        unsigned ambsource      : 1;
        unsigned diffusefunc    : 2; // LIGHTDIF_X
        unsigned attnfunc       : 2; // LIGHTATTN_X
        unsigned lightMask4_7   : 4;
    };
    struct
    {
        u32 hex : 15;
        u32 unused : 17;
    };
    struct
    {
        u32 dummy0 : 7;
        u32 lightparams : 4;
        u32 dummy1 : 21;
    };
    unsigned int GetFullLightMask() const
    {
        return enablelighting ? (lightMask0_3 | (lightMask4_7 << 4)) : 0;
    }
};

struct ColorChannel
{
    u32 ambColor;
    u32 matColor;
    LitChannel color;
    LitChannel alpha;
};

union INVTXSPEC
{
    struct
    {
        unsigned numcolors : 2;
        unsigned numnormals : 2; // 0 - nothing, 1 - just normal, 2 - normals and binormals
        unsigned numtextures : 4;
        unsigned unused : 24;
    };
    u32 hex;
};

union TexMtxInfo
{
    struct 
    {
        unsigned unknown : 1;
        unsigned projection : 1; // XF_TEXPROJ_X
        unsigned inputform : 2; // XF_TEXINPUT_X
        unsigned texgentype : 3; // XF_TEXGEN_X
        unsigned sourcerow : 5; // XF_SRCGEOM_X
        unsigned embosssourceshift : 3; // what generated texcoord to use
        unsigned embosslightshift : 3; // light index that is used
    };
    u32 hex;
};

union PostMtxInfo
{
    struct 
    {
        unsigned index : 6; // base row of dual transform matrix
        unsigned unused : 2;
        unsigned normalize : 1; // normalize before send operation
    };
    u32 hex;
};

struct TexCoordInfo
{
    TexMtxInfo texmtxinfo;
    PostMtxInfo postmtxinfo;
};

struct XFRegisters
{
    int numTexGens;
    int nNumChans;
    INVTXSPEC hostinfo; // number of textures,colors,normals from vertex input
    ColorChannel colChans[2]; //C0A0 C1A1
    TexCoordInfo texcoords[8];
    bool bEnableDualTexTransform;
	float rawViewport[6];
	float rawProjection[7];
};

#define XFMEM_SIZE               0x8000
#define XFMEM_POSMATRICES        0x000
#define XFMEM_POSMATRICES_END    0x100
#define XFMEM_NORMALMATRICES     0x400
#define XFMEM_NORMALMATRICES_END 0x460
#define XFMEM_POSTMATRICES       0x500
#define XFMEM_POSTMATRICES_END   0x600
#define XFMEM_LIGHTS             0x600
#define XFMEM_LIGHTS_END         0x680

struct Viewport
{
	float wd;
	float ht;
	float nearZ;
	float xOrig;
	float yOrig;
	float farZ;
};

extern XFRegisters xfregs;
extern u32 xfmem[XFMEM_SIZE];

#endif

