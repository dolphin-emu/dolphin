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
#include "StringUtil.h"
#include "IniFile.h"
#include "HW/Memmap.h"
#include "ActionReplay.h"

u32 cmd_addr;
u8 cmd;
u32 addr;
u32 data;
u8 subtype;
u8 w;
u8 type;
std::vector<AREntry>::const_iterator iter;
std::vector<ARCode> arCodes;
ARCode code;

void LoadActionReplayCodes(IniFile &ini) 
{
	std::vector<std::string> lines;
	ARCode currentCode;
	arCodes.clear();

	if (!ini.GetLines("ActionReplay", lines)) return;

	for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
	{
		std::string line = *it;

		std::vector<std::string> pieces;
		SplitString(line, " ", pieces);
		if (pieces.size() == 2 && pieces[0].size() == 8 && pieces[1].size() == 8)
		{
			// Smells like a decrypted Action Replay code, great! Decode!
			AREntry op;
			bool success = TryParseUInt(std::string("0x") + pieces[0], &op.cmd_addr);
   			success |= TryParseUInt(std::string("0x") + pieces[1], &op.value);
			if (!success)
				PanicAlert("Invalid AR code line: %s", line.c_str());
			else
				currentCode.ops.push_back(op);
		}
		else
		{
			SplitString(line, "-", pieces);
			if (pieces.size() == 3 && pieces[0].size() == 4 && pieces[1].size() == 4 && pieces[2].size() == 4) 
			{
				// Encrypted AR code
				PanicAlert("Dolphin does not yet support encrypted AR codes.");
			}
			else if (line.size() > 1)
			{
				// OK, name line. This is the start of a new code. Push the old one, prepare the new one.
				if (currentCode.ops.size())
					arCodes.push_back(currentCode);
				currentCode.name = "(invalid)";
				currentCode.ops.clear();

				if (line[0] == '+')
				{ 
					// Active code - name line.
					line = StripSpaces(line.substr(1));
					currentCode.name = line;
					currentCode.active = true;
				}
				else
				{
					// Inactive code.
					currentCode.name = line;
					currentCode.active = false;
				}
			}
		}
	}

	// Handle the last code correctly.
	if (currentCode.ops.size())
		arCodes.push_back(currentCode);
}


// The mechanism is slightly different than what the real AR uses, so there may be compatibility problems.
// For example, some authors have created codes that add features to AR. Hacks for popular ones can be added here,
// but the problem is not generally solvable.
void RunActionReplayCode(const ARCode &arcode, bool nowIsBootup) {
	code = arcode;
	for (iter = code.ops.begin(); iter != code.ops.end(); ++iter) 
	{
		cmd_addr = iter->cmd_addr;
		cmd = iter->cmd_addr>>24;
		addr = iter->cmd_addr;
		data = iter->value;
		subtype = ((cmd_addr >> 30) & 0x03);
		w = (cmd - ((cmd >> 4) << 4));
		type = ((cmd_addr >> 27) & 0x07);

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
		if (iter == code.ops.begin() && cmd == 1) continue;

		// SubType selector
		switch(subtype)
		{
			case 0x0: // Ram write (and fill)
				{
					DoARSubtype_RamWriteAndFill(); continue;
				}
			case 0x1: // Write to pointer
				{
					DoARSubtype_WriteToPointer(); continue;
				}
			case 0x2: // Add code
				{
					DoARSubtype_AddCode(); continue;
				}
			case 0x3: // Master Code & Write to CCXXXXXX
				{
					DoARSubtype_MasterCodeAndWriteToCCXXXXXX(); continue;// TODO: This is not implemented yet
				}
			default: // non-specific z codes (hacks)
				{
					DoARSubtype_Other(); continue;
				}
		}
	}
}

