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

#ifndef _ACTIONREPLAY_H_
#define _ACTIONREPLAY_H_

#include "IniFile.h"

enum
{
//Start OF Zero Codes
	AR_ZCODE_END      = 0x00,
	AR_ZCODE_NORM     = 0x02, 
	AR_ZCODE_ROW	  = 0x03, 
	AR_ZCODE_MEM_COPY = 0x04, 
//Start of normal Codes
	AR_CODE_IF_EQUAL     = 0x1, 
	AR_CODE_IF_NOT_EQUAL = 0x2, 
	AR_CODE_IF_LESS_THAN_SIGNED      = 0x3,
	AR_CODE_IF_GREATER_THAN_SIGNED	 = 0x4,
	AR_CODE_IF_LESS_THAN_UNSIGNED	 = 0x5,
	AR_CODE_IF_GREATER_THAN_UNSIGNED = 0x6,
	AR_CODE_IF_AND	 = 0x7,
//Add Stuff
	AR_BYTE_ADD	 = 0x0,
	AR_SHORT_ADD = 0x1, 
	AR_DWORD_ADD = 0x2, 
	AR_FLOAT_ADD = 0x3,
//Write Stuff
	AR_BYTE_WRITE  = 0x00,
	AR_SHORT_WRITE = 0x01,
	AR_DWORD_WRITE = 0x02,
//More write Stuff
	AR_SIZE_BYTE_WRITE  = 0x0,
	AR_SIZE_SHORT_WRITE = 0x1,
	AR_SIZE_WORD_WRITE = 0x2,
//Write Pointer Stuff
	AR_BYTE_WRITE_POINTER  = 0x00,
	AR_SHORT_WRITE_POINTER = 0x01,
	AR_DWORD_WRITE_POINTER = 0x02,
//Subtype
	AR_SUB_RAM_WRITE	 = 0x0,
	AR_SUB_WRITE_POINTER = 0x1, 
	AR_SUB_ADD_CODE		 = 0x2, 
	AR_SUB_MASTER_CODE   = 0x3,
};

namespace ActionReplay
{

struct AREntry {
	AREntry() {}
	AREntry(u32 _addr, u32 _value) : cmd_addr(_addr), value(_value) {}
	u32 cmd_addr;
	u32 value;
};

struct ARCode {
	std::string name;
	std::vector<AREntry> ops;
	bool active;
};

void RunAllActive();
bool RunCode(const ARCode &arcode);
void LoadCodes(IniFile &ini, bool forceLoad);
void LoadCodes(std::vector<ARCode> &_arCodes, IniFile &ini);
size_t GetCodeListSize();
ARCode GetARCode(size_t index);
void SetARCode_IsActive(bool active, size_t index);
void UpdateActiveList();
void EnableSelfLogging(bool enable);
const std::vector<std::string> &GetSelfLog();
bool IsSelfLogging();
}  // namespace

#endif // _ACTIONREPLAY_H_
