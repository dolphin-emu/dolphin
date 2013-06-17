// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

// TODO: Should compact packing be enabled?
//#pragma pack(4)
struct pixel_shader_uid_data
{
	u32 components;
	u32 dstAlphaMode : 2;
	u32 Pretest : 2;

	u32 genMode_numtexgens : 4;
	u32 genMode_numtevstages : 4;
	u32 genMode_numindstages : 3;

	u32 nIndirectStagesUsed : 8;

	u32 texMtxInfo_n_unknown : 8; // 8x1 bit
	u32 texMtxInfo_n_projection : 8; // 8x1 bit
	u32 texMtxInfo_n_inputform : 16; // 8x2 bits
	u32 texMtxInfo_n_texgentype : 24; // 8x3 bits
	u64 texMtxInfo_n_sourcerow : 40; // 8x5 bits
	u32 texMtxInfo_n_embosssourceshift : 24; // 8x3 bits
	u32 texMtxInfo_n_embosslightshift : 24; // 8x3 bits

	u32 tevindref_bi0 : 3;
	u32 tevindref_bc0 : 3;
	u32 tevindref_bi1 : 3;
	u32 tevindref_bc1 : 3;
	u32 tevindref_bi2 : 3;
	u32 tevindref_bc3 : 3;
	u32 tevindref_bi4 : 3;
	u32 tevindref_bc4 : 3;
	inline void SetTevindrefValues(int index, u32 texcoord, u32 texmap)
	{
		if (index == 0) { tevindref_bc0 = texcoord; tevindref_bi0 = texmap; }
		else if (index == 1) { tevindref_bc1 = texcoord; tevindref_bi1 = texmap; }
		else if (index == 2) { tevindref_bc3 = texcoord; tevindref_bi2 = texmap; }
		else if (index == 3) { tevindref_bc4 = texcoord; tevindref_bi4 = texmap; }
	}
	inline void SetTevindrefTexmap(int index, u32 texmap)
	{
		if (index == 0) { tevindref_bi0 = texmap; }
		else if (index == 1) { tevindref_bi1 = texmap; }
		else if (index == 2) { tevindref_bi2 = texmap; }
		else if (index == 3) { tevindref_bi4 = texmap; }
	}

	u64 tevorders_n_texcoord : 48; // 16 x 3 bits

	u64 tevind_n_sw : 48;         // 16 x 3 bits
	u64 tevind_n_tw : 48;         // 16 x 3 bits
	u32 tevind_n_fb_addprev : 16; // 16 x 1 bit
	u32 tevind_n_bs : 32;         // 16 x 2 bits
	u32 tevind_n_fmt : 32;        // 16 x 2 bits
	u32 tevind_n_bt : 32;         // 16 x 2 bits
	u64 tevind_n_bias : 48;       // 16 x 3 bits
	u64 tevind_n_mid : 64;        // 16 x 4 bits

	// NOTE: These assume that the affected bits are zero before calling
	void Set_tevind_sw(int index, u64 val)
	{
		tevind_n_sw |= val << (3*index);
	}
	void Set_tevind_tw(int index, u64 val)
	{
		tevind_n_tw |= val << (3*index);
	}
	void Set_tevind_bias(int index, u64 val)
	{
		tevind_n_bias |= val << (3*index);
	}
	void Set_tevind_mid(int index, u64 val)
	{
		tevind_n_mid |= val << (4*index);
	}

	u32 tevksel_n_swap1 : 16; // 8x2 bits
	u32 tevksel_n_swap2 : 16; // 8x2 bits
	u64 tevksel_n_kcsel0 : 40; // 8x5 bits
	u64 tevksel_n_kasel0 : 40; // 8x5 bits
	u64 tevksel_n_kcsel1 : 40; // 8x5 bits
	u64 tevksel_n_kasel1 : 40; // 8x5 bits
	void set_tevksel_kcsel(int index, int i, u32 value) { if (i) tevksel_n_kcsel1 |= value << (5*index); else tevksel_n_kcsel0 |= value << (5*index); }
	void set_tevksel_kasel(int index, int i, u32 value) { if( i) tevksel_n_kasel1 |= value << (5*index); else tevksel_n_kasel0 |= value << (5*index); }

	u64 cc_n_d : 64; // 16x4 bits
	u64 cc_n_c : 64; // 16x4 bits
	u64 cc_n_b : 64; // 16x4 bits
	u64 cc_n_a : 64; // 16x4 bits
	u32 cc_n_bias : 32; // 16x2 bits
	u32 cc_n_op : 16; // 16x1 bit
	u32 cc_n_clamp : 16; // 16x1 bit
	u32 cc_n_shift : 32; // 16x2 bits
	u32 cc_n_dest : 32; // 16x2 bits

	u32 ac_n_rswap : 32; // 16x2 bits
	u32 ac_n_tswap : 32; // 16x2 bits
	u64 ac_n_d : 48; // 16x3 bits
	u64 ac_n_c : 48; // 16x3 bits
	u64 ac_n_b : 48; // 16x3 bits
	u64 ac_n_a : 48; // 16x3 bits
	u32 ac_n_bias : 32; // 16x2 bits
	u32 ac_n_op : 16; // 16x1 bit
	u32 ac_n_clamp : 16; // 16x1 bit
	u32 ac_n_shift : 32; // 16x2 bits
	u32 ac_n_dest : 32; // 16x2 bits

	u32 alpha_test_comp0 : 3;
	u32 alpha_test_comp1 : 3;
	u32 alpha_test_logic : 2;
	u32 alpha_test_use_zcomploc_hack : 1;

	u32 fog_proj : 1;
	u32 fog_fsel : 3;
	u32 fog_RangeBaseEnabled : 1;

	u32 ztex_op : 2;

	u32 per_pixel_depth : 1;
	u32 bHasIndStage : 16;

	u32 xfregs_numTexGen_numTexGens : 4;

	LightingUidData lighting;
};
//#pragma pack()

typedef ShaderUid<pixel_shader_uid_data> PixelShaderUid;
typedef ShaderCode PixelShaderCode; // TODO: Obsolete
typedef ShaderConstantProfile PixelShaderConstantProfile; // TODO: Obsolete

void GeneratePixelShaderCode(PixelShaderCode& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components);
void GetPixelShaderUid(PixelShaderUid& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components);
void GetPixelShaderConstantProfile(PixelShaderConstantProfile& object, DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType, u32 components);

#endif // GCOGL_PIXELSHADER_H
