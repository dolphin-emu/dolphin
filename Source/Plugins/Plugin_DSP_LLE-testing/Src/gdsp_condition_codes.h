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

bool CheckCondition(u8 _Condition);
s8 GetMultiplyModifier();

void Update_SR_Register(s16 _Value);
void Update_SR_Register(s64 _Value);

}  // namespace

#endif  // _GDSP_CONDITION_CODES_H
