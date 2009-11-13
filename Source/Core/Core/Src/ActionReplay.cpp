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

// -----------------------------------------------------------------------------------------
// Partial Action Replay code system implementation.
// Will never be able to support some AR codes - specifically those that patch the running
// Action Replay engine itself - yes they do exist!!!
// Action Replay actually is a small virtual machine with a limited number of commands.
// It probably is Turning complete - but what does that matter when AR codes can write
// actual PowerPC code...
// -----------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------------------------------
// Codes Types:
// (Unconditonal) Normal Codes (0): this one has subtypes inside
// (Conditional) Normal Codes (1 - 7): these just compare values and set the line skip info
// Zero Codes: any code with no address.  These codes are used to do special operations like memory copy, etc
// -------------------------------------------------------------------------------------------------------------

#include <string>
#include <vector>

#include "Common.h"
#include "StringUtil.h"
#include "HW/Memmap.h"
#include "ActionReplay.h"
#include "Core.h"
#include "ARDecrypt.h"
#include "LogManager.h"
#include "ConfigManager.h"

namespace ActionReplay
{
enum
{
    // Zero Code Types
	ZCODE_END      = 0x00,
	ZCODE_NORM     = 0x02, 
	ZCODE_ROW	   = 0x03, 
	ZCODE_04 = 0x04,

    // Conditonal Codes
	CONDTIONAL_IF_EQUAL                  = 0x01,
	CONDTIONAL_IF_NOT_EQUAL              = 0x02, 
	CONDTIONAL_IF_LESS_THAN_SIGNED       = 0x03,
	CONDTIONAL_IF_GREATER_THAN_SIGNED	 = 0x04,
	CONDTIONAL_IF_LESS_THAN_UNSIGNED	 = 0x05,
	CONDTIONAL_IF_GREATER_THAN_UNSIGNED  = 0x06,
	CONDTIONAL_IF_AND	                 = 0x07,

	// Data Types
	DATATYPE_8BIT	     = 0x00,
	DATATYPE_16BIT       = 0x01, 
	DATATYPE_32BIT       = 0x02, 
	DATATYPE_32BIT_FLOAT = 0x03,

