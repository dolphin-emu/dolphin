// Copyright (C) 2003-2008 Dolphin Project.

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

#ifndef _PATCHENGINE_H
#define _PATCHENGINE_H

#include "IniFile.h"

namespace PatchEngine
{

enum PatchType
{
	PATCH_8BIT,
	PATCH_16BIT,
	PATCH_32BIT,
};

static const char *PatchTypeStrings[] = 
{
	"byte",
	"word",
	"dword",
	0
};

struct PatchEntry
{
	PatchEntry() {}
	PatchEntry(PatchType _t, u32 _addr, u32 _value) : type(_t), address(_addr), value(_value) {}
	PatchType type;
	u32 address;
	u32 value;
};

struct Patch
{
	std::string name;
	std::vector<PatchEntry> entries;
	bool active;
};

int GetSpeedhackCycles(u32 addr);
void LoadPatchSection(const char *section, std::vector<Patch> &patches, IniFile &ini);
void LoadPatches(const char *gameID);
void ApplyFramePatches();
void ApplyARPatches();
}  // namespace

#endif //_PATCHENGINE_H
