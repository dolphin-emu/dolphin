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

// Partial Action Replay code system implementation.

// Will never be able to support some AR codes - specifically those that patch the running
// Action Replay engine itself - yes they do exist!!!

// Action Replay actually is a small virtual machine with a limited number of commands.
// It probably is Turing complete - but what does that matter when AR codes can write
// actual PowerPC code...

// THIS FILE IS GROSS!!!

#include <string>
#include <vector>

#include "Common.h"
#include "StringUtil.h"
#include "HW/Memmap.h"
#include "ActionReplay.h"
#include "Core.h"
#include "ARDecrypt.h"
#include "LogManager.h"

namespace ActionReplay
{

static std::vector<AREntry>::const_iterator iter;
static ARCode code;
static bool b_RanOnce = false;
static std::vector<ARCode> arCodes;
static std::vector<ARCode> activeCodes;
static bool logSelf = false;
static std::vector<std::string> arLog;


void LogInfo(const char *format, ...);
// --- Codes ---
// SubTypes (Normal 0 Codes)
bool Subtype_RamWriteAndFill(u32 addr, u32 data);
bool Subtype_WriteToPointer(u32 addr, u32 data);
bool Subtype_AddCode(u32 addr, u32 data);
bool Subtype_MasterCodeAndWriteToCCXXXXXX();
// Zero Codes
bool ZeroCode_FillAndSlide(u32 val_last, u32 addr, u32 data);
bool ZeroCode_MemoryCopy(u32 val_last, u32 addr, u32 data);
// Normal Codes
bool NormalCode_Type_0(u8 subtype, u32 addr, u32 data);
bool NormalCode_Type_1(u8 subtype, u32 addr, u32 data, int *count, bool *skip);
bool NormalCode_Type_2(u8 subtype, u32 addr, u32 data, int *count, bool *skip);
bool NormalCode_Type_3(u8 subtype, u32 addr, u32 data, int *count, bool *skip);
bool NormalCode_Type_4(u8 subtype, u32 addr, u32 data, int *count, bool *skip);
bool NormalCode_Type_5(u8 subtype, u32 addr, u32 data, int *count, bool *skip);
bool NormalCode_Type_6(u8 subtype, u32 addr, u32 data, int *count, bool *skip);
bool NormalCode_Type_7(u8 subtype, u32 addr, u32 data, int *count, bool *skip);

void LoadCodes(IniFile &ini, bool forceLoad)
{
	// Parses the Action Replay section of a game ini file.
	if (!Core::GetStartupParameter().bEnableCheats && !forceLoad) 
		return;

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
			
			if (line.size() > 1)
			{
				if (line[0] == '+')
				{
					currentCode.active = true;
					currentCode.name = line.substr(2, line.size() - 2);;
					Core::DisplayMessage("AR code active: " + currentCode.name, 5000);
				}
				else
				{
					currentCode.active = false;
					currentCode.name = line.substr(1, line.size() - 1);
				}
			}
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

	UpdateActiveList();
}

void LoadCodes(std::vector<ARCode> &_arCodes, IniFile &ini)
{
	LoadCodes(ini, true);
	_arCodes = arCodes;
}

void LogInfo(const char *format, ...)
{
	if (!b_RanOnce) 
	{
		if (LogManager::Enabled() || logSelf)
		{
			char* temp = (char*)alloca(strlen(format)+512);
			va_list args;
			va_start(args, format);
			CharArrayFromFormatV(temp, 512, format, args);
			va_end(args);
			if (LogManager::Enabled())
				LogManager::Log(LogTypes::ACTIONREPLAY, temp);
			if (logSelf)
			{
				std::string text = temp;
				text += "\n";
				arLog.push_back(text.c_str());
			}
		}
	}
}

void RunAllActive()
{
	if (Core::GetStartupParameter().bEnableCheats) {
		for (std::vector<ARCode>::iterator iter = activeCodes.begin(); iter != activeCodes.end(); ++iter) 
		{
			if (iter->active)
			{
				if (!RunCode(*iter))
					iter->active = false;
				LogInfo("\n");
			}
		}
		if (!b_RanOnce) 
			b_RanOnce = true;
	}
}

bool RunCode(const ARCode &arcode) {
	// The mechanism is different than what the real AR uses, so there may be compatibility problems.
	u8 cmd;
	u32 addr;
	u32 data;
	bool doFillNSlide = false;
	bool doMemoryCopy = false;
	int count = 0;
	bool skip = false;
	bool cond = false;
	u32 addr_last = 0;
	u32 val_last = 0;

	code = arcode;

	LogInfo("Code Name: %s", code.name.c_str());
	LogInfo("Number of codes: %i", code.ops.size());
	
	for (iter = code.ops.begin(); iter != code.ops.end(); ++iter) 
	{
		// If conditional mode has been set to true, then run our code execution control
		if (cond)
		{
			// Some checks on the count value
			if (count == -1 || count < -2 || count > (int)code.ops.size())
			{
				LogInfo("Bad Count: %i", count);
				PanicAlert("Action Replay: Bad Count: %i (%s)", count, code.name.c_str());
				return false;
			}

			if (skip && count > 0) { LogInfo("Line skipped"); if (count-- == 0) cond = false; continue; } // Skip n lines
			if (skip && count == -2) { LogInfo("Line skipped"); continue; } // Skip all lines

			if (!skip && count == 0) { LogInfo("Line skipped"); continue; }// Skip rest of lines
			if (!skip && count > 0) count--; // execute n lines
			// if -2 : execute all lines

			if (b_RanOnce)
				b_RanOnce = false;
		}

		cmd = iter->cmd_addr >> 24; // AR command
		addr = iter->cmd_addr; // AR command with address offset
		data = iter->value;
		
		LogInfo("--- Running Code: %08x %08x ---", addr, data);
		LogInfo("Command: %08x", cmd);

		// Do Fill & Slide
		if (doFillNSlide) {
			doFillNSlide = false;
			LogInfo("Doing Fill And Slide");
			if (!ZeroCode_FillAndSlide(val_last, addr, data))
				return false;
			continue;
			}

		// Memory Copy
		if (doMemoryCopy) {
			doMemoryCopy = false;
			LogInfo("Doing Memory Copy");
			if (!ZeroCode_MemoryCopy(val_last, addr, data))
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
	    if (iter == code.ops.begin() && cmd == 1)
			continue;

		// Zero codes
		if (addr == 0x0) // Check if the code is a zero code
		{
			u8 zcode = ((data >> 29) & 0x07);
			LogInfo("Doing Zero Code %08x", zcode);
			switch (zcode)
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
						val_last = data;
					}
					continue;
				default: 
					LogInfo("ZCode: Unknown");
					PanicAlert("Zero code unknown to dolphin: %08x",zcode); 
					return false;
			}
		}

		// Normal codes
		u8 type = ((addr >> 27) & 0x07);
		u8 subtype = ((addr >> 30) & 0x03);
		LogInfo("Doing Normal Code %08x", type);
		LogInfo("Subtype: %08x", subtype);
		if (type >= 1 && type <= 7) {
			cond = true;
			LogInfo("This Normal Code is a Conditional Code");
		}
		switch (type)
		{
		case 0x0:
			if (!NormalCode_Type_0(subtype, addr, data))
				return false;
			continue;
		case 0x1:
			LogInfo("Type 1: If Equal");
			if (!NormalCode_Type_1(subtype, addr, data, &count, &skip))
				return false;
			continue;
		case 0x2:
			LogInfo("Type 2: If Not Equal");
			if (!NormalCode_Type_2(subtype, addr, data, &count, &skip))
				return false;
			continue;
		case 0x3:
			LogInfo("Type 3: If Less Than (Signed)");
			if (!NormalCode_Type_3(subtype, addr, data, &count, &skip))
				return false;
			continue;
		case 0x4:
			LogInfo("Type 4: If Greater Than (Signed)");
			if (!NormalCode_Type_4(subtype, addr, data, &count, &skip))
				return false;
			continue;
		case 0x5:
			LogInfo("Type 5: If Less Than (Unsigned)");
			if (!NormalCode_Type_5(subtype, addr, data, &count, &skip))
				return false;
			continue;
		case 0x6:
			LogInfo("Type 6: If Greater Than (Unsigned)");
			if (!NormalCode_Type_6(subtype, addr, data, &count, &skip))
				return false;
			continue;
		case 0x7:
			LogInfo("Type 7: If AND");
			if (!NormalCode_Type_7(subtype, addr, data, &count, &skip))
				return false;
			continue;
		default:
			LogInfo("Bad Normal Code type");
			PanicAlert("Action Replay: Invalid Normal Code Type %08x (%s)", type, code.name.c_str());
		}
	}

