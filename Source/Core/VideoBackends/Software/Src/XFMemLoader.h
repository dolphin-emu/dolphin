// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _XFMEMLOADER_H_
#define _XFMEMLOADER_H_


#include "Common.h"

/////////////
// Lighting
/////////////

#define XF_TEXPROJ_ST   0
#define XF_TEXPROJ_STQ  1

#define XF_TEXINPUT_AB11 0
#define XF_TEXINPUT_ABC1 1

#define XF_TEXGEN_REGULAR       0
#define XF_TEXGEN_EMBOSS_MAP    1 // used when bump mapping
#define XF_TEXGEN_COLOR_STRGBC0 2
#define XF_TEXGEN_COLOR_STRGBC1 3

#define XF_SRCGEOM_INROW       0 // input is abc
#define XF_SRCNORMAL_INROW     1 // input is abc
#define XF_SRCCOLORS_INROW     2
#define XF_SRCBINORMAL_T_INROW 3 // input is abc
#define XF_SRCBINORMAL_B_INROW 4 // input is abc
#define XF_SRCTEX0_INROW       5
#define XF_SRCTEX1_INROW       6
#define XF_SRCTEX2_INROW       7
#define XF_SRCTEX3_INROW       8
#define XF_SRCTEX4_INROW       9
#define XF_SRCTEX5_INROW      10
#define XF_SRCTEX6_INROW      11
#define XF_SRCTEX7_INROW      12

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
#define LIGHTATTN_DIR  3

#define GX_PERSPECTIVE  0
#define GX_ORTHOGRAPHIC 1

union LitChannel
{
	struct
	{
		u32 matsource      : 1;
		u32 enablelighting : 1;
		u32 lightMask0_3   : 4;
		u32 ambsource      : 1;
		u32 diffusefunc    : 2; // LIGHTDIF_X
		u32 attnfunc       : 2; // LIGHTATTN_X
		u32 lightMask4_7   : 4;
		u32 unused         : 17;
	};
	u32 hex;
	unsigned int GetFullLightMask() const
	{
		return enablelighting ? (lightMask0_3 | (lightMask4_7 << 4)) : 0;
	}
};

union INVTXSPEC
{
	struct
	{
		u32 numcolors : 2;
		u32 numnormals : 2; // 0 - nothing, 1 - just normal, 2 - normals and binormals
		u32 numtextures : 4;
		u32 unused : 24;
	};
	u32 hex;
};

union TXFMatrixIndexA
{
	struct
	{
		u32 PosNormalMtxIdx : 6;
		u32 Tex0MtxIdx : 6;
		u32 Tex1MtxIdx : 6;
		u32 Tex2MtxIdx : 6;
		u32 Tex3MtxIdx : 6;
	};
	struct
	{
		u32 Hex : 30;
		u32 unused : 2;
	};
};

union TXFMatrixIndexB
{
	struct
	{
		u32 Tex4MtxIdx : 6;
		u32 Tex5MtxIdx : 6;
		u32 Tex6MtxIdx : 6;
		u32 Tex7MtxIdx : 6;
	};
	struct
	{
		u32 Hex : 24;
		u32 unused : 8;
	};
};

struct Viewport
{
	float wd;
	float ht;
	float zRange;
	float xOrig;
	float yOrig;
	float farZ;
};

struct Projection
{
	float rawProjection[6];
	u32 type;                      // only GX_PERSPECTIVE or GX_ORTHOGRAPHIC are allowed
};

union TexMtxInfo
{
	struct 
	{
		u32 unknown    : 1;
		u32 projection : 1; // XF_TEXPROJ_X
		u32 inputform  : 2; // XF_TEXINPUT_X
		u32 texgentype : 3; // XF_TEXGEN_X
		u32 sourcerow  : 5; // XF_SRCGEOM_X
		u32 embosssourceshift : 3; // what generated texcoord to use
		u32 embosslightshift  : 3; // light index that is used
	};
	u32 hex;
};

union PostMtxInfo
{
	struct 
	{
		u32 index : 6; // base row of dual transform matrix
		u32 unused : 2;
		u32 normalize : 1; // normalize before send operation
	};
	u32 hex;
};

struct XFRegisters
{
	u32 posMatrices[256];           // 0x0000 - 0x00ff
	u32 unk0[768];                  // 0x0100 - 0x03ff
	u32 normalMatrices[96];         // 0x0400 - 0x045f
	u32 unk1[160];                  // 0x0460 - 0x04ff
	u32 postMatrices[256];          // 0x0500 - 0x05ff
	u32 lights[128];                // 0x0600 - 0x067f
	u32 unk2[2432];                 // 0x0680 - 0x0fff
	u32 error;                      // 0x1000
	u32 diag;                       // 0x1001
	u32 state0;                     // 0x1002
	u32 state1;                     // 0x1003
	u32 xfClock;                    // 0x1004
	u32 clipDisable;                // 0x1005
	u32 perf0;                      // 0x1006
	u32 perf1;                      // 0x1007
	INVTXSPEC hostinfo;             // 0x1008 number of textures,colors,normals from vertex input
	u32 nNumChans;                  // 0x1009
	u32 ambColor[2];                // 0x100a, 0x100b
	u32 matColor[2];                // 0x100c, 0x100d
	LitChannel color[2];            // 0x100e, 0x100f
	LitChannel alpha[2];            // 0x1010, 0x1011
	u32 dualTexTrans;               // 0x1012
	u32 unk3;                       // 0x1013
	u32 unk4;                       // 0x1014
	u32 unk5;                       // 0x1015
	u32 unk6;                       // 0x1016
	u32 unk7;                       // 0x1017
	TXFMatrixIndexA MatrixIndexA;   // 0x1018
	TXFMatrixIndexB MatrixIndexB;   // 0x1019
	Viewport viewport;              // 0x101a - 0x101f
	Projection projection;          // 0x1020 - 0x1026
	u32 unk8[24];                   // 0x1027 - 0x103e
	u32 numTexGens;                 // 0x103f
	TexMtxInfo texMtxInfo[8];       // 0x1040 - 0x1047
	u32 unk9[8];                    // 0x1048 - 0x104f
	PostMtxInfo postMtxInfo[8];     // 0x1050 - 0x1057
};

#define XFMEM_POSMATRICES        0x000
#define XFMEM_POSMATRICES_END    0x100
#define XFMEM_NORMALMATRICES     0x400
#define XFMEM_NORMALMATRICES_END 0x460
#define XFMEM_POSTMATRICES       0x500
#define XFMEM_POSTMATRICES_END   0x600
#define XFMEM_LIGHTS             0x600
#define XFMEM_LIGHTS_END         0x680


extern XFRegisters swxfregs;

void InitXFMemory();

void XFWritten(u32 transferSize, u32 baseAddress);

void SWLoadXFReg(u32 transferSize, u32 baseAddress, u32 *pData);

void SWLoadIndexedXF(u32 val, int array);

#endif
