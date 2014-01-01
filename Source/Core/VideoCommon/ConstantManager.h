// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _CONSTANTMANAGER_H
#define _CONSTANTMANAGER_H

// all constant buffer attributes must be 16 bytes aligned, so this are the only allowed components:
typedef float float4[4];
typedef u32 uint4[4];
typedef s32 int4[4];

struct PixelShaderConstants
{
	float4 colors[4];
	float4 kcolors[4];
	float4 alpha;
	float4 texdims[8];
	float4 zbias[2];
	float4 indtexscale[2];
	float4 indtexmtx[6];
	float4 fog[3];

	// For pixel lighting
	float4 plights[40];
	float4 pmaterials[4];
};

struct VertexShaderConstants
{
	float4 posnormalmatrix[6];
	float4 projection[4];
	float4 materials[4];
	float4 lights[40];
	float4 texmatrices[24];
	float4 transformmatrices[64];
	float4 normalmatrices[32];
	float4 posttransformmatrices[64];
	float4 depthparams;
};

#endif