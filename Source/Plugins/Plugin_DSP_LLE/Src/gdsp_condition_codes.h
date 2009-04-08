// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Globals.h"

#include "gdsp_registers.h"

namespace DSPInterpreter {

// SR flag defines.
#define SR_CMP_MASK     0x3f   // Shouldn't this include 0x40?

// These are probably not accurate. Do not use yet.
#define SR_UNKNOWN    0x0002   // ????????
#define SR_ARITH_ZERO 0x0004
#define SR_SIGN       0x0008
#define SR_TOP2BITS   0x0020   // this is an odd one.
#define SR_LOGIC_ZERO 0x0040   // ?? duddie's doc sometimes say & 1<<6 (0x40), sometimes 1<<14 (0x4000), while we have 0x20 .. eh
#define SR_INT_ENABLE 0x0200   // Not 100% sure but duddie says so. This should replace the hack, if so.
#define SR_MUL_MODIFY 0x2000   // 1 = normal. 0 = x2

bool CheckCondition(u8 _Condition);

int GetMultiplyModifier();

void Update_SR_Register16(s16 _Value);
void Update_SR_Register64(s64 _Value);

}  // namespace

#endif  // _GDSP_CONDITION_CODES_H
