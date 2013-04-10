// Copyright (C) 2003 Dolphin Project.

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

#ifndef GCOGL_PIXELSHADER_H
#define GCOGL_PIXELSHADER_H

#include "VideoCommon.h"
#include "ShaderGenCommon.h"
#include "BPMemory.h"

#define I_COLORS      "color"
#define I_KCOLORS     "k"
#define I_ALPHA       "alphaRef"
#define I_TEXDIMS     "texdim"
#define I_ZBIAS       "czbias"
#define I_INDTEXSCALE "cindscale"
#define I_INDTEXMTX   "cindmtx"
#define I_FOG         "cfog"
#define I_PLIGHTS     "cPLights"
#define I_PMATERIALS  "cPmtrl"

#define C_COLORMATRIX	0						// 0
#define C_COLORS		0						// 0
#define C_KCOLORS		(C_COLORS + 4)			// 4
#define C_ALPHA			(C_KCOLORS + 4)			// 8
#define C_TEXDIMS		(C_ALPHA + 1)			// 9
#define C_ZBIAS			(C_TEXDIMS + 8)			//17
#define C_INDTEXSCALE	(C_ZBIAS + 2)			//19
#define C_INDTEXMTX		(C_INDTEXSCALE + 2)		//21
#define C_FOG			(C_INDTEXMTX + 6)		//27

#define C_PLIGHTS		(C_FOG + 3)
#define C_PMATERIALS	(C_PLIGHTS + 40)
#define C_PENVCONST_END (C_PMATERIALS + 4)

// Different ways to achieve rendering with destination alpha
enum DSTALPHA_MODE
{
	DSTALPHA_NONE, // Render normally, without destination alpha
	DSTALPHA_ALPHA_PASS, // Render normally first, then render again for alpha
	DSTALPHA_DUAL_SOURCE_BLEND // Use dual-source blending
};

// Annoying sure, can be removed once we get up to GLSL ~1.3
const s_svar PSVar_Loc[] = { {I_COLORS, C_COLORS, 4 },
						{I_KCOLORS, C_KCOLORS, 4 },
						{I_ALPHA, C_ALPHA, 1 },
						{I_TEXDIMS, C_TEXDIMS, 8 },
						{I_ZBIAS , C_ZBIAS, 2  },
						{I_INDTEXSCALE , C_INDTEXSCALE, 2  },
						{I_INDTEXMTX, C_INDTEXMTX, 6 },
						{I_FOG, C_FOG, 3 },
						{I_PLIGHTS, C_PLIGHTS, 40 },
						{I_PMATERIALS, C_PMATERIALS, 4 },
						};

struct pixel_shader_uid_data
{
	u32 components;
	DSTALPHA_MODE dstAlphaMode; // TODO: as u32 :2
	AlphaTest::TEST_RESULT Pretest; // TODO: As :2
	u32 nIndirectStagesUsed : 8;
	struct {
        u32 numtexgens : 4;
        u32 numtevstages : 4;
		u32 numindstages : 3;
	} genMode;
	struct
	{
		u32 unknown : 1;
		u32 projection : 1; // XF_TEXPROJ_X
		u32 inputform : 2; // XF_TEXINPUT_X
		u32 texgentype : 3; // XF_TEXGEN_X
		u32 sourcerow : 5; // XF_SRCGEOM_X
		u32 embosssourceshift : 3; // what generated texcoord to use
		u32 embosslightshift : 3; // light index that is used
	} texMtxInfo[8];
	struct
	{
        u32 bi0 : 3; // indirect tex stage 0 ntexmap
        u32 bc0 : 3; // indirect tex stage 0 ntexcoord
        u32 bi1 : 3;
        u32 bc1 : 3;
        u32 bi2 : 3;
        u32 bc3 : 3;
        u32 bi4 : 3;
        u32 bc4 : 3;
		void SetValues(int index, u32 texcoord, u32 texmap)
		{
			if (index == 0) { bc0 = texcoord; bi0 = texmap; }
			else if (index == 1) { bc1 = texcoord; bi1 = texmap; }
			else if (index == 2) { bc3 = texcoord; bi2 = texmap; }
			else if (index == 3) { bc4 = texcoord; bi4 = texmap; }
		}
	} tevindref;

