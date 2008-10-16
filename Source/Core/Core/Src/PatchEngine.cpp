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

// PatchEngine
// Supports simple memory patches, and has a partial Action Replay implementation
// in ActionReplay.cpp/h.

// Zelda item hang fixes:
// [Tue Aug 21 2007] [18:30:40] <Knuckles->    0x802904b4 in US released
// [Tue Aug 21 2007] [18:30:53] <Knuckles->    0x80294d54 in EUR Demo version
// [Tue Aug 21 2007] [18:31:10] <Knuckles->    we just patch a blr on it (0x4E800020)
// [OnLoad]
// 0x80020394=dword,0x4e800020

#include <string>
#include <vector>
#include "StringUtil.h"
#include "PatchEngine.h"
#include "IniFile.h"
#include "HW/Memmap.h"
#include "ActionReplay.h"

using namespace Common;

namespace
{

std::vector<Patch> onLoad;
std::vector<Patch> onFrame;

}  // namespace

void LoadPatchSection(const char *section, std::vector<Patch> &patches, IniFile &ini)
{
	std::vector<std::string> keys;
	ini.GetKeys(section, keys);

	for (std::vector<std::string>::const_iterator iter = keys.begin(); iter != keys.end(); ++iter)
	{
		std::string key = *iter;
		std::string value;
		ini.Get(section, key.c_str(), &value, "BOGUS");
		if (value != "BOGUS")
		{
			std::string val(value);
			std::vector<std::string> items;
			SplitString(val, ":", items);
			Patch p;
			bool success = true;
			success = success && TryParseUInt(std::string(key.c_str()), &p.address);
			success = success && TryParseUInt(items[1], &p.value);
			p.type = (PatchType)ChooseStringFrom(items[0].c_str(), PatchTypeStrings);
			success = success && (p.type != (PatchType)-1);
			if (success)
				patches.push_back(p);
		}
	}
}

void PatchEngine_LoadPatches(const char *gameID)
{
	IniFile ini;
	std::string filename = std::string("GameIni/") + gameID + ".ini";
	if (ini.Load(filename.c_str())) {
		LoadPatchSection("OnLoad",  onLoad, ini);
		LoadPatchSection("OnFrame", onFrame, ini);
		LoadActionReplayCodes(ini);
	}
}

void ApplyPatches(const std::vector<Patch> &patches)
{
	for (std::vector<Patch>::const_iterator iter = patches.begin(); iter != patches.end(); ++iter)
	{
		u32 addr = iter->address;
		u32 value = iter->value;
		switch (iter->type)
		{
		case PATCH_8BIT:
			Memory::Write_U8((u8)value, addr);
			break;
		case PATCH_16BIT:
			Memory::Write_U16((u16)value, addr);
			break;
		case PATCH_32BIT:
			Memory::Write_U32(value, addr);
			break;
		default:
			//unknown patchtype
			break;
		}
	}
}

void PatchEngine_ApplyLoadPatches() 
{
	ApplyPatches(onLoad);
}

void PatchEngine_ApplyFramePatches() 
{
	ApplyPatches(onFrame);
}

void PatchEngine_ApplyARPatches()
{
	ActionReplayRunAllActive();
}