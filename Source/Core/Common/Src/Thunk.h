// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _THUNK_H_
#define _THUNK_H_

#include <map>

#include "Common.h"
#include "x64Emitter.h"

// This simple class creates a wrapper around a C/C++ function that saves all fp state
// before entering it, and restores it upon exit. This is required to be able to selectively
// call functions from generated code, without inflicting the performance hit and increase
// of complexity that it means to protect the generated code from this problem.

// This process is called thunking.

// There will only ever be one level of thunking on the stack, plus,
// we don't want to pollute the stack, so we store away regs somewhere global.
// NOT THREAD SAFE. This may only be used from the CPU thread.
// Any other thread using this stuff will be FATAL.

class ThunkManager : public Gen::XCodeBlock
{
	std::map<void *, const u8 *> thunks;

	const u8 *save_regs;
	const u8 *load_regs;

public:
	ThunkManager() {
		Init();
	}
	~ThunkManager() {
		Shutdown();
	}
	void *ProtectFunction(void *function, int num_params);
private:
	void Init();
	void Shutdown();
	void Reset();
};

#endif // _THUNK_H_