void DoARSubtype_RamWriteAndFill()
{
	if(w < 0x8) // Check the value W in 0xZWXXXXXXX
	{
		u32 new_addr = ( (addr & 0x01FFFFFF) | 0x80000000);
		switch ((addr >> 25) & 0x03) 
		{
			case 0x00: // Byte write
				{
					u8 repeat = data >> 8;
					for (int i = 0; i <= repeat; i++) {
						Memory::Write_U8(data & 0xFF, new_addr + i);
					}
					break;
				}

			case 0x01: // Short write
				{
					u16 repeat = data >> 16;
					for (int i = 0; i <= repeat; i++) {
						Memory::Write_U16(data & 0xFFFF, new_addr + i * 2);
					}
					break;
				}


			case 0x02: // Dword write
				{
					Memory::Write_U32(data, new_addr);
					break;
				}
			default: break; // TODO(Omega): maybe add a PanicAlert here?
		}
	}
}
void DoARSubtype_WriteToPointer()
{
	if(w < 0x8)
	{
		u32 new_addr = ( addr | 0x80000000);
		switch ((addr >> 25) & 0x03) 
		{
			case 0x00: // Byte write to pointer [40]
				{
					u32 ptr = Memory::Read_U32(new_addr);
					u8 thebyte = data & 0xFF;
					u32 offset = data >> 8;
					Memory::Write_U8(thebyte, ptr + offset);
					break;
				}

			case 0x01: // Short write to pointer [42]
				{
					u32 ptr = Memory::Read_U32(new_addr);
					u16 theshort = data & 0xFFFF;
					u32 offset = (data >> 16) << 1;
					Memory::Write_U16(theshort, ptr + offset);
					break;
				}

			case 0x02: // Dword write to pointer [44]
				{
					Memory::Write_U32(data, Memory::Read_U32(new_addr)); break;
				}

			default: PanicAlert("AR Method Error (Write To Pointer): w = %08x, addr = %08x",w,addr);
		}
	}
}

void DoARSubtype_AddCode()
{
	if(w < 0x8)
	{
		u32 new_addr = ( (addr & 0x01FFFFFF) | 0x81FFFFFF);
		switch((addr >> 25) & 0x03)
		{
		case 0x0: // Byte add
			{
				Memory::Write_U8(Memory::Read_U8(new_addr) + (data & 0xFF), new_addr); break;
			}
		case 0x1: // Short add
			{
				Memory::Write_U16(Memory::Read_U16(new_addr) + (data & 0xFFFF), new_addr); break;
			}
		case 0x2: // DWord add
			{
				Memory::Write_U32(Memory::Read_U32(new_addr) + data, new_addr); break;
			}
		case 0x3: // Float add (not working?)
			{
				union { u32 u; float f;} fu, d;
				fu.u = Memory::Read_U32(new_addr);
				d.u = data;
				fu.f += data;
				Memory::Write_U32(fu.u, new_addr);
				break;
			}
		default: break;
		}
	}
}

void DoARSubtype_MasterCodeAndWriteToCCXXXXXX()
{
	// code not yet implemented - TODO

	//if(w < 0x8)
	//{
	//	u32 new_addr = (addr | 0x80000000);
	//	switch((new_addr >> 25) & 0x03)
	//	{
	//	case 0x2:
	//		{

	//		}
	//	}
	//}
}

void DoARSubtype_Other()
{
					switch (cmd & 0xFE) 
				 {
					case 0x90: if (Memory::Read_U32(addr) == data) return; // IF 32 bit equal, exit
					case 0x08: // IF 8 bit equal, execute next opcode
					case 0x48: // (double)
						{
							if (Memory::Read_U16(addr) != (data & 0xFFFF)) {
								if (++iter == code.ops.end()) return;
								if (cmd == 0x48) if (++iter == code.ops.end()) return;
							}
							break;
						}
					case 0x0A: // IF 16 bit equal, execute next opcode
					case 0x4A: // (double)
						{
							if (Memory::Read_U16(addr) != (data & 0xFFFF)) {
								if (++iter == code.ops.end()) return;
								if (cmd == 0x4A) if (++iter == code.ops.end()) return;
							}
							break;
						}
					case 0x0C:  // IF 32 bit equal, execute next opcode
					case 0x4C:  // (double)
						{
							if (Memory::Read_U32(addr) != data) {
								if (++iter == code.ops.end()) return;
								if (cmd == 0x4C) if (++iter == code.ops.end()) return;
							}
							break;
						}
					case 0x10:  // IF NOT 8 bit equal, execute next opcode
					case 0x50:  // (double)
						{
							if (Memory::Read_U8(addr) == (data & 0xFF)) {
								if (++iter == code.ops.end()) return;
								if (cmd == 0x50) if (++iter == code.ops.end()) return;
							}
							break;
						}
					case 0x12:  // IF NOT 16 bit equal, execute next opcode
					case 0x52:  // (double)
						{
							if (Memory::Read_U16(addr) == (data & 0xFFFF)) {
								if (++iter == code.ops.end()) return;
								if (cmd == 0x52) if (++iter == code.ops.end()) return;
							}
							break;
						}
					case 0x14:  // IF NOT 32 bit equal, execute next opcode
					case 0x54:  // (double)
						{
							if (Memory::Read_U32(addr) == data) {
								if (++iter == code.ops.end()) return;
								if (cmd == 0x54) if (++iter == code.ops.end()) return;
							}
							break;
						}
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
					default: PanicAlert("Unknown Action Replay command %02x (%08x %08x)", cmd, iter->cmd_addr, iter->value); break;
				}
}