    // Normal Code 0 Subtypes
	SUB_RAM_WRITE	    = 0x00,
	SUB_WRITE_POINTER   = 0x01, 
	SUB_ADD_CODE		= 0x02, 
	SUB_MASTER_CODE     = 0x03,
};

static std::vector<AREntry>::const_iterator iter;
static ARCode code;
static bool b_RanOnce = false;
static std::vector<ARCode> arCodes;
static std::vector<ARCode> activeCodes;
static bool logSelf = false;
static std::vector<std::string> arLog;

void LogInfo(const char *format, ...);
bool Subtype_RamWriteAndFill(u32 addr, u32 data);
bool Subtype_WriteToPointer(u32 addr, u32 data);
bool Subtype_AddCode(u32 addr, u32 data);
bool Subtype_MasterCodeAndWriteToCCXXXXXX(u32 addr, u32 data);
bool ZeroCode_FillAndSlide(u32 val_last, u32 addr, u32 data);
bool ZeroCode_MemoryCopy(u32 val_last, u32 addr, u32 data);
bool NormalCode(u8 subtype, u32 addr, u32 data);
bool ConditionalCode(u8 subtype, u32 addr, u32 data, int *pCount, bool *pSkip, int compareType);
bool SetLineSkip(int codetype, u8 subtype, bool *pSkip, bool skip, int *pCount);
bool CompareValues(u32 val1, u32 val2, int type);

// ----------------------
// AR Remote Functions
void LoadCodes(IniFile &ini, bool forceLoad)
{
	// Parses the Action Replay section of a game ini file.
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats 
	    && !forceLoad) 
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
					if (!forceLoad)
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
		if (LogManager::GetMaxLevel() >= LogTypes::LINFO || logSelf)
		{
			char* temp = (char*)alloca(strlen(format)+512);
			va_list args;
			va_start(args, format);
			CharArrayFromFormatV(temp, 512, format, args);
			va_end(args);
			INFO_LOG(ACTIONREPLAY, temp);

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
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats)
	{
		for (std::vector<ARCode>::iterator i = activeCodes.begin(); i != activeCodes.end(); ++i) 
		{
			if (i->active)
			{
				i->active = RunCode(*i);
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
		if (doFillNSlide)
		{
			doFillNSlide = false;
			LogInfo("Doing Fill And Slide");
			if (!ZeroCode_FillAndSlide(val_last, addr, data))
				return false;
			continue;
		}

		// Memory Copy
		if (doMemoryCopy)
		{
			doMemoryCopy = false;
			LogInfo("Doing Memory Copy");
			if (!ZeroCode_MemoryCopy(val_last, addr, data))
				return false;
			continue;
		}

		// ActionReplay program self modification codes
		if (addr >= 0x00002000 && addr < 0x00003000)
		{
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
				case ZCODE_END: // END OF CODES
					LogInfo("ZCode: End Of Codes");
					return true;
				case ZCODE_NORM: // Normal execution of codes
					// Todo: Set register 1BB4 to 0
					LogInfo("ZCode: Normal execution of codes, set register 1BB4 to 0 (zcode not supported)");
					break;
				case ZCODE_ROW: // Executes all codes in the same row
					// Todo: Set register 1BB4 to 1
					LogInfo("ZCode: Executes all codes in the same row, Set register 1BB4 to 1 (zcode not supported)");
					PanicAlert("Zero 3 code not supported");
					return false;
				case ZCODE_04: // Fill & Slide or Memory Copy
					if (((addr >> 25) & 0x03) == 0x3) 
					{
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
		if (type == 0x00)
		{
			if (!NormalCode(subtype, addr, data))
				return false;
		}
		else if (type >= 1 && type <= 7) 
		{
			cond = true;
			LogInfo("This Normal Code is a Conditional Code");
			if (!ConditionalCode(subtype, addr, data, &count, &skip, type))
				return false;
		}
		else
		{
			LogInfo("Bad Normal Code type");
			return false;
		}
	}

	if (b_RanOnce && cond)
		b_RanOnce = true;

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
	SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats = false;
	b_RanOnce = false;
	activeCodes.clear();
	for (size_t i = 0; i < arCodes.size(); i++)
	{
		if (arCodes[i].active)
			activeCodes.push_back(arCodes[i]);
	}
	SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableCheats = true;
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

// ----------------------
// Code Functions
bool Subtype_RamWriteAndFill(u32 addr, u32 data)
{
	u32 new_addr = (addr & 0x01FFFFFF) | 0x80000000; // real GC address
	u8 size = (addr >> 25) & 0x03;
	LogInfo("Hardware Address: %08x", new_addr);
	LogInfo("Size: %08x", size);
	switch (size)
	{
	case DATATYPE_8BIT:
		{
			LogInfo("8-bit Write");
			LogInfo("--------");
			u32 repeat = data >> 8;
			for (u32 i = 0; i <= repeat; i++) {
				Memory::Write_U8(data & 0xFF, new_addr + i);
				LogInfo("Wrote %08x to address %08x", data & 0xFF, new_addr + i);
			}
			LogInfo("--------");
			break;
		}

	case DATATYPE_16BIT:
		{
			LogInfo("16-bit Write");
			LogInfo("--------");
			u32 repeat = data >> 16;
			for (u32 i = 0; i <= repeat; i++) {
				Memory::Write_U16(data & 0xFFFF, new_addr + i * 2);
				LogInfo("Wrote %08x to address %08x", data & 0xFFFF, new_addr + i * 2);
			}
			LogInfo("--------");
			break;
		}
	case DATATYPE_32BIT_FLOAT:
	case DATATYPE_32BIT: // Dword write
		LogInfo("32bit Write");
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
	u32 new_addr = (addr & 0x01FFFFFF) | 0x80000000;
	u8 size = (addr >> 25) & 0x03;
	LogInfo("Hardware Address: %08x", new_addr);
	LogInfo("Size: %08x", size);
	switch (size)
	{
	case DATATYPE_8BIT:
		{
			LogInfo("Write 8-bit to pointer");
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

	case DATATYPE_16BIT:
		{
			LogInfo("Write 16-bit to pointer");
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
	case DATATYPE_32BIT_FLOAT:
	case DATATYPE_32BIT:
		LogInfo("Write 32-bit to pointer");
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
	// Used to incrment a value in memory
	u32 new_addr = (addr & 0x81FFFFFF);
	u8 size = (addr >> 25) & 0x03;
	LogInfo("Hardware Address: %08x", new_addr);
	LogInfo("Size: %08x", size);
	switch (size)
	{
	case DATATYPE_8BIT:
		LogInfo("8-bit Add");
		LogInfo("--------");
		Memory::Write_U8(Memory::Read_U8(new_addr) + data, new_addr);
		LogInfo("Wrote %08x to address %08x", Memory::Read_U8(new_addr) + (data & 0xFF), new_addr);
		LogInfo("--------");
		break;
	case DATATYPE_16BIT:
		LogInfo("16-bit Add");
		LogInfo("--------");
		Memory::Write_U16(Memory::Read_U16(new_addr) + data, new_addr);
		LogInfo("Wrote %08x to address %08x", Memory::Read_U16(new_addr) + (data & 0xFFFF), new_addr);
		LogInfo("--------");
		break;
	case DATATYPE_32BIT:
		LogInfo("32-bit Add");
		LogInfo("--------");
		Memory::Write_U32(Memory::Read_U32(new_addr) + data, new_addr);
		LogInfo("Wrote %08x to address %08x", Memory::Read_U32(new_addr) + data, new_addr);
		LogInfo("--------");
		break;
	case DATATYPE_32BIT_FLOAT:
		{
			LogInfo("32-bit floating Add");
			LogInfo("--------");

			u32 read = Memory::Read_U32(new_addr);
			float fread = *((float*)&read);
			fread += (float)data;
			u32 newval = *((u32*)&fread);
			Memory::Write_U32(newval, new_addr);
			LogInfo("Old Value %08x", read);
			LogInfo("Increment %08x", data);
			LogInfo("New value %08x", newval);
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

bool Subtype_MasterCodeAndWriteToCCXXXXXX(u32 addr, u32 data)
{
	u32 new_addr = (addr & 0x01FFFFFF) | 0x80000000;
	u8	mcode_type = (data & 0xFF0000) >> 16;
	u8  mcode_count = (data & 0xFF00) >> 8;
	u8  mcode_number = data & 0xFF;
	// code not yet implemented - TODO
	PanicAlert("Action Replay Error: Master Code and Write To CCXXXXXX not implemented (%s)", code.name.c_str());
	return false;
}

bool ZeroCode_FillAndSlide(u32 val_last, u32 addr, u32 data) // This needs more testing
{
	u32 new_addr = (val_last & 0x81FFFFFF);
	u8  size = (val_last >> 25) & 0x03;
	s16 addr_incr = (s16)(data & 0xFFFF);
	s8  val_incr = (s8)((data & 0xFF000000) >> 24);
	u8  write_num = (data & 0xFF0000) >> 16;
	u32 val = addr;
	u32 curr_addr = new_addr;

	LogInfo("Current Hardware Address: %08x", new_addr);
	LogInfo("Size: %08x", size);
	LogInfo("Write Num: %08x", write_num);
	LogInfo("Address Increment: %i", addr_incr);
	LogInfo("Value Increment: %i", val_incr);

	switch (size)
	{
		case DATATYPE_8BIT:
			LogInfo("8-bit Write");
			LogInfo("--------");
			for (int i = 0; i < write_num; i++) 
			{
				Memory::Write_U8(val & 0xFF, curr_addr);
				curr_addr += addr_incr;
				val += val_incr;
				LogInfo("Write %08x to address %08x", val & 0xFF, curr_addr);

				LogInfo("Value Update: %08x", val);
				LogInfo("Current Hardware Address Update: %08x", curr_addr);
			}
			LogInfo("--------");
			break;
		case DATATYPE_16BIT:
			LogInfo("16-bit Write");
			LogInfo("--------");
			for (int i=0; i < write_num; i++) {
				Memory::Write_U16(val & 0xFFFF, curr_addr);
				LogInfo("Write %08x to address %08x", val & 0xFFFF, curr_addr);
				curr_addr += addr_incr * 2;
				val += val_incr;
				LogInfo("Value Update: %08x", val);
				LogInfo("Current Hardware Address Update: %08x", curr_addr);
			}
			LogInfo("--------");
			break;
		case DATATYPE_32BIT:
			LogInfo("32-bit Write");
			LogInfo("--------");
			for (int i = 0; i < write_num; i++) {
				Memory::Write_U32(val, curr_addr);
				LogInfo("Write %08x to address %08x", val, curr_addr);
				curr_addr += addr_incr * 4;
				val += val_incr;
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

// Looks like this is new??
bool ZeroCode_MemoryCopy(u32 val_last, u32 addr, u32 data) // Has not been tested
{
	u32 addr_dest = val_last | 0x06000000;
	u32 addr_src = (addr & 0x01FFFFFF) | 0x80000000;
	u8 num_bytes = data & 0x7FFF;
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

bool NormalCode(u8 subtype, u32 addr, u32 data)
{
	switch (subtype)
	{
	case SUB_RAM_WRITE: // Ram write (and fill)
		LogInfo("Doing Ram Write And Fill");
		if (!Subtype_RamWriteAndFill(addr, data))
			return false;
		break;
	case SUB_WRITE_POINTER: // Write to pointer
		LogInfo("Doing Write To Pointer");
		if (!Subtype_WriteToPointer(addr, data))
			return false;
		break;
	case SUB_ADD_CODE: // Increment Value
		LogInfo("Doing Add Code");
		if (!Subtype_AddCode(addr, data))
			return false;
		break;
	case SUB_MASTER_CODE : // Master Code & Write to CCXXXXXX
		LogInfo("Doing Master Code And Write to CCXXXXXX (ncode not supported)");
		if (!Subtype_MasterCodeAndWriteToCCXXXXXX(addr, data))
			return false;
		break;
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code 0: Invalid Subtype %08x (%s)", subtype, code.name.c_str());
		return false;
	}
	return true;
}
bool ConditionalCode(u8 subtype, u32 addr, u32 data, int *pCount, bool *pSkip, int compareType)
{
	u8 size = (addr >> 25) & 0x03;
	u32 new_addr = ((addr & 0x01FFFFFF) | 0x80000000);
	LogInfo("Size: %08x", size);
	LogInfo("Hardware Address: %08x", new_addr);
	bool con = true;
	switch (size)
	{
	case DATATYPE_8BIT: con = CompareValues((u32)Memory::Read_U8(new_addr), (data & 0xFF), compareType); break;
	case DATATYPE_16BIT: con = CompareValues((u32)Memory::Read_U16(new_addr), (data & 0xFFFF), compareType); break;
	case DATATYPE_32BIT_FLOAT:
	case DATATYPE_32BIT: con = CompareValues(Memory::Read_U32(new_addr), data, compareType); break;
	default:
		LogInfo("Bad Size");
		PanicAlert("Action Replay: Conditional Code: Invalid Size %08x (%s)", size, code.name.c_str());
		return false;
	}

	return SetLineSkip(1, subtype, pSkip, con, pCount);
}
// ----------------------
// Internal Functions
bool SetLineSkip(int codetype, u8 subtype, bool *pSkip, bool skip, int *pCount)
{
	*pSkip = !skip; // set skip
	LogInfo("Skip set to %s", !skip ? "True" : "False");

	switch (subtype)
	{
	case 0x00: *pCount = 1; break; // Skip 1 line
	case 0x01: *pCount = 2; break; // Skip 2 lines
	case 0x02:  // skip all lines
	case 0x03: *pCount = -2; break; // While != : no idea the purpose of this case
	default:
		LogInfo("Bad Subtype");
		PanicAlert("Action Replay: Normal Code %i: Invalid subtype %08x (%s)", codetype, subtype, code.name.c_str());
		return false;
	}

	return true;
}
bool CompareValues(u32 val1, u32 val2, int type)
{
	switch(type)
	{
	case CONDTIONAL_IF_EQUAL:
		LogInfo("Type 1: If Equal");
		return (val1 == val2);
	case CONDTIONAL_IF_NOT_EQUAL:
		LogInfo("Type 2: If Not Equal");
		return (val1 != val2);
	case CONDTIONAL_IF_LESS_THAN_SIGNED:
		LogInfo("Type 3: If Less Than (Signed)");
		return ((int)val1 < (int)val2);
	case CONDTIONAL_IF_GREATER_THAN_SIGNED:
		LogInfo("Type 4: If Greater Than (Signed)");
		return ((int)val1 > (int)val2);
	case CONDTIONAL_IF_LESS_THAN_UNSIGNED:
		LogInfo("Type 5: If Less Than (Unsigned)");
		return (val1 < val2);
	case CONDTIONAL_IF_GREATER_THAN_UNSIGNED:
		LogInfo("Type 6: If Greater Than (Unsigned)");
		return (val1 > val2);
	case CONDTIONAL_IF_AND:
		LogInfo("Type 7: If And");
		return (val1 && val2);
	default: LogInfo("Unknown Compare type");
		PanicAlert("Action Replay: Invalid Normal Code Type %08x (%s)", type, code.name.c_str());
		return false;
	}
}
} // namespace ActionReplay