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


// Simple partial Action Replay code system implementation.

// Will never be able to support some AR codes - specifically those that patch the running
// Action Replay engine itself - yes they do exist!!!

// Action Replay actually is a small virtual machine with a limited number of commands.
// It probably is Turing complete - but what does that matter when AR codes can write
// actual PowerPC code.

#include <string>
#include <vector>

#include "Common.h"
#include "StringUtil.h"
#include "HW/Memmap.h"
#include "ActionReplay.h"
#include "Core.h"
#include "ARDecrypt.h"
#include "LogManager.h"

namespace
{
	static std::vector<AREntry>::const_iterator iter;
	static std::vector<ARCode> arCodes;
	static ARCode code;
	static bool b_RanOnce = false;
} // namespace


bool DoARSubtype_RamWriteAndFill(u8 w, u32 addr, u32 data);
bool DoARSubtype_WriteToPointer(u8 w, u32 addr, u32 data);
bool DoARSubtype_AddCode(u8 w, u32 addr, u32 data);
bool DoARSubtype_MasterCodeAndWriteToCCXXXXXX();
bool DoARSubtype_Other(u8 cmd, u32 addr, u32 data);
bool DoARZeroCode_FillAndSlide(u32 addr_last, u32 addr, u32 data);
bool DoARZeroCode_MemoryCopy(u32 val_last, u32 addr, u32 data);
void LogInfo(const char *format, ...);

// Parses the Action Replay section of a game ini file.
void LoadActionReplayCodes(IniFile &ini, bool bForGUI) 
{
	if (!Core::GetStartupParameter().bEnableCheats && !bForGUI) 
		return; // If cheats are off, do not load them; but load anyway if it's for GameConfig GUI

	std::vector<std::string> lines;
	std::vector<std::string> encryptedLines;
	ARCode currentCode;
	arCodes.clear();

	if (!ini.GetLines("ActionReplay", lines))
		return;  // no codes found.

	for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
	{
		std::string line = *it;
		std::vector<std::string> pieces; 

		// Check if the line is a name of the code
		if (line[0] == '+' || line[0] == '$')
		{
			if (currentCode.ops.size())
			{
				arCodes.push_back(currentCode);
				currentCode.ops.clear();
			}
			if (encryptedLines.size())
			{
				DecryptARCode(encryptedLines, currentCode.ops);
				arCodes.push_back(currentCode);
				currentCode.ops.clear();
				encryptedLines.clear();
			}
			currentCode.name = line;
			if (line[0] == '+'){
				Core::DisplayMessage("AR code active: " + line, 5000);
				currentCode.active = true;
			}
			else
				currentCode.active = false;
			continue;
		}

		SplitString(line, " ", pieces);

		// Check if the AR code is decrypted
		if (pieces.size() == 2 && pieces[0].size() == 8 && pieces[1].size() == 8)
		{
			AREntry op;
			bool success_addr = TryParseUInt(std::string("0x") + pieces[0], &op.cmd_addr);
   			bool success_val = TryParseUInt(std::string("0x") + pieces[1], &op.value);
			if (!(success_addr | success_val)) {
				PanicAlert("Action Replay Error: invalid AR code line: %s", line.c_str());
				if (!success_addr) PanicAlert("The address is invalid");
				if (!success_val) PanicAlert("The value is invalid");
			}
			else
				currentCode.ops.push_back(op);
		}
		else
		{
			SplitString(line, "-", pieces);
			if (pieces.size() == 3 && pieces[0].size() == 4 && pieces[1].size() == 4 && pieces[2].size() == 5) 
			{
				// Encrypted AR code
				// Decryption is done in "blocks", so we must push blocks into a vector,
				//	then send to decrypt when a new block is encountered, or if it's the last block.
				encryptedLines.push_back(pieces[0]+pieces[1]+pieces[2]);
			}
		}
	}

	// Handle the last code correctly.
	if (currentCode.ops.size())
	{
		arCodes.push_back(currentCode);
	}
	if (encryptedLines.size())
	{
		DecryptARCode(encryptedLines, currentCode.ops);
		arCodes.push_back(currentCode);
	}
}

