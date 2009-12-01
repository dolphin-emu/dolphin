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

// Additional copyrights go to Duddie and Tratax (c) 2004

#ifndef _GDSP_CONDITION_CODES_H
#define _GDSP_CONDITION_CODES_H

// Anything to do with SR and conditions goes here.

#include "Common.h"

namespace DSPInterpreter {

bool CheckCondition(u8 _Condition);

int GetMultiplyModifier();

void Update_SR_Register16(s16 _Value, bool carry = false, bool overflow = false);
void Update_SR_Register64(s64 _Value, bool carry = false, bool overflow = false);
void Update_SR_LZ(bool value);

inline bool isAddCarry(u64 val, u64 result) {
	return (val > result);
}

inline bool isSubCarry(u64 val, u64 result) {
	return (val < result);
}

inline bool isOverflow(s64 val1, s64 val2, s64 res) {
	return ((val1 ^ res) & (val2 ^ res)) < 0;
}

}  // namespace

#endif  // _GDSP_CONDITION_CODES_H
