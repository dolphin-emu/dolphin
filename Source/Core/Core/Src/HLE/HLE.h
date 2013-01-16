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

#ifndef _HLE_H
#define _HLE_H

#include <map>
#include "Common.h"

namespace HLE
{
	enum
	{
		HLE_HOOK_START = 0,   // Hook the beginning of the function and execute the function afterwards
		HLE_HOOK_END = 1,     // Hook the end of the function, executing the function first before the hook
		HLE_HOOK_REPLACE = 2, // Replace the function with the HLE version
		HLE_HOOK_NONE = 3,    // Do not hook the function
	};

	enum
	{
		HLE_TYPE_GENERIC = 0, // Miscellaneous function
		HLE_TYPE_MEMORY = 1,  // Memory operation
		HLE_TYPE_FP = 2,      // Floating Point operation
		HLE_TYPE_DEBUG = 3,   // Debug output function
	};

	void PatchFunctions();

	void Patch(u32 pc, const char *func_name);
	u32 UnPatch(std::string patchName);
	void Execute(u32 _CurrentPC, u32 _Instruction);

	u32 GetFunctionIndex(u32 em_address);
	int GetFunctionTypeByIndex(u32 index);
	int GetFunctionFlagsByIndex(u32 index);

	bool IsEnabled(int flags);

	static std::map<u32, u32> orig_instruction;
}

#endif