void ActionReplayRunAllActive()
{
	if (Core::GetStartupParameter().bEnableCheats) {
		for (std::vector<ARCode>::iterator iter = arCodes.begin(); iter != arCodes.end(); ++iter)
			if (iter->active) {
				if(!RunActionReplayCode(*iter))
					iter->active = false;
				LogInfo("\n");
			}
		if(!b_RanOnce) 
			b_RanOnce = true;
	}
}


// The mechanism is slightly different than what the real AR uses, so there may be compatibility problems.
// For example, some authors have created codes that add features to AR. Hacks for popular ones can be added here,
// but the problem is not generally solvable.
// TODO: what is "nowIsBootup" for?
bool RunActionReplayCode(const ARCode &arcode) {
	u8 cmd;
	u32 addr;
	u32 data;
	u8 subtype;
	u8 w;
	u8 type;
	u8 zcode;
	bool doFillNSlide = false;
	bool doMemoryCopy = false;
	u32 addr_last = 0;
	u32 val_last;

	code = arcode;

	LogInfo("Code Name: %s", code.name.c_str());
	LogInfo("Num code lines: %i", code.ops.size());
	
	for (iter = code.ops.begin(); iter != code.ops.end(); ++iter) 
	{
		LogInfo("====Do Code====");
		cmd = iter->cmd_addr >> 24; // AR command
		addr = iter->cmd_addr; // AR command with address offset
		data = iter->value;
		subtype = ((addr >> 30) & 0x03);
		w = (cmd & 0x07);
		type = ((addr >> 27) & 0x07);
		zcode = ((data >> 29) & 0x07);

		LogInfo("################");
		LogInfo("Command: %08x", cmd);
		LogInfo("Address: %08x", addr);
		LogInfo("Data: %08x", data);
		LogInfo("SubType: %08x", subtype);
		LogInfo("W: %08x", w);
		LogInfo("ZCode: %08x", zcode);
		LogInfo("Type: %08x", type);
		LogInfo("################");

		// Do Fill & Slide
		if (doFillNSlide) {
			doFillNSlide = false;
			LogInfo("Doing Fill And Slide");
			if (!DoARZeroCode_FillAndSlide(addr_last, addr, data))
				return false;
			continue;
			}

		// Memory Copy
		if (doMemoryCopy) {
			doMemoryCopy = false;
			LogInfo("Doing Memory Copy");
			if (!DoARZeroCode_MemoryCopy(val_last, addr, data))
				return false;
			continue;
		}

		// ActionReplay program self modification codes
		if (addr >= 0x00002000 && addr < 0x00003000) {
			LogInfo("This action replay simulator does not support codes that modify Action Replay itself.");
			PanicAlert("This action replay simulator does not support codes that modify Action Replay itself.");
			return false;
		}

		// skip these weird init lines
	    if (iter == code.ops.begin() && cmd == 1) continue;

		// Zero codes
		if (addr == 0x0) // Check if the code is a zero code
		{
			LogInfo("This is a zcode");
			switch(zcode)
			{
				case 0x00: // END OF CODES
					LogInfo("ZCode: End Of Codes");
					return true;
				case 0x02: // Normal execution of codes
					// Todo: Set register 1BB4 to 0
					LogInfo("ZCode: Normal execution of codes, set register 1BB4 to 0 (zcode not supported)");
					break;
				case 0x03: // Executes all codes in the same row
					// Todo: Set register 1BB4 to 1
					LogInfo("ZCode: Executes all codes in the same row, Set register 1BB4 to 1 (zcode not supported)");
					PanicAlert("Zero 3 code not supported");
					return false;
				case 0x04: // Fill & Slide or Memory Copy
					if (((addr >> 25) & 0x03) == 0x3) {
						LogInfo("ZCode: Memory Copy");
						doMemoryCopy = true;
						addr_last = addr;
						val_last = data;
					}
					else 
					{
						LogInfo("ZCode: Fill And Slide");
						doFillNSlide = true;
						addr_last = addr;
					}
					continue;
				default: 
					LogInfo("ZCode: Unknown");
					PanicAlert("Zero code unknown to dolphin: %08x",zcode); 
					return false;
			}
		}

		// Normal codes
		LogInfo("Is a normal code");
		switch (subtype)
		{
		case 0x0: // Ram write (and fill)
			LogInfo("NCode: Ram Write And Fill");
			if (!DoARSubtype_RamWriteAndFill(w, addr, data))
				return false;
			continue;
		case 0x1: // Write to pointer
			LogInfo("NCode: Write To Pointer");
			if (!DoARSubtype_WriteToPointer(w, addr, data))
				return false;
			continue;
		case 0x2: // Add code
			LogInfo("NCode: Add Code");
			if (!DoARSubtype_AddCode(w, addr, data))
				return false;
			continue;
		case 0x3: // Master Code & Write to CCXXXXXX
			LogInfo("NCode: Master Code And Write to CCXXXXXX (ncode not supported)");
			if (!DoARSubtype_MasterCodeAndWriteToCCXXXXXX())
				return false;
			continue;
		default: // non-specific z codes (hacks)
			LogInfo("NCode: other AR hacks");
			if (!DoARSubtype_Other(cmd, addr, data))
				return false;
			continue;
		}
	}
	return true;
}

