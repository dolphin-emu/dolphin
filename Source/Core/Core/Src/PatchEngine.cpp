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
#include <map>
#include "StringUtil.h"
#include "PatchEngine.h"
#include "HW/Memmap.h"
#include "ActionReplay.h"

using namespace Common;

namespace PatchEngine
{

std::vector<Patch> onFrame;
std::map<u32, int> speedHacks;

void LoadPatchSection(const char *section, std::vector<Patch> &patches, IniFile &ini)
{
	std::vector<std::string> lines;
	if (!ini.GetLines(section, lines))
		return;

	Patch currentPatch;

	for (std::vector<std::string>::const_iterator iter = lines.begin(); iter != lines.end(); ++iter)
	{
		std::string line = *iter;
		if (line.size())
		{
			if (line[0] == '+' || line[0] == '$')
			{
				// Take care of the previous code
				if (currentPatch.name.size()) patches.push_back(currentPatch);
				currentPatch.entries.clear();

				// Set active and name
				currentPatch.active = (line[0] == '+') ? true : false;
				if (currentPatch.active)
					currentPatch.name = line.substr(2, line.size() - 2);
				else
					currentPatch.name = line.substr(1, line.size() - 1);
				continue;
			}

			std::string::size_type loc = line.find_first_of('=', 0);
			if (loc != std::string::npos)
				line.at(loc) = ':';

			std::vector<std::string> items;
			SplitString(line, ":", items);
			if (items.size() >= 3) {
				PatchEntry pE;
				bool success = true;
				success = success && TryParseUInt(items[0], &pE.address);
				success = success && TryParseUInt(items[2], &pE.value);
				pE.type = (PatchType)ChooseStringFrom(items[1].c_str(), PatchTypeStrings);
				success = success && (pE.type != (PatchType)-1);
				if (success)
					currentPatch.entries.push_back(pE);
			}
		}
	}
	if (currentPatch.name.size()) patches.push_back(currentPatch);
}

static void LoadSpeedhacks(const char *section, std::map<u32, int> &hacks, IniFile &ini) {
	std::vector<std::string> keys;
	ini.GetKeys(section, keys);
	for (std::vector<std::string>::const_iterator iter = keys.begin(); iter != keys.end(); ++iter)
	{
		std::string key = *iter;
		std::string value;
		ini.Get(section, key.c_str(), &value, "BOGUS");
		if (value != "BOGUS")
		{
			u32 address;
			u32 cycles;
			bool success = true;
			success = success && TryParseUInt(std::string(key.c_str()), &address);
			success = success && TryParseUInt(value, &cycles);
			if (success) {
				speedHacks[address] = (int)cycles;
			}
		}
	}
}

int GetSpeedhackCycles(u32 addr)
{
	std::map<u32, int>::const_iterator iter = speedHacks.find(addr);
	if (iter == speedHacks.end())
		return 0;
	else
		return iter->second;
}

void LoadPatches(const char *gameID)
{
	IniFile ini;
	std::string filename = std::string(FULL_GAMECONFIG_DIR) + gameID + ".ini";
	if (ini.Load(filename.c_str())) {
		LoadPatchSection("OnFrame", onFrame, ini);
		LoadActionReplayCodes(ini);
		LoadSpeedhacks("Speedhacks", speedHacks, ini);
	}
}

void ApplyPatches(const std::vector<Patch> &patches)
{
	for (std::vector<Patch>::const_iterator iter = patches.begin(); iter != patches.end(); ++iter)
	{
		if (iter->active)
		{
			for (std::vector<PatchEntry>::const_iterator iter2 = iter->entries.begin(); iter2 != iter->entries.end(); ++iter2)
			{
				u32 addr = iter2->address;
				u32 value = iter2->value;
				switch (iter2->type)
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
	}
}

void ApplyFramePatches() 
{
	ApplyPatches(onFrame);
}

void ApplyARPatches()
{
	ActionReplayRunAllActive();
}
}  // namespace
