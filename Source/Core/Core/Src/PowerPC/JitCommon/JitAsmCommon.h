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

#ifndef _JITASMCOMMON_H
#define _JITASMCOMMON_H

#include "../JitCommon/Jit_Util.h"

class CommonAsmRoutines : public EmuCodeBlock {
protected:
	void GenQuantizedLoads();
	void GenQuantizedStores();
public:
	void GenFifoWrite(int size);
	void GenFifoXmm64Write();
	void GenFifoFloatWrite();

	// In: array index: GQR to use.
	// In: ECX: Address to read from.
	// Out: XMM0: Bottom two 32-bit slots hold the read value,
	//            converted to a pair of floats.
	// Trashes: EAX ECX EDX
	const u8 GC_ALIGNED16(*pairedLoadQuantized[8]);

	// In: array index: GQR to use.
	// In: ECX: Address to write to.
	// In: XMM0: Bottom two 32-bit slots hold the pair of floats to be written.
	// Out: Nothing.
	// Trashes: EAX ECX EDX
	const u8 GC_ALIGNED16(*pairedStoreQuantized[8]);
};

#endif
