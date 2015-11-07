// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/CPMemory.h"

namespace FifoAnalyzer
{
	void Init();

	u8 ReadFifo8(u8*& data);
	u16 ReadFifo16(u8*& data);
	u32 ReadFifo32(u8*& data);

	// TODO- move to video common
	void InitBPMemory(BPMemory* bpMem);
	BPCmd DecodeBPCmd(u32 value, const BPMemory &bpMem);
	void LoadBPReg(const BPCmd& bp, BPMemory &bpMem);

	struct CPMemory
	{
		TVtxDesc vtxDesc;
		VAT vtxAttr[8];
		u32 arrayBases[16];
		u32 arrayStrides[16];
	};

	void LoadCPReg(u32 subCmd, u32 value, CPMemory& cpMem);

	u32 CalculateVertexSize(int vatIndex, const CPMemory& cpMem);
	void CalculateVertexElementSizes(int sizes[], int vatIndex, const CPMemory& cpMem);
}
