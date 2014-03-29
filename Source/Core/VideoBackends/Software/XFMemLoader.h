// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"

#include "VideoCommon/XFMemory.h"

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

struct SWXFRegisters
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

extern SWXFRegisters swxfregs;

void InitXFMemory();

void XFWritten(u32 transferSize, u32 baseAddress);

void SWLoadXFReg(u32 transferSize, u32 baseAddress, u32 *pData);

void SWLoadIndexedXF(u32 val, int array);