bool DoARSubtype_RamWriteAndFill(u8 w, u32 addr, u32 data)
{
	if (w < 0x8) // Check the value W in 0xZWXXXXXXX
	{
		u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000); // real GC address
		u8 size = ((addr >> 25) & 0x03);
		LogInfo("Gamecube Address: %08x", new_addr);
		LogInfo("Size: %08x", size);
		switch (size)
		{
		case 0x00: // Byte write
			{
				LogInfo("Byte Write");
				LogInfo("--------");
				u8 repeat = data >> 8;
				for (int i = 0; i <= repeat; i++) {
					Memory::Write_U8(data & 0xFF, new_addr + i);
					LogInfo("Wrote %08x to address %08x", data & 0xFF, new_addr + i);
				}
				LogInfo("--------");
				break;
			}

		case 0x01: // Short write
			{
				LogInfo("Short Write");
				LogInfo("--------");
				u16 repeat = data >> 16;
				for (int i = 0; i <= repeat; i++) {
					Memory::Write_U16(data & 0xFFFF, new_addr + i * 2);
					LogInfo("Wrote %08x to address %08x", data & 0xFFFF, new_addr + i * 2);
				}
				LogInfo("--------");
				break;
			}
		case 0x03: //some codes use 03, but its just the same as 02...
			LogInfo("The odd size 3 code (we just decided to write a U32 for this)");
		case 0x02: // Dword write
			LogInfo("Dword Write");
			LogInfo("--------");
			Memory::Write_U32(data, new_addr);
			LogInfo("Wrote %08x to address %08x", data, new_addr);
			LogInfo("--------");
			break;
		default:
			LogInfo("Bad Size");
			PanicAlert("Action Replay Error: Invalid size (%08x : address = %08x) in Ram Write And Fill (%s)", size, addr, code.name.c_str());
			return false;
		}
		return true;
	}
	return false;
}