	if (b_RanOnce && cond)
		b_RanOnce = true;

	return true;
}

// Subtypes
bool Subtype_RamWriteAndFill(u32 addr, u32 data)
{
	u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000); // real GC address
	u8 size = ((addr >> 25) & 0x03);
	LogInfo("Hardware Address: %08x", new_addr);
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

bool Subtype_WriteToPointer(u32 addr, u32 data)
{
	u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000);
	u8 size = ((addr >> 25) & 0x03);
	LogInfo("Hardware Address: %08x", new_addr);
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
	case 0x03:
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

bool Subtype_AddCode(u32 addr, u32 data)
{
	u32 new_addr = (addr & 0x81FFFFFF);
	u8 size = ((addr >> 25) & 0x03);
	LogInfo("Hardware Address: %08x", new_addr);
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
			float newval = (float)Memory::Read_U32(new_addr) + (float)data;
			Memory::Write_U32((u32)newval, new_addr);
			LogInfo("Wrote %08x to address %08x",  (u32)newval, new_addr);
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

bool Subtype_MasterCodeAndWriteToCCXXXXXX()
{
	// code not yet implemented - TODO
	PanicAlert("Action Replay Error: Master Code and Write To CCXXXXXX not implemented (%s)", code.name.c_str());
	return false;
}

// Zero Codes
bool ZeroCode_FillAndSlide(u32 val_last, u32 addr, u32 data) // This needs more testing
{
	u32 new_addr = (val_last & 0x81FFFFFF);
	u8 size = ((val_last >> 25) & 0x03);
	int addr_incr;
	u32 val = addr;
	int val_incr;
	u8 write_num = ((data & 0x78000) >> 16); // Z2
	u32 curr_addr = new_addr;
	LogInfo("Current Hardware Address: %08x", new_addr);
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

	LogInfo("Address Increment: %i", addr_incr);
	LogInfo("Value Increment: %i", val_incr);

	// Correct?
	if (val_incr < 0)
	{
		curr_addr = new_addr + (addr_incr * write_num);
		LogInfo("Value increment is less than 0, we need to go to the last address");
		LogInfo("Current Hardware Address Update: %08x", curr_addr);
	}

	switch (size)
	{
		case 0x0: // Byte
			LogInfo("Byte Write");
			LogInfo("--------");
			for (int i=0; i < write_num; i++) {
				Memory::Write_U8(val & 0xFF, curr_addr);
				LogInfo("Write %08x to address %08x", val & 0xFF, curr_addr);
				if (val_incr < 0)
				{
					val -= (u32)abs(val_incr);
				}
				if (val_incr > 0)
				{
					val += (u32)val_incr;
				}
				if (addr_incr < 0)
				{
					curr_addr -= (u32)abs(addr_incr);
				}
				if (addr_incr > 0)
				{
					curr_addr += (u32)addr_incr;
				}
				LogInfo("Value Update: %08x", val);
				LogInfo("Current Hardware Address Update: %08x", curr_addr);
			}
			LogInfo("--------");
			break;
		case 0x1: // Halfword
			LogInfo("Short Write");
			LogInfo("--------");
			for (int i=0; i < write_num; i++) {
				Memory::Write_U16(val & 0xFFFF, curr_addr);
				LogInfo("Write %08x to address %08x", val & 0xFFFF, curr_addr);
				if (val_incr < 0)
				{
					val -= (u32)abs(val_incr);
				}
				if (val_incr > 0)
				{
					val += (u32)val_incr;
				}
				if (addr_incr < 0)
				{
					curr_addr -= (u32)abs(addr_incr);
				}
				if (addr_incr > 0)
				{
					curr_addr += (u32)addr_incr;
				}
				LogInfo("Value Update: %08x", val);
				LogInfo("Current Hardware Address Update: %08x", curr_addr);
			}
			LogInfo("--------");
			break;
		case 0x2: // Word
			LogInfo("Word Write");
			LogInfo("--------");
			for (int i = 0; i < write_num; i++) {
				Memory::Write_U32(val, curr_addr);
				LogInfo("Write %08x to address %08x", val, curr_addr);
				if (val_incr < 0)
				{
					val -= (u32)abs(val_incr);
				}
				if (val_incr > 0)
				{
					val += (u32)val_incr;
				}
				if (addr_incr < 0)
				{
					curr_addr -= (u32)abs(addr_incr);
				}
				if (addr_incr > 0)
				{
					curr_addr += (u32)addr_incr;
				}
				LogInfo("Value Update: %08x", val);
				LogInfo("Current Hardware Address Update: %08x", curr_addr);
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

bool ZeroCode_MemoryCopy(u32 val_last, u32 addr, u32 data) // Has not been tested
{
	u32 addr_dest = (val_last | 0x06000000);
	u32 addr_src = ((addr & 0x7FFFFF) | 0x80000000);
	u8 num_bytes = (data & 0x7FFF);
	LogInfo("Dest Address: %08x", addr_dest);
	LogInfo("Src Address: %08x", addr_src);
	LogInfo("Size: %08x", num_bytes);

	if ((data & ~0x7FFF) == 0x0000)
	{ 
		if ((data >> 24) != 0x0)
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

// Normal Codes
bool NormalCode_Type_0(u8 subtype, u32 addr, u32 data)
{
	switch (subtype)
	{
	case 0x0: // Ram write (and fill)
		LogInfo("Doing Ram Write And Fill");
		if (!Subtype_RamWriteAndFill(addr, data))
			return false;
		break;
	case 0x1: // Write to pointer
		LogInfo("Doing Write To Pointer");
		if (!Subtype_WriteToPointer(addr, data))
			return false;
		break;
	case 0x2: // Add code
		LogInfo("Doing Add Code");
		if (!Subtype_AddCode(addr, data))
			return false;
		break;
	case 0x3: // Master Code & Write to CCXXXXXX
		LogInfo("Doing Master Code And Write to CCXXXXXX (ncode not supported)");
		if (!Subtype_MasterCodeAndWriteToCCXXXXXX())
			return false;
		break;
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code 0: Invalid Subtype %08x (%s)", subtype, code.name.c_str());
		return false;
	}
	return true;
}
// Conditional Codes
bool NormalCode_Type_1(u8 subtype, u32 addr, u32 data, int *count, bool *skip)
{
	u8 size = (addr >> 25) & 0x03;
	u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000);
	LogInfo("Size: %08x", size);
	LogInfo("Hardware Address: %08x", new_addr);
	bool con = true;
	switch (size)
	{
	case 0x0: con = (Memory::Read_U8(new_addr) == (u8)(data & 0xFF)); break;
	case 0x1: con = (Memory::Read_U16(new_addr) == (u16)(data & 0xFFFF)); break;
	case 0x3:
	case 0x2: con = (Memory::Read_U32(new_addr) == data); break;
	default:
		LogInfo("Bad Size");
		PanicAlert("Action Replay: Normal Code 1: Invalid Size %08x (%s)", size, code.name.c_str());
		return false;
	}

	*skip = !con; // set skip
	LogInfo("Skip set to %s", !con ? "False" : "True");

	switch (subtype)
	{
	case 0x0: *count = 1; break; // 1 line
	case 0x1: *count = 2; break; // 2 lines
	case 0x2: *count = -2; break; // all lines
	case 0x3: *count = -2; break; // While != : skip all codes ("infinite loop on the code" ?)
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code 1: Invalid subtype %08x (%s)", subtype, code.name.c_str());
		return false;
	}
	return true;
}

bool NormalCode_Type_2(u8 subtype, u32 addr, u32 data, int *count, bool *skip)
{
	u8 size = (addr >> 25) & 0x03;
	u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000);
	LogInfo("Size: %08x", size);
	LogInfo("Hardware Address: %08x", new_addr);
	bool con = true;
	switch (size)
	{
	case 0x0: con = (Memory::Read_U8(new_addr) != (u8)(data & 0xFF)); break;
	case 0x1: con = (Memory::Read_U16(new_addr) != (u16)(data & 0xFFFF)); break;
	case 0x3:
	case 0x2: con = (Memory::Read_U32(new_addr) != data); break;
	default:
		LogInfo("Bad Size");
		PanicAlert("Action Replay: Normal Code 2: Invalid Size %08x (%s)", size, code.name.c_str());
		return false;
	}

	*skip = !con; // set skip
	LogInfo("Skip set to %s", !con ? "False" : "True");

	switch (subtype)
	{
	case 0x0: *count = 1; break; // 1 line
	case 0x1: *count = 2; break; // 2 lines
	case 0x2: *count = -2; break; // all lines
	case 0x3: *count = -2; break; // While != : skip all codes ("infinite loop on the code" ?)
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code 2: Invalid subtype %08x (%s)", subtype, code.name.c_str());
		return false;
	}
	return true;
}

bool NormalCode_Type_3(u8 subtype, u32 addr, u32 data, int *count, bool *skip)
{
	u8 size = (addr >> 25) & 0x03;
	u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000);
	LogInfo("Size: %08x", size);
	LogInfo("Hardware Address: %08x", new_addr);
	bool con = true;
	switch (size)
	{
	case 0x0: con = ((char)Memory::Read_U8(new_addr) < (char)(data & 0xFF)); break;
	case 0x1: con = ((short)Memory::Read_U16(new_addr) < (short)(data & 0xFFFF)); break;
	case 0x3:
	case 0x2: con = ((int)Memory::Read_U32(new_addr) < (int)data); break;
	default:
		LogInfo("Bad Size");
		PanicAlert("Action Replay: Normal Code 3: Invalid Size %08x (%s)", size, code.name.c_str());
		return false;
	}

	*skip = !con; // set skip
	LogInfo("Skip set to %s", !con ? "False" : "True");

	switch (subtype)
	{
	case 0x0: *count = 1; break; // 1 line
	case 0x1: *count = 2; break; // 2 lines
	case 0x2: *count = -2; break; // all lines
	case 0x3: *count = -2; break; // While != : skip all codes ("infinite loop on the code" ?)
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code 3: Invalid subtype %08x (%s)", subtype, code.name.c_str());
		return false;
	}
	return true;
}

bool NormalCode_Type_4(u8 subtype, u32 addr, u32 data, int *count, bool *skip)
{
	u8 size = (addr >> 25) & 0x03;
	u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000);
	LogInfo("Size: %08x", size);
	LogInfo("Hardware Address: %08x", new_addr);
	bool con = true;
	switch (size)
	{
	case 0x0: con = ((char)Memory::Read_U8(new_addr) > (char)(data & 0xFF)); break;
	case 0x1: con = ((short)Memory::Read_U16(new_addr) > (short)(data & 0xFFFF)); break;
	case 0x3:
	case 0x2: con = ((int)Memory::Read_U32(new_addr) > (int)data); break;
	default:
		LogInfo("Bad Size");
		PanicAlert("Action Replay: Normal Code 4: Invalid Size %08x (%s)", size, code.name.c_str());
		return false;
	}

	*skip = !con; // set skip
	LogInfo("Skip set to %s", !con ? "False" : "True");

	switch (subtype)
	{
	case 0x0: *count = 1; break; // 1 line
	case 0x1: *count = 2; break; // 2 lines
	case 0x2: *count = -2; break; // all lines
	case 0x3: *count = -2; break; // While != : skip all codes ("infinite loop on the code" ?)
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code 4: Invalid subtype %08x (%s)", subtype, code.name.c_str());
		return false;
	}
	return true;
}

bool NormalCode_Type_5(u8 subtype, u32 addr, u32 data, int *count, bool *skip)
{
	u8 size = (addr >> 25) & 0x03;
	u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000);
	LogInfo("Size: %08x", size);
	LogInfo("Hardware Address: %08x", new_addr);
	bool con = true;
	switch (size)
	{
	case 0x0: con = (Memory::Read_U8(new_addr) < (data & 0xFF)); break;
	case 0x1: con = (Memory::Read_U16(new_addr) < (data & 0xFFFF)); break;
	case 0x3:
	case 0x2: con = (Memory::Read_U32(new_addr) < data); break;
	default:
		LogInfo("Bad Size");
		PanicAlert("Action Replay: Normal Code 5: Invalid Size %08x (%s)", size, code.name.c_str());
		return false;
	}

	*skip = !con; // set skip
	LogInfo("Skip set to %s", !con ? "False" : "True");

	switch (subtype)
	{
	case 0x0: *count = 1; break; // 1 line
	case 0x1: *count = 2; break; // 2 lines
	case 0x2: *count = -2; break; // all lines
	case 0x3: *count = -2; break; // While != : skip all codes ("infinite loop on the code" ?)
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code 5: Invalid subtype %08x (%s)", subtype, code.name.c_str());
		return false;
	}
	return true;
}

bool NormalCode_Type_6(u8 subtype, u32 addr, u32 data, int *count, bool *skip)
{
	u8 size = (addr >> 25) & 0x03;
	u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000);
	LogInfo("Size: %08x", size);
	LogInfo("Hardware Address: %08x", new_addr);
	bool con = true;
	switch (size)
	{
	case 0x0: con = (Memory::Read_U8(new_addr) > (data & 0xFF)); break;
	case 0x1: con = (Memory::Read_U16(new_addr) > (data & 0xFFFF)); break;
	case 0x3:
	case 0x2: con = (Memory::Read_U32(new_addr) > data); break;
	default:
		LogInfo("Bad Size");
		PanicAlert("Action Replay: Normal Code 6: Invalid Size %08x (%s)", size, code.name.c_str());
		return false;
	}

	*skip = !con; // set skip
	LogInfo("Skip set to %s", !con ? "False" : "True");

	switch (subtype)
	{
	case 0x0: *count = 1; break; // 1 line
	case 0x1: *count = 2; break; // 2 lines
	case 0x2: *count = -2; break; // all lines
	case 0x3: *count = -2; break; // While != : skip all codes ("infinite loop on the code" ?)
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code 6: Invalid subtype %08x (%s)", subtype, code.name.c_str());
		return false;
	}
	return true;
}

