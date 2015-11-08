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

	enum DecodeMode
	{
		DECODE_RECORD,
		DECODE_PLAYBACK,
	};

	u32 AnalyzeCommand(u8* data, DecodeMode mode);

	struct CPMemory
	{
		TVtxDesc vtxDesc;
		VAT vtxAttr[8];
		u32 arrayBases[16];
		u32 arrayStrides[16];
	};

	void LoadCPReg(u32 subCmd, u32 value, CPMemory& cpMem);

	void CalculateVertexElementSizes(int sizes[], int vatIndex, const CPMemory& cpMem);

	extern bool s_DrawingObject;
	extern FifoAnalyzer::CPMemory s_CpMem;
}
