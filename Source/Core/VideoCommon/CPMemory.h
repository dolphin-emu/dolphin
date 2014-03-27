// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"
#include "Common/Common.h"

// Vertex array numbers
enum
{
	ARRAY_POSITION  = 0,
	ARRAY_NORMAL    = 1,
	ARRAY_COLOR     = 2,
	ARRAY_COLOR2    = 3,
	ARRAY_TEXCOORD0 = 4,
};

// Vertex components
enum
{
	FORMAT_UBYTE   = 0, // 2 Cmp
	FORMAT_BYTE    = 1, // 3 Cmp
	FORMAT_USHORT  = 2,
	FORMAT_SHORT   = 3,
	FORMAT_FLOAT   = 4,
};

enum
{
	FORMAT_16B_565  = 0, // NA
	FORMAT_24B_888  = 1,
	FORMAT_32B_888x = 2,
	FORMAT_16B_4444 = 3,
	FORMAT_24B_6666 = 4,
	FORMAT_32B_8888 = 5,
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

	enum VertexComponentType : u64
	{
		NOT_PRESENT = 0,
		DIRECT      = 1,
		INDEX8      = 2,
		INDEX16     = 3,
	};

	// 0: not present
	// 1: present
	BitField< 0,1,u64> PosMatIdx;
	BitField< 1,1,u64> Tex0MatIdx;
	BitField< 2,1,u64> Tex1MatIdx;
	BitField< 3,1,u64> Tex2MatIdx;
	BitField< 4,1,u64> Tex3MatIdx;
	BitField< 5,1,u64> Tex4MatIdx;
	BitField< 6,1,u64> Tex5MatIdx;
	BitField< 7,1,u64> Tex6MatIdx;
	BitField< 8,1,u64> Tex7MatIdx;

	// 00: not present
	// 01: direct
	// 10: 8 bit index
	// 11: 16 bit index
	BitField< 9,2,VertexComponentType> Position;
	BitField<11,2,VertexComponentType> Normal;
	BitField<13,2,VertexComponentType> Color0;
	BitField<15,2,VertexComponentType> Color1;
	BitField<17,2,VertexComponentType> Tex0Coord;
	BitField<19,2,VertexComponentType> Tex1Coord;
	BitField<21,2,VertexComponentType> Tex2Coord;
	BitField<23,2,VertexComponentType> Tex3Coord;
	BitField<25,2,VertexComponentType> Tex4Coord;
	BitField<27,2,VertexComponentType> Tex5Coord;
	BitField<29,2,VertexComponentType> Tex6Coord;
	BitField<31,2,VertexComponentType> Tex7Coord;

	// 31 unused bits

	BitField< 0,32,u64> Hex0;
	BitField<32,32,u64> Hex1;
};

union UVAT_group0
{
	u32 Hex;

	BitField< 0,1,u32> PosElements;
	BitField< 1,3,u32> PosFormat;
	BitField< 4,5,u32> PosFrac;

	BitField< 9,1,u32> NormalElements;
	BitField<10,3,u32> NormalFormat;

	BitField<13,1,u32> Color0Elements;
	BitField<14,3,u32> Color0Comp;

	BitField<17,1,u32> Color1Elements;
	BitField<18,3,u32> Color1Comp;

	BitField<21,1,u32> Tex0CoordElements;
	BitField<22,3,u32> Tex0CoordFormat;
	BitField<25,5,u32> Tex0Frac;

	BitField<30,1,u32> ByteDequant;
	BitField<31,1,u32> NormalIndex3;
};

union UVAT_group1
{
	u32 Hex;

	BitField< 0,1,u32> Tex1CoordElements;
	BitField< 1,3,u32> Tex1CoordFormat;
	BitField< 4,5,u32> Tex1Frac;

	BitField< 9,1,u32> Tex2CoordElements;
	BitField<10,3,u32> Tex2CoordFormat;
	BitField<13,5,u32> Tex2Frac;

	BitField<18,1,u32> Tex3CoordElements;
	BitField<19,3,u32> Tex3CoordFormat;
	BitField<22,5,u32> Tex3Frac;

	BitField<27,1,u32> Tex4CoordElements;
	BitField<28,3,u32> Tex4CoordFormat;

	// 1 unused bit
};

union UVAT_group2
{
	u32 Hex;

	BitField<0,5,u32> Tex4Frac;

	BitField<5,1,u32> Tex5CoordElements;
	BitField<6,3,u32> Tex5CoordFormat;
	BitField<9,5,u32> Tex5Frac;

	BitField<14,1,u32> Tex6CoordElements;
	BitField<15,3,u32> Tex6CoordFormat;
	BitField<18,5,u32> Tex6Frac;

	BitField<23,1,u32> Tex7CoordElements;
	BitField<24,3,u32> Tex7CoordFormat;
	BitField<27,5,u32> Tex7Frac;
};

// Matrix indices
union TMatrixIndexA
{
	BitField< 0,6,u32> PosNormalMtxIdx;
	BitField< 6,6,u32> Tex0MtxIdx;
	BitField<12,6,u32> Tex1MtxIdx;
	BitField<18,6,u32> Tex2MtxIdx;
	BitField<24,6,u32> Tex3MtxIdx;

	BitField<0,30,u32> Hex; // TODO: Ugly
};

union TMatrixIndexB
{
	BitField< 0,6,u32> Tex4MtxIdx;
	BitField< 6,6,u32> Tex5MtxIdx;
	BitField<12,6,u32> Tex6MtxIdx;
	BitField<18,6,u32> Tex7MtxIdx;

	BitField<0,24,u32> Hex; // TODO: Ugly
};

#pragma pack()

extern u32 arraybases[16];
extern u8 *cached_arraybases[16];
extern u32 arraystrides[16];
extern TMatrixIndexA MatrixIndexA;
extern TMatrixIndexB MatrixIndexB;

struct VAT
{
	UVAT_group0 g0;
	UVAT_group1 g1;
	UVAT_group2 g2;
};

extern TVtxDesc g_VtxDesc;
extern VAT g_VtxAttr[8];

// Might move this into its own file later.
void LoadCPReg(u32 SubCmd, u32 Value);

// Fills memory with data from CP regs
void FillCPMemoryArray(u32 *memory);
