// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"


// Lighting


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
#define XF_SRCTEX5_INROW       10
#define XF_SRCTEX6_INROW       11
#define XF_SRCTEX7_INROW       12

#define GX_SRC_REG 0
#define GX_SRC_VTX 1

#define LIGHTDIF_NONE  0
#define LIGHTDIF_SIGN  1
#define LIGHTDIF_CLAMP 2

#define LIGHTATTN_SPEC 0 // specular attenuation
#define LIGHTATTN_SPOT 1 // distance/spotlight attenuation
#define LIGHTATTN_NONE 2
#define LIGHTATTN_DIR  3

#define GX_PERSPECTIVE  0
#define GX_ORTHOGRAPHIC 1

#define XFMEM_POSMATRICES        0x000
#define XFMEM_POSMATRICES_END    0x100
#define XFMEM_NORMALMATRICES     0x400
#define XFMEM_NORMALMATRICES_END 0x460
#define XFMEM_POSTMATRICES       0x500
#define XFMEM_POSTMATRICES_END   0x600
#define XFMEM_LIGHTS             0x600
#define XFMEM_LIGHTS_END         0x680
#define XFMEM_ERROR              0x1000
#define XFMEM_DIAG               0x1001
#define XFMEM_STATE0             0x1002
#define XFMEM_STATE1             0x1003
#define XFMEM_CLOCK              0x1004
#define XFMEM_CLIPDISABLE        0x1005
#define XFMEM_SETGPMETRIC        0x1006
#define XFMEM_VTXSPECS           0x1008
#define XFMEM_SETNUMCHAN         0x1009
#define XFMEM_SETCHAN0_AMBCOLOR  0x100a
#define XFMEM_SETCHAN1_AMBCOLOR  0x100b
#define XFMEM_SETCHAN0_MATCOLOR  0x100c
#define XFMEM_SETCHAN1_MATCOLOR  0x100d
#define XFMEM_SETCHAN0_COLOR     0x100e
#define XFMEM_SETCHAN1_COLOR     0x100f
#define XFMEM_SETCHAN0_ALPHA     0x1010
#define XFMEM_SETCHAN1_ALPHA     0x1011
#define XFMEM_DUALTEX            0x1012
#define XFMEM_SETMATRIXINDA      0x1018
#define XFMEM_SETMATRIXINDB      0x1019
#define XFMEM_SETVIEWPORT        0x101a
#define XFMEM_SETZSCALE          0x101c
#define XFMEM_SETZOFFSET         0x101f
#define XFMEM_SETPROJECTION      0x1020
/*#define XFMEM_SETPROJECTIONB     0x1021
#define XFMEM_SETPROJECTIONC     0x1022
#define XFMEM_SETPROJECTIOND     0x1023
#define XFMEM_SETPROJECTIONE     0x1024
#define XFMEM_SETPROJECTIONF     0x1025
#define XFMEM_SETPROJECTIONORTHO1 0x1026
#define XFMEM_SETPROJECTIONORTHO2 0x1027*/
#define XFMEM_SETNUMTEXGENS      0x103f
#define XFMEM_SETTEXMTXINFO      0x1040
#define XFMEM_SETPOSMTXINFO      0x1050

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


union INVTXSPEC
{
	struct
	{
		u32 numcolors   : 2;
		u32 numnormals  : 2; // 0 - nothing, 1 - just normal, 2 - normals and binormals
		u32 numtextures : 4;
		u32 unused : 24;
	};
	u32 hex;
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
		u32 index     : 6; // base row of dual transform matrix
		u32 unused    : 2;
		u32 normalize : 1; // normalize before send operation
	};
	u32 hex;
};

union NumColorChannel
{
	struct
	{
		u32 numColorChans : 2;
	};
	u32 hex;
};

union NumTexGen
{
	struct
	{
		u32 numTexGens : 4;
	};
	u32 hex;
};

union DualTexInfo
{
	struct
	{
		u32 enabled : 1;
	};
	u32 hex;
};



struct Light
{
	u32 useless[3];
	u8 color[4];
	float cosatt[3]; // cos attenuation
	float distatt[3]; // dist attenuation

	union
	{
		struct
		{
			float dpos[3];
			float ddir[3]; // specular lights only
		};

		struct
		{
			float sdir[3];
			float shalfangle[3]; // specular lights only
		};
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

struct XFMemory
{
	u32 posMatrices[256];           // 0x0000 - 0x00ff
	u32 unk0[768];                  // 0x0100 - 0x03ff
	u32 normalMatrices[96];         // 0x0400 - 0x045f
	u32 unk1[160];                  // 0x0460 - 0x04ff
	u32 postMatrices[256];          // 0x0500 - 0x05ff
	Light lights[8];                // 0x0600 - 0x067f
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
	NumColorChannel numChan;        // 0x1009
	u32 ambColor[2];                // 0x100a, 0x100b
	u32 matColor[2];                // 0x100c, 0x100d
	LitChannel color[2];            // 0x100e, 0x100f
	LitChannel alpha[2];            // 0x1010, 0x1011
	DualTexInfo dualTexTrans;       // 0x1012
	u32 unk3;                       // 0x1013
	u32 unk4;                       // 0x1014
	u32 unk5;                       // 0x1015
	u32 unk6;                       // 0x1016
	u32 unk7;                       // 0x1017
	TMatrixIndexA MatrixIndexA;     // 0x1018
	TMatrixIndexB MatrixIndexB;     // 0x1019
	Viewport viewport;              // 0x101a - 0x101f
	Projection projection;          // 0x1020 - 0x1026
	u32 unk8[24];                   // 0x1027 - 0x103e
	NumTexGen numTexGen;            // 0x103f
	TexMtxInfo texMtxInfo[8];       // 0x1040 - 0x1047
	u32 unk9[8];                    // 0x1048 - 0x104f
	PostMtxInfo postMtxInfo[8];     // 0x1050 - 0x1057
};


extern XFMemory xfmem;

void LoadXFReg(u32 transferSize, u32 address);
void LoadIndexedXF(u32 val, int array);
void PreprocessIndexedXF(u32 val, int refarray);