	u32 tevorders_n_texcoord1 : 24; // 8 x 3 bit
	u32 tevorders_n_texcoord2 : 24; // 8 x 3 bit
	struct
	{
		u32 sw1 : 24;        // 8 x 3 bit
		u32 sw2 : 24;        // 8 x 3 bit
		u32 tw1 : 24;        // 8 x 3 bit
		u32 tw2 : 24;        // 8 x 3 bit
		u32 fb_addprev : 16; // 16 x 1 bit
		u32 bs : 32;         // 16 x 2 bit
		u32 fmt : 32;        // 16 x 2 bit
		u32 bt : 32;         // 16 x 2 bit
		u32 bias1 : 24;      // 8 x 3 bit
		u32 bias2 : 24;      // 8 x 3 bit
		u32 mid1 : 32;       // 8 x 4 bit
		u32 mid2 : 32;       // 8 x 4 bit

		// NOTE: These assume that the affected bits are zero before calling
		void Set_sw(int index, u32 val)
		{
			if (index < 8) sw1 |= val << (3*index);
			else sw2 |= val << (3*index - 24);
		}
		void Set_tw(int index, u32 val)
		{
			if (index < 8) tw1 |= val << (3*index);
			else tw2 |= val << (3*index - 24);
		}
		void Set_bias(int index, u32 val)
		{
			if (index < 8) bias1 |= val << (3*index);
			else bias2 |= val << (3*index - 24);
		}
		void Set_mid(int index, u32 val)
		{
			if (index < 8) mid1 |= val << (4*index);
			else mid2 |= val << (4*index - 32);
		}
	} tevind_n;

	u32 tevksel_n_swap : 32; // 8 x 2 bit (swap1) + 8 x 2 bit (swap2)
	struct
	{
		union {
			struct  //abc=8bit,d=10bit
			{
				u32 d : 4; // TEVSELCC_X
				u32 c : 4; // TEVSELCC_X
				u32 b : 4; // TEVSELCC_X
				u32 a : 4; // TEVSELCC_X

				u32 bias : 2;
				u32 op : 1;
				u32 clamp : 1;

				u32 shift : 2;
				u32 dest : 2;  //1,2,3
			};
			u32 hex : 24;
		} colorC;
		union {
			struct
			{
				u32 rswap : 2;
				u32 tswap : 2;
				u32 d : 3; // TEVSELCA_
				u32 c : 3; // TEVSELCA_
				u32 b : 3; // TEVSELCA_
				u32 a : 3; // TEVSELCA_

				u32 bias : 2; //GXTevBias
				u32 op : 1;
				u32 clamp : 1;

				u32 shift : 2;
				u32 dest : 2;  //1,2,3
			};
			u32 hex : 24;
		} alphaC;
	} combiners[16];
	struct
	{
		u32 comp0 : 3;
		u32 comp1 : 3;
		u32 logic : 2;
		// TODO: ref???
		u32 use_zcomploc_hack : 1;
	} alpha_test;

	union {
		struct
		{
			u32 proj : 1; // 0 - perspective, 1 - orthographic
			u32 fsel : 3; // 0 - off, 2 - linear, 4 - exp, 5 - exp2, 6 - backward exp, 7 - backward exp2
			u32 RangeBaseEnabled : 1;
		};
		u32 hex : 4;
	} fog;

	union {
		struct {
			u32 op : 2;
		};
		u32 hex : 2;
	} ztex;

	u32 per_pixel_depth : 1;
	u32 bHasIndStage : 16;

	u32 xfregs_numTexGen_numTexGens : 4;

	LightingUidData lighting;
};

typedef ShaderUid<pixel_shader_uid_data> PixelShaderUid;
typedef ShaderCode PixelShaderCode; // TODO: Obsolete
typedef ShaderConstantProfile PixelShaderConstantProfile; // TODO: Obsolete

void GeneratePixelShaderCode(PixelShaderCode& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components);
void GetPixelShaderUid(PixelShaderUid& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components);
void GetPixelShaderConstantProfile(PixelShaderConstantProfile& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components);

#endif // GCOGL_PIXELSHADER_H
