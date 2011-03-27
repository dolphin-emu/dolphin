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

#ifndef _FIFOANALYZER_H
#define _FIFOANALYZER_H

#include "Common.h"

#include "BPMemory.h"
#include "CPMemory.h"

namespace FifoAnalyzer
{
	void Init();

	u8 ReadFifo8(u8 *&data);
	u16 ReadFifo16(u8 *&data);
	u32 ReadFifo32(u8 *&data);

	// TODO- move to video common
	void InitBPMemory(BPMemory *bpMem);
	BPCmd DecodeBPCmd(u32 value, const BPMemory &bpMem);
	void LoadBPReg(const BPCmd &bp, BPMemory &bpMem);
	void GetTlutLoadData(u32 &tlutAddr, u32 &memAddr, u32 &tlutXferCount, BPMemory &bpMem);

	struct CPMemory
	{
		TVtxDesc vtxDesc;
		VAT vtxAttr[8];
		u32 arrayBases[16];
		u32 arrayStrides[16];
	};

	void LoadCPReg(u32 subCmd, u32 value, CPMemory &cpMem);

	u32 CalculateVertexSize(int vatIndex, const CPMemory &cpMem);
	void CalculateVertexElementSizes(int sizes[], int vatIndex, const CPMemory &cpMem);
}

#endif