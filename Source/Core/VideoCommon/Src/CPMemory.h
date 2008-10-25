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

#ifndef _CPMEMORY_H
#define _CPMEMORY_H

#include "Common.h"

// Vertex array numbers
enum
{
	ARRAY_POSITION	= 0,
	ARRAY_NORMAL	= 1,
	ARRAY_COLOR     = 2,
	ARRAY_COLOR2    = 3,
	ARRAY_TEXCOORD0 = 4,
};

// Vertex components
enum
{
	NOT_PRESENT = 0,
	DIRECT = 1,
	INDEX8 = 2,
	INDEX16 = 3,
};

enum
{
	FORMAT_UBYTE		= 0,	// 2 Cmp
	FORMAT_BYTE			= 1,	// 3 Cmp
	FORMAT_USHORT		= 2,
	FORMAT_SHORT		= 3,
	FORMAT_FLOAT		= 4,
};

enum
{
	FORMAT_16B_565		= 0,	// NA
	FORMAT_24B_888		= 1,	
	FORMAT_32B_888x		= 2,
	FORMAT_16B_4444		= 3,
	FORMAT_24B_6666		= 4,
	FORMAT_32B_8888		= 5,
};

enum
{
	VAT_0_FRACBITS = 0x3e0001f0,
	VAT_1_FRACBITS = 0x07c3e1f0,
	VAT_2_FRACBITS = 0xf87c3e1f,
};

#pragma pack(4)
union TVtxDesc
{
	u64 Hex;
	struct 
	{
		// 0: not present
		// 1: present
		unsigned PosMatIdx		: 1;
		unsigned Tex0MatIdx 	: 1;
		unsigned Tex1MatIdx 	: 1;
		unsigned Tex2MatIdx 	: 1;
		unsigned Tex3MatIdx 	: 1;
		unsigned Tex4MatIdx 	: 1;
		unsigned Tex5MatIdx 	: 1;
		unsigned Tex6MatIdx 	: 1;
		unsigned Tex7MatIdx 	: 1;

		// 00: not present 
		// 01: direct 
		// 10: 8 bit index 
		// 11: 16 bit index
		unsigned Position		: 2;
		unsigned Normal			: 2;
		unsigned Color0 		: 2;
		unsigned Color1 		: 2;
		unsigned Tex0Coord 		: 2;
		unsigned Tex1Coord 		: 2;
		unsigned Tex2Coord 		: 2;
		unsigned Tex3Coord 		: 2;
		unsigned Tex4Coord 		: 2;
		unsigned Tex5Coord 		: 2;
		unsigned Tex6Coord 		: 2;
		unsigned Tex7Coord 		: 2;
		unsigned				:31;
	};
    struct {
        u32 Hex0, Hex1;
    };
};

union UVAT_group0
{
    u32 Hex;
    struct 
    {
        // 0:8
        unsigned PosElements	: 1;
        unsigned PosFormat		: 3; 
        unsigned PosFrac		: 5; 
        // 9:12
        unsigned NormalElements	: 1; 
        unsigned NormalFormat	: 3; 
        // 13:16
        unsigned Color0Elements : 1;
        unsigned Color0Comp		: 3; 
        // 17:20
        unsigned Color1Elements : 1;
        unsigned Color1Comp		: 3; 
        // 21:29
        unsigned Tex0CoordElements	: 1;
        unsigned Tex0CoordFormat	: 3;
        unsigned Tex0Frac			: 5;
        // 30:31
        unsigned ByteDequant: 1;
        unsigned NormalIndex3: 1;
    };
};

union UVAT_group1
{
    u32 Hex;
    struct 
    {
        // 0:8
        unsigned Tex1CoordElements	: 1;
        unsigned Tex1CoordFormat	: 3;
        unsigned Tex1Frac			: 5;
        // 9:17
        unsigned Tex2CoordElements	: 1;
        unsigned Tex2CoordFormat	: 3;
        unsigned Tex2Frac			: 5;
        // 18:26
        unsigned Tex3CoordElements	: 1;
        unsigned Tex3CoordFormat	: 3;
        unsigned Tex3Frac			: 5;
        // 27:30
        unsigned Tex4CoordElements	: 1;
        unsigned Tex4CoordFormat	: 3;
        // 
        unsigned					: 1;
    };
};

union UVAT_group2
{
    u32 Hex;
    struct 
    {
        // 0:4
        unsigned Tex4Frac			: 5;
        // 5:13
        unsigned Tex5CoordElements	: 1;
        unsigned Tex5CoordFormat	: 3;
        unsigned Tex5Frac			: 5;
        // 14:22
        unsigned Tex6CoordElements	: 1;
        unsigned Tex6CoordFormat	: 3;
        unsigned Tex6Frac			: 5;
        // 23:31
        unsigned Tex7CoordElements	: 1;
        unsigned Tex7CoordFormat	: 3;
        unsigned Tex7Frac			: 5;
    };
};

struct ColorAttr
{
    u8 Elements;
    u8 Comp;
};

struct TexAttr
{
    u8 Elements;
    u8 Format;
    u8 Frac;
};

struct TVtxAttr
{		
    u8 PosElements;
    u8 PosFormat; 
    u8 PosFrac; 
    u8 NormalElements;
    u8 NormalFormat; 
    ColorAttr color[2];
    TexAttr texCoord[8];
    u8 ByteDequant;
    u8 NormalIndex3;
};

// Matrix indices
union TMatrixIndexA
{
    struct
    {
        unsigned PosNormalMtxIdx : 6;
        unsigned Tex0MtxIdx : 6;
        unsigned Tex1MtxIdx : 6;
        unsigned Tex2MtxIdx : 6;
        unsigned Tex3MtxIdx : 6;
    };
    struct
    {
        u32 Hex : 30;
        u32 unused : 2;
    };
};

union TMatrixIndexB
{
    struct
    {
        unsigned Tex4MtxIdx : 6;
        unsigned Tex5MtxIdx : 6;
        unsigned Tex6MtxIdx : 6;
        unsigned Tex7MtxIdx : 6;
    };
    struct
    {
        u32 Hex : 24;
        u32 unused : 8;
    };
};

#pragma pack()

extern u32 arraybases[16];
extern u32 arraystrides[16];
extern TMatrixIndexA MatrixIndexA;
extern TMatrixIndexB MatrixIndexB;

#endif
