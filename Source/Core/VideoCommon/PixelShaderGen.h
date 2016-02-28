// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VideoCommon.h"

// Different ways to achieve rendering with destination alpha
enum DSTALPHA_MODE
{
	DSTALPHA_NONE, // Render normally, without destination alpha
	DSTALPHA_ALPHA_PASS, // Render normally first, then render again for alpha
	DSTALPHA_DUAL_SOURCE_BLEND // Use dual-source blending
};

#pragma pack(1)
struct pixel_shader_uid_data
{
	// TODO: Optimize field order for easy access!

	u32 num_values; // TODO: Shouldn't be a u32
	u32 NumValues() const { return num_values; }

	u32 components : 23;
	u32 dstAlphaMode : 2;
	u32 Pretest : 2;
	u32 nIndirectStagesUsed : 4;
	u32 stereo : 1;

	u32 genMode_numtexgens : 4;
	u32 genMode_numtevstages : 4;
	u32 genMode_numindstages : 3;
	u32 alpha_test_comp0 : 3;
	u32 alpha_test_comp1 : 3;
	u32 alpha_test_logic : 2;
	u32 alpha_test_use_zcomploc_hack : 1;
	u32 fog_proj : 1;
	u32 fog_fsel : 3;
	u32 fog_RangeBaseEnabled : 1;
	u32 ztex_op : 2;
	u32 fast_depth_calc : 1;
	u32 per_pixel_depth : 1;
	u32 forced_early_z : 1;
	u32 early_ztest : 1;
	u32 bounding_box : 1;

	// TODO: 29 bits of padding is a waste. Can we free up some bits elseware?
	u32 zfreeze : 1;
	u32 msaa : 1;
	u32 ssaa : 1;
	u32 pad : 29;

	u32 texMtxInfo_n_projection : 8; // 8x1 bit
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

	struct {
		// TODO: Can save a lot space by removing the padding bits
		u32 cc : 24;
		u32 ac : 24;

		u32 tevorders_texmap : 3;
		u32 tevorders_texcoord : 3;
		u32 tevorders_enable : 1;
		u32 tevorders_colorchan : 3;
		u32 pad1 : 6;

		// TODO: Clean up the swapXY mess
		u32 hasindstage : 1;
		u32 tevind : 21;
		u32 tevksel_swap1a : 2;
		u32 tevksel_swap2a : 2;
		u32 tevksel_swap1b : 2;
		u32 tevksel_swap2b : 2;
		u32 pad2 : 2;

		u32 tevksel_swap1c : 2;
		u32 tevksel_swap2c : 2;
		u32 tevksel_swap1d : 2;
		u32 tevksel_swap2d : 2;
		u32 tevksel_kc : 5;
		u32 tevksel_ka : 5;
		u32 pad3 : 14;
	} stagehash[16];

	// TODO: I think we're fine without an enablePixelLighting field, should probably double check, though..
	LightingUidData lighting;
};
#pragma pack()

typedef ShaderUid<pixel_shader_uid_data> PixelShaderUid;

ShaderCode GeneratePixelShaderCode(DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType);
PixelShaderUid GetPixelShaderUid(DSTALPHA_MODE dstAlphaMode, API_TYPE ApiType);
