// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

// all constant buffer attributes must be 16 bytes aligned, so this are the only allowed components:
typedef float float4[4];
typedef u32 uint4[4];
typedef s32 int4[4];

struct PixelShaderConstants
{
	int4 colors[4];
	int4 kcolors[4];
	int4 alpha;
	float4 texdims[8];
	int4 zbias[2];
	int4 indtexscale[2];
	int4 indtexmtx[6];
	int4 fogcolor;
	int4 fogi;
	float4 fogf[2];
};

struct VertexShaderConstants
{
	float4 posnormalmatrix[6];
	float4 projection[4];
	int4 materials[4];
	struct Light
	{
		int4 color;
		float4 cosatt;
		float4 distatt;
		float4 pos;
		float4 dir;
	} lights [8];
	float4 texmatrices[24];
	float4 transformmatrices[64];
	float4 normalmatrices[32];
	float4 posttransformmatrices[64];
	float4 depthparams;
	float4 stereoparams;
};