bool DoARSubtype_WriteToPointer(u8 w, u32 addr, u32 data)
{
	if (w < 0x8)
	{
		u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000);
		u8 size = ((addr >> 25) & 0x03);
		LogInfo("Gamecube Address: %08x", new_addr);
		LogInfo("Size: %08x", size);
		switch (size)
		{
		case 0x00: // Byte write to pointer [40]
			{
				LogInfo("Write byte to pointer");
				LogInfo("--------");
				u32 ptr = Memory::Read_U32(new_addr);
				u8 thebyte = data & 0xFF;
				u32 offset = data >> 8;
				LogInfo("Pointer: %08x", ptr);
				LogInfo("Byte: %08x", thebyte);
				LogInfo("Offset: %08x", offset);
				Memory::Write_U8(thebyte, ptr + offset);
				LogInfo("Wrote %08x to address %08x", thebyte, ptr + offset);
				LogInfo("--------");
				break;
			}

		case 0x01: // Short write to pointer [42]
			{
				LogInfo("Write short to pointer");
				LogInfo("--------");
				u32 ptr = Memory::Read_U32(new_addr);
				u16 theshort = data & 0xFFFF;
				u32 offset = (data >> 16) << 1;
				LogInfo("Pointer: %08x", ptr);
				LogInfo("Byte: %08x", theshort);
				LogInfo("Offset: %08x", offset);
				Memory::Write_U16(theshort, ptr + offset);
				LogInfo("Wrote %08x to address %08x", theshort, ptr + offset);
				LogInfo("--------");
				break;
			}

		case 0x02: // Dword write to pointer [44]
			LogInfo("Write dword to pointer");
			LogInfo("--------");
			Memory::Write_U32(data, Memory::Read_U32(new_addr));
			LogInfo("Wrote %08x to address %08x", data, Memory::Read_U32(new_addr));
			LogInfo("--------");
			break;

		default:
			LogInfo("Bad Size");
			PanicAlert("Action Replay Error: Invalid size (%08x : address = %08x) in Write To Pointer (%s)", size, addr, code.name.c_str());
			return false;
		}
		return true;
	}
	return false;
}

bool DoARSubtype_AddCode(u8 w, u32 addr, u32 data)
{
	if (w < 0x8)
	{
		u32 new_addr = (addr & 0x81FFFFFF);
		u8 size = ((addr >> 25) & 0x03);
		LogInfo("Gamecube Address: %08x", new_addr);
		LogInfo("Size: %08x", size);
		switch (size)
		{
		case 0x0: // Byte add
			LogInfo("Byte Add");
			LogInfo("--------");
			Memory::Write_U8(Memory::Read_U8(new_addr) + (data & 0xFF), new_addr);
			LogInfo("Wrote %08x to address %08x", Memory::Read_U8(new_addr) + (data & 0xFF), new_addr);
			LogInfo("--------");
			break;
		case 0x1: // Short add
			LogInfo("Short Add");
			LogInfo("--------");
			Memory::Write_U16(Memory::Read_U16(new_addr) + (data & 0xFFFF), new_addr);
			LogInfo("Wrote %08x to address %08x", Memory::Read_U16(new_addr) + (data & 0xFFFF), new_addr);
			LogInfo("--------");
			break;
		case 0x2: // DWord add
			LogInfo("Dword Add");
			LogInfo("--------");
			Memory::Write_U32(Memory::Read_U32(new_addr) + data, new_addr);
			LogInfo("Wrote %08x to address %08x", Memory::Read_U32(new_addr) + data, new_addr);
			LogInfo("--------");
			break;
		case 0x3: // Float add (not working?)
			{
				LogInfo("Float Add");
				LogInfo("--------");
				union { u32 u; float f;} fu, d;
				fu.u = Memory::Read_U32(new_addr);
				d.u = data;
				fu.f += data;
				Memory::Write_U32(fu.u, new_addr);
				LogInfo("Wrote %08x to address %08x", fu.u, new_addr);
				LogInfo("--------");
				break;
			}
		default:
			LogInfo("Bad Size");
			PanicAlert("Action Replay Error: Invalid size(%08x : address = %08x) in Add Code (%s)", size, addr, code.name.c_str());
			return false;
		}
		return true;
	}
	return false;
}

bool DoARSubtype_MasterCodeAndWriteToCCXXXXXX()
{
	// code not yet implemented - TODO
	PanicAlert("Action Replay Error: Master Code and Write To CCXXXXXX not implemented (%s)", code.name.c_str());
	return false;
}

