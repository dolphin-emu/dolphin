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

#define I_COLORS      "color"
#define I_KCOLORS     "k"
#define I_ALPHA       "alphaRef"
#define I_TEXDIMS     "texdim"
#define I_ZBIAS       "czbias"
#define I_INDTEXSCALE "cindscale"
#define I_INDTEXMTX   "cindmtx"
#define I_FOG         "cfog"
#define I_PLIGHTS	  "cLights"
#define I_PMATERIALS   "cmtrl"

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

enum ALPHA_PRETEST_RESULT
{
	ALPHAPT_UNDEFINED, // AlphaTest Result is not defined
	ALPHAPT_ALWAYSFAIL, // Alpha test alway Fail
	ALPHAPT_ALWAYSPASS // Alpha test alway Pass
};

struct pixel_shader_uid_data
{
	u32 components;
	DSTALPHA_MODE dstAlphaMode; // TODO: as u32 :2
	ALPHA_PRETEST_RESULT Pretest; // TODO: As :2
	u32 nIndirectStagesUsed : 8;
	struct {
        u32 numtexgens : 4;
        u32 numtevstages : 4;
		u32 numindstages : 3;
	} genMode;
	u32 fogc_proj_fselfsel : 3;
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
	} tevindref;

	u32 tevorders_n_texcoord1 : 24; // 8 x 3 bit
	u32 tevorders_n_texcoord2 : 24; // 8 x 3 bit
	u32 tevorders_n_sw1 : 24; // 8 x 3 bit
	u32 tevorders_n_sw2 : 24; // 8 x 3 bit
	u32 tevorders_n_tw1 : 24; // 8 x 3 bit
	u32 tevorders_n_tw2 : 24; // 8 x 3 bit

	u32 tevind_n_bs : 32; // 16 x 2 bit
	u32 tevind_n_fmt : 32; // 16 x 2 bit
	u32 tevind_n_bt : 32; // 16 x 2 bit
	u32 tevind_n_bias1 : 24; // 8 x 3 bit
	u32 tevind_n_bias2 : 24; // 8 x 3 bit
	u32 tevind_n_mid1 : 32; // 8 x 4 bit
	u32 tevind_n_mid2 : 32; // 8 x 4 bit

	u32 tevksel_n_swap : 32; // 8 x 2 bit (swap1) + 8 x 2 bit (swap2)
	struct
	{
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
		} colorC;
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
		} alphaC;
	} combiners[16];
	struct
	{
		u32 comp0 : 3;
		u32 comp1 : 3;
		u32 logic : 2;
		// TODO: ref???
	} alpha_test;

	u32 bHasIndStage : 16;
};

typedef ShaderUid<pixel_shader_uid_data> PixelShaderUid;
typedef ShaderCode<pixel_shader_uid_data> PixelShaderCode;


void GeneratePixelShaderCode(PixelShaderCode& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components);
void GetPixelShaderId(PixelShaderUid& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components);

#endif // GCOGL_PIXELSHADER_H
