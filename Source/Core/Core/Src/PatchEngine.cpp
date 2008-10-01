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

#include <string>
#include <vector>

// PatchEngine

// [Tue Aug 21 2007] [18:30:40] <Knuckles->    0x802904b4 in US released
// [Tue Aug 21 2007] [18:30:53] <Knuckles->    0x80294d54 in EUR Demo version
// [Tue Aug 21 2007] [18:31:10] <Knuckles->    we just patch a blr on it (0x4E800020)
// A little present to our dear hacker friends
// (A partial Action Replay engine)
// And a temporary "solution" to Zelda item glitch...

// [OnLoad]
// 0x80020394=dword,0x4e800020

// #define BLR_OP 0x4e800020

#include "StringUtil.h"
#include "PatchEngine.h"
#include "IniFile.h"
#include "HW/Memmap.h"


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


struct Patch
{
	Patch() {}
	Patch(PatchType _t, u32 _addr, u32 _value) : type(_t), address(_addr), value(_value) {}
	PatchType type;
	u32 address;
	u32 value;
};

std::vector<Patch> onLoad;
std::vector<Patch> onFrame;

struct AREntry {
	u32 cmd_addr;
	u32 value;
};

struct ARCode {
	std::string name;
	std::vector<AREntry> ops;
	bool active;
};

std::vector<ARCode> arCodes;

using namespace Common;

void RunActionReplayCode(const ARCode &code, bool nowIsBootup);

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

void LoadActionReplayCodes(IniFile &ini);

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
	for (std::vector<ARCode>::const_iterator iter = arCodes.begin(); iter != arCodes.end(); ++iter) {
		if (iter->active)
			RunActionReplayCode(*iter, false);
	}
}

void LoadActionReplayCodes(IniFile &ini) 
{
	std::vector<std::string> lines;
	ARCode currentCode;
	arCodes.clear();
	if (!ini.GetLines("ActionReplay", lines)) 
		return;

	for (std::vector<std::string>::const_iterator iter = lines.begin(); iter != lines.end(); ++iter) {
		std::string line = StripSpaces(*iter);
		std::vector<std::string> pieces;
		SplitString(line, " ", pieces);
		if (pieces.size() == 2 && pieces[0].size() == 8 && pieces[1].size() == 8) {
			// Smells like a decrypted Action Replay code, great! Decode!
			AREntry op;
			bool success = TryParseUInt(std::string("0x") + pieces[0], &op.cmd_addr);
			success |= TryParseUInt(std::string("0x") + pieces[1], &op.value);
			if (!success) {
				PanicAlert("Invalid AR code line: %s", line.c_str());
			} else {
				currentCode.ops.push_back(op);
			}
		} else {
			SplitString(line, "-", pieces);
			if (pieces.size() == 3 && pieces[0].size() == 4 && pieces[1].size() == 4 && pieces[2].size() == 4) 
			{
				// Encrypted AR code
				PanicAlert("Dolphin does not yet support encrypted AR codes.");
			}
			else if (line.size()) {
				if (line[0] == '+') { 
					// Active code
					line = StripSpaces(line.substr(1));
					currentCode.name = line;
					currentCode.active = true;
				}
				else {
					currentCode.name = line;
					currentCode.active = false;
				}
			} else {
				// Empty line - end of code. Push it.
				arCodes.push_back(currentCode);
				currentCode.name = "(invalid)";
				currentCode.ops.clear();
			}
		}
	}
	arCodes.push_back(currentCode);
}