// TODO(Omega): I think this needs cleanup, there might be a better way to code this part
bool DoARSubtype_Other(u8 cmd, u32 addr, u32 data)
{
	LogInfo("Case: %08x", cmd & 0xFE);
	LogInfo("Sorry not logging for now ... too much of a mess here ...");
	switch (cmd & 0xFE) 
	{
	case 0x90:
		// Eh, this must be wrong. Should it really fallthrough?
		if (Memory::Read_U32(addr) == data) return true; // IF 32 bit equal, exit
	case 0x08: // IF 8 bit equal, execute next opcode
	case 0x48: // (double)
		if (Memory::Read_U16(addr) != (data & 0xFFFF)) {
			if (++iter == code.ops.end()) return true;
			if (cmd == 0x48) if (++iter == code.ops.end()) return true;
		}
		break;
	case 0x0A: // IF 16 bit equal, execute next opcode
	case 0x4A: // (double)
		if (Memory::Read_U16(addr) != (data & 0xFFFF)) {
			if (++iter == code.ops.end()) return true;
			if (cmd == 0x4A) if (++iter == code.ops.end()) return true;
		}
		break;
	case 0x0C:  // IF 32 bit equal, execute next opcode
	case 0x4C:  // (double)
		if (Memory::Read_U32(addr) != data) {
			if (++iter == code.ops.end()) return true;
			if (cmd == 0x4C) if (++iter == code.ops.end()) return true;
		}
		break;
	case 0x10:  // IF NOT 8 bit equal, execute next opcode
	case 0x50:  // (double)
		if (Memory::Read_U8(addr) == (data & 0xFF)) {
			if (++iter == code.ops.end()) return true;
			if (cmd == 0x50) if (++iter == code.ops.end()) return true;
		}
		break;
	case 0x12:  // IF NOT 16 bit equal, execute next opcode
	case 0x52:  // (double)
		if (Memory::Read_U16(addr) == (data & 0xFFFF)) {
			if (++iter == code.ops.end()) return true;
			if (cmd == 0x52) if (++iter == code.ops.end()) return true;
		}
		break;
	case 0x14:  // IF NOT 32 bit equal, execute next opcode
	case 0x54:  // (double)
		if (Memory::Read_U32(addr) == data) {
			if (++iter == code.ops.end()) return true;
			if (cmd == 0x54) if (++iter == code.ops.end()) return true;
		}
		break;
	case 0xC4: // "Master Code" - configure the AR
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
			break;
		}
	default:
		PanicAlert("Action Replay Error: Unknown Action Replay command %02x (%08x %08x)(%s)", cmd, iter->cmd_addr, iter->value, code.name.c_str());
		return false;
	}
	return true;
}