bool NormalCode_Type_7(u8 subtype, u32 addr, u32 data, int *count, bool *skip)
{
	u8 size = (addr >> 25) & 0x03;
	u32 new_addr = ((addr & 0x7FFFFF) | 0x80000000);
	LogInfo("Size: %08x", size);
	LogInfo("Hardware Address: %08x", new_addr);
	bool con = true;
	switch (size)
	{
	case 0x0: con = ((Memory::Read_U8(new_addr) & (data & 0xFF)) != 0); break;
	case 0x1: con = ((Memory::Read_U16(new_addr) & (data & 0xFFFF)) != 0); break;
	case 0x3:
	case 0x2: con = ((Memory::Read_U32(new_addr) & data) != 0); break;
	default:
		LogInfo("Bad Size");
		PanicAlert("Action Replay: Normal Code 7: Invalid Size %08x (%s)", size, code.name.c_str());
		return false;
	}

	*skip = !con; // set skip
	LogInfo("Skip set to %s", !con ? "False" : "True");

	switch (subtype)
	{
	case 0x0: *count = 1; break; // 1 line
	case 0x1: *count = 2; break; // 2 lines
	case 0x2: *count = -2; break; // all lines
	case 0x3: *count = -2; break; // While != : skip all codes ("infinite loop on the code" ?)
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code 7: Invalid subtype %08x (%s)", subtype, code.name.c_str());
		return false;
	}
	return true;
}

size_t GetCodeListSize()
{
	return arCodes.size();
}

ARCode GetARCode(size_t index)
{
	if (index > arCodes.size())
	{
		PanicAlert("GetARCode: Index is greater than ar code list size %i", index);
		return ARCode();
	}
	return arCodes[index];
}

void SetARCode_IsActive(bool active, size_t index)
{
	if (index > arCodes.size())
	{
		PanicAlert("SetARCode_IsActive: Index is greater than ar code list size %i", index);
		return;
	}
	arCodes[index].active = active;
	UpdateActiveList();
}

void UpdateActiveList()
{
	b_RanOnce = false;
	activeCodes.clear();
	for (size_t i = 0; i < arCodes.size(); i++)
	{
		if (arCodes[i].active)
			activeCodes.push_back(arCodes[i]);
	}
}

void EnableSelfLogging(bool enable)
{
	logSelf = enable;
}

const std::vector<std::string> &GetSelfLog()
{
	return arLog;
}

bool IsSelfLogging()
{
	return logSelf;
}

} // namespace ActionReplay