// The mechanism is slightly different than what the real AR uses, so there may be compatibility problems.
// For example, some authors have created codes that add features to AR. Hacks for popular ones can be added here,
// but the problem is not generally solvable.
void RunActionReplayCode(const ARCode &code, bool nowIsBootup) {
	for (std::vector<AREntry>::const_iterator iter = code.ops.begin(); iter != code.ops.end(); ++iter) {
		u32 cmd_addr = iter->cmd_addr;
		u8 cmd = iter->cmd_addr>>24;
		u32 addr = (iter->cmd_addr & 0xFFFFFF);
		u32 data = iter->value;

		if (addr >= 0x00002000 && addr < 0x00003000) {
			PanicAlert("This action replay simulator does not support codes that modify Action Replay itself.");
			return;
		}

		// End of sequence. Dunno why anybody would use it.
		if (iter->cmd_addr == 0) {
			// Special command!
			if ((data >> 28) == 0x8) {  // Fill 'n' slide
				PanicAlert("Fill'n'slide command not yet supported.");
				++iter; /*
				u32 x = data;
				u32 size = (addr >> 25) & 3;
				addr &= 0x01FFFFFF;*/
			}
			else {
				if (data == 0x40000000) 
				{
					// Resume normal execution. Don't need to do anything here.
				}
				else 
				{
					PanicAlert("This action replay command (%08x %08x) not yet supported.", cmd_addr, data);
				}
			}
		}

		// skip these weird init lines
		if (iter == code.ops.begin() && cmd == 1) 
			continue;

		switch (cmd & 0xFE) {
		case 0x00:  // Byte write
			{
				u8 repeat = data >> 8;
				for (int i = 0; i <= repeat; i++) {
					Memory::Write_U8(data & 0xFF, addr + i);
				}
				break;
			}

		case 0x02:  // Short write
			{
				u16 repeat = data >> 16;
				for (int i = 0; i <= repeat; i++) {
					Memory::Write_U16(data & 0xFFFF, addr + i * 2);
				}
				break;
			}

		case 0x04:  // Dword write
			Memory::Write_U32(data, addr);
			break;

		case 0x80:  // Byte add
			Memory::Write_U8(Memory::Read_U8(addr) + (data & 0xFF), addr);
			break;
		case 0x82:  // Short add
			Memory::Write_U16(Memory::Read_U16(addr) + (data & 0xFFFF), addr);
			break;
		case 0x84:  // DWord add
			Memory::Write_U32(Memory::Read_U32(addr) + data, addr);
			break;
		case 0x86:  // Float add (not working?)
			{
				union {
					u32 u;
					float f;
				} fu, d;
				fu.u = Memory::Read_U32(addr);
				d.u = data;
				fu.f += data;
				Memory::Write_U32(fu.u, addr);
			}
			break;

		case 0x90:  // IF 32 bit equal, exit
			if (Memory::Read_U32(addr) == data)
				return;

		case 0x08:  // IF 8 bit equal, execute next opcode
		case 0x48:  // (double)
			if (Memory::Read_U16(addr) != (data & 0xFFFF)) {
				if (++iter == code.ops.end()) return;
				if (cmd == 0x48) if (++iter == code.ops.end()) return;
			}
			break;

		case 0x0A:  // IF 16 bit equal, execute next opcode
		case 0x4A:  // (double)
			if (Memory::Read_U16(addr) != (data & 0xFFFF)) {
				if (++iter == code.ops.end()) return;
				if (cmd == 0x4A) if (++iter == code.ops.end()) return;
			}
			break;

		case 0x0C:  // IF 32 bit equal, execute next opcode
		case 0x4C:  // (double)
			if (Memory::Read_U32(addr) != data) {
				if (++iter == code.ops.end()) return;
				if (cmd == 0x4C) if (++iter == code.ops.end()) return;
			}
			break;

		case 0x10:  // IF NOT 8 bit equal, execute next opcode
		case 0x50:  // (double)
			if (Memory::Read_U8(addr) == (data & 0xFF)) {
				if (++iter == code.ops.end()) return;
				if (cmd == 0x50) if (++iter == code.ops.end()) return;
			}
			break;

		case 0x12:  // IF NOT 16 bit equal, execute next opcode
		case 0x52:  // (double)
			if (Memory::Read_U16(addr) == (data & 0xFFFF)) {
				if (++iter == code.ops.end()) return;
				if (cmd == 0x52) if (++iter == code.ops.end()) return;
			}
			break;

		case 0x14:  // IF NOT 32 bit equal, execute next opcode
		case 0x54:  // (double)
			if (Memory::Read_U32(addr) == data) {
				if (++iter == code.ops.end()) return;
				if (cmd == 0x54) if (++iter == code.ops.end()) return;
			}
			break;

		case 0x40: // Write byte to pointer.
			{
				u32 ptr = Memory::Read_U32(addr);
				u8 thebyte = data & 0xFF;
				u32 offset = data >> 8;
				Memory::Write_U8(thebyte, ptr + offset);
				break;
			}

		case 0x42: // Write short to pointer.
			{
				u32 ptr = Memory::Read_U32(addr);
				u16 theshort = data & 0xFFFF;
				u32 offset = (data >> 16) << 1;
				Memory::Write_U16(theshort, ptr + offset);
				break;
			}

		case 0x44: // Write dword to pointer.
			{
				u32 ptr = Memory::Read_U32(addr);
				Memory::Write_U32(data, ptr);
				break;
			}

		case 0xC4:
			// "Master Code" - configure the AR
			{
				u8 number = data & 0xFF;
				if (number == 0)
				{
					// Normal master code - execute once.
				} else {
					// PanicAlert("Not supporting multiple master codes.");
				}
				// u8 numOpsPerFrame = (data >> 8) & 0xFF;
				// Blah, we generally ignore master codes.	
			}
			break;
		default:
			PanicAlert("Unknown Action Replay command %02x (%08x %08x)", cmd, iter->cmd_addr, iter->value);
			break;
		}
	}
}