bool DoARZeroCode_FillAndSlide(u32 addr_last, u32 addr, u32 data) // This needs more testing
{
	u32 new_addr = (addr_last & 0x81FFFFFF);
	u8 size = ((new_addr >> 25) & 0x03);
	int addr_incr;
	u32 val = addr;
	int val_incr;
	u8 write_num = ((data & 0x78000) >> 16); // Z2
	u32 curr_addr = new_addr;
	LogInfo("Current Gamecube Address: %08x", new_addr);
	LogInfo("Size: %08x", size);
	LogInfo("Write Num: %08x", write_num);

	if (write_num < 1) {
		LogInfo("Write Num is less than 1, exiting Fill and Slide call...");
		return true;
	}

	if ((data >> 24) >> 3) { // z1 >> 3
		addr_incr = ((data & 0x7FFF) + 0xFFFF0000); // FFFFZ3Z4 
		val_incr = (int)((data & 0x7F) + 0xFFFFFF00); // FFFFFFZ1
	}
	else {
		addr_incr = (data & 0x7FFF); // 0000Z3Z4
		val_incr = (int)(data & 0x7F); // 000000Z1
	}

	LogInfo("Address Increment: %08x", addr_incr);
	LogInfo("Value Increment: %08x", val_incr);

	// Correct?
	if (val_incr < 0)
	{
		curr_addr = new_addr + (addr_incr * write_num);
		LogInfo("Value increment is less than 0, we need to go to the last address");
		LogInfo("Current GCN Address Update: %08x", curr_addr);
	}

	switch(size)
	{
		case 0x0: // Byte
				LogInfo("Byte Write");
				LogInfo("--------");
				for (int i=0; i < write_num; i++) {
					u8 repeat = val >> 8;
					for(int j=0; j < repeat; j++) {
						Memory::Write_U8(val & 0xFF, new_addr + j);
						LogInfo("Write %08x to address %08x", val & 0xFF, new_addr + j);
						val += val_incr;
						curr_addr += addr_incr;
						LogInfo("Value Update: %08x", val);
						LogInfo("Current GCN Address Update: %08x", curr_addr);
					}
				}
				LogInfo("--------");
				break;
		case 0x1: // Halfword
				addr_incr >>= 1;
				LogInfo ("Address increment shifted right by 1: %08x", addr_incr);
				LogInfo("Short Write");
				LogInfo("--------");
				for (int i=0; i < write_num; i++) {
					u8 repeat = val >> 16;
					for(int j=0; j < repeat; j++) {
						Memory::Write_U16(val & 0xFFFF, new_addr + j * 2);
						LogInfo("Write %08x to address %08x", val & 0xFFFF, new_addr + j * 2);
						val += val_incr;
					    curr_addr += addr_incr;
						LogInfo("Value Update: %08x", val);
						LogInfo("Current GCN Address Update: %08x", curr_addr);
					}
				}
				LogInfo("--------");
				break;
		case 0x2: // Word
				addr_incr >>= 2;
				LogInfo ("Address increment shifted right by 2: %08x", addr_incr);
				LogInfo("Word Write");
				LogInfo("--------");
				for (int i=0; i < write_num; i++) {
					Memory::Write_U32(val, new_addr);
					LogInfo("Write %08x to address %08x", val, new_addr);
					val += val_incr;
					curr_addr += addr_incr;
					LogInfo("Value Update: %08x", val);
					LogInfo("Current GCN Address Update: %08x", curr_addr);
				}
				LogInfo("--------");
				break;
		default:
			LogInfo("Bad Size");
			PanicAlert("Action Replay Error: Invalid size (%08x : address = %08x) in Fill and Slide (%s)", size, new_addr, code.name.c_str());
			return false;
	}
	return true;
}

bool DoARZeroCode_MemoryCopy(u32 val_last, u32 addr, u32 data) // Has not been tested
{
	u32 addr_dest = (val_last | 0x06000000);
	u32 addr_src = ((addr & 0x7FFFFF) | 0x80000000);
	u8 num_bytes = (data & 0x7FFF);
	LogInfo("Dest Address: %08x", addr_dest);
	LogInfo("Src Address: %08x", addr_src);
	LogInfo("Size: %08x", num_bytes);

	if ((data & ~0x7FFF) == 0x0000)
	{ 
		if((data >> 24) != 0x0)
		{ // Memory Copy With Pointers Support
			LogInfo("Memory Copy With Pointers Support");
			LogInfo("--------");
			for (int i = 0; i < 138; i++) {
				Memory::Write_U8(Memory::Read_U8(addr_src + i), addr_dest + i);
				LogInfo("Wrote %08x to address %08x", Memory::Read_U8(addr_src + i), addr_dest + i);
			}
			LogInfo("--------");
		}
		else
		{ // Memory Copy Without Pointer Support
			LogInfo("Memory Copy Without Pointers Support");
			LogInfo("--------");
			for (int i=0; i < num_bytes; i++) {
				Memory::Write_U32(Memory::Read_U32(addr_src + i), addr_dest + i);
				LogInfo("Wrote %08x to address %08x", Memory::Read_U32(addr_src + i), addr_dest + i);
			}
			LogInfo("--------");
			return true;
		}
	}
	else
	{
		LogInfo("Bad Value");
		PanicAlert("Action Replay Error: Invalid value (&08x) in Memory Copy (%s)", (data & ~0x7FFF), code.name.c_str());
		return false;
	}
	return true;
}

void LogInfo(const char *format, ...)
{
	if(!b_RanOnce && IsLoggingActivated()) {
		char* temp = (char*)alloca(strlen(format)+512);
		va_list args;
		va_start(args, format);
		CharArrayFromFormatV(temp, 512, format, args);
		va_end(args);
		LogManager::Log(LogTypes::ACTIONREPLAY, temp);
	}
}
