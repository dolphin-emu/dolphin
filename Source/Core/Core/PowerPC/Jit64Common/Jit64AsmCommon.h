// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/PowerPC/JitCommon/Jit_Util.h"
#include "Core/PowerPC/JitCommon/JitAsmCommon.h"

class CommonAsmRoutines : public CommonAsmRoutinesBase, public EmuCodeBlock
{
protected:
	void GenQuantizedLoads();
	void GenQuantizedStores();
	void GenQuantizedSingleStores();

public:
	void GenFifoWrite(int size);
	void GenFrsqrte();
	void GenFres();
	void GenMfcr();
};
