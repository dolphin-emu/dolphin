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

#include "x64Analyzer.h"

bool DisassembleMov(const unsigned char *codePtr, InstructionInfo &info, int accessType)
{
	unsigned const char *startCodePtr = codePtr;
	u8 rex = 0;
	u8 codeByte = 0;
	u8 codeByte2 = 0;
	
	//Check for regular prefix
	info.operandSize = 4;
	info.zeroExtend = false;
	info.signExtend = false;
	info.hasImmediate = false;
	info.isMemoryWrite = false;

	u8 modRMbyte = 0;
	u8 sibByte = 0;
    bool hasModRM = false;

	int displacementSize = 0;

	if (*codePtr == 0x66)
	{
		info.operandSize = 2;
		codePtr++;
	} 
	else if (*codePtr == 0x67)
	{
		codePtr++;
	}

	//Check for REX prefix
	if ((*codePtr & 0xF0) == 0x40)
	{
		rex = *codePtr;
		if (rex & 8) //REX.W
		{
			info.operandSize = 8;
		}
		codePtr++;
	}

	codeByte = *codePtr++;

    // Skip two-byte opcode byte 
    bool twoByte = false; 
    if(codeByte == 0x0F) 
    { 
        twoByte = true; 
		codeByte2 = *codePtr++;
    } 

	if (!twoByte)
	{
        if ((codeByte & 0xF0) == 0x80 || 
            ((codeByte & 0xF8) == 0xC0 && (codeByte & 0x0E) != 0x02))
		{
			modRMbyte = *codePtr++;
			hasModRM = true;
		}
	}
	else
	{
        if (((codeByte2 & 0xF0) == 0x00 && (codeByte2 & 0x0F) >= 0x04 && (codeByte2 & 0x0D) != 0x0D) || 
            (codeByte2 & 0xF0) == 0x30 || 
            codeByte2 == 0x77 || 
            (codeByte2 & 0xF0) == 0x80 || 
            ((codeByte2 & 0xF0) == 0xA0 && (codeByte2 & 0x07) <= 0x02) || 
            (codeByte2 & 0xF8) == 0xC8) 
        { 
            // No mod R/M byte 
        } 
        else 
        { 
			modRMbyte = *codePtr++;
			hasModRM = true;
        } 
	}

	if (hasModRM)
	{
		ModRM mrm(modRMbyte, rex);
		info.regOperandReg = mrm.reg;
		if (mrm.mod < 3)
		{
			if (mrm.rm == 4)
			{
				//SIB byte
				sibByte = *codePtr++;
				info.scaledReg = (sibByte >> 3) & 7;
				info.otherReg = (sibByte & 7);
				if (rex & 2) info.scaledReg += 8;
				if (rex & 1) info.otherReg += 8;
			}
			else
			{
				//info.scaledReg = 
			}
		}
		if (mrm.mod == 1 || mrm.mod == 2)
		{
			if (mrm.mod == 1)
				displacementSize = 1;
			else
				displacementSize = 4;
		}
	}

	if (displacementSize == 1)
		info.displacement = (s32)(s8)*codePtr;
	else
		info.displacement = *((s32 *)codePtr);
	codePtr += displacementSize;

	
	if (accessType == 1)
	{
		info.isMemoryWrite = true;
		//Write access
		switch (codeByte)
		{
		case MOVE_8BIT: //move 8-bit immediate
			{
				info.hasImmediate = true;
				info.immediate = *codePtr;
				codePtr++; //move past immediate
			}
			break;

		case MOVE_16_32BIT: //move 16 or 32-bit immediate, easiest case for writes
			{
				if (info.operandSize == 2)
				{
					info.hasImmediate = true;
					info.immediate = *(u16*)codePtr;
					codePtr += 2;
				}
				else if (info.operandSize == 4)
				{
					info.hasImmediate = true;
					info.immediate = *(u32*)codePtr;
					codePtr += 4;
				}
				else if (info.operandSize == 8)
				{
					info.zeroExtend = true;
					info.immediate = *(u32*)codePtr;
					codePtr += 4;
				}
			}
			break;
		case MOVE_REG_TO_MEM: //move reg to memory
			break;

		default:
			PanicAlert("Unhandled disasm case in write handler!\n\nPlease implement or avoid.");
			return false;
		}
	}
	else
	{
		// Memory read

		//mov eax, dword ptr [rax]   == 8b 00
		switch (codeByte)
		{
		case 0x0F:
			switch (codeByte2)
			{
			case MOVZX_BYTE: //movzx on byte
				info.zeroExtend = true;
				info.operandSize = 1;
				break;
			case MOVZX_SHORT: //movzx on short
				info.zeroExtend = true;
				info.operandSize = 2;
				break;
			case MOVSX_BYTE: //movsx on byte
				info.signExtend = true;
				info.operandSize = 1;
				break;
			case MOVSX_SHORT: //movsx on short
				info.signExtend = true;
				info.operandSize = 2;
				break;
			default:
				return false;
			}
			break;
		case 0x8a: 
			if (info.operandSize == 4)
			{
				info.operandSize = 1;
				break;
			}
			else 
				return false;
		case 0x8b: 
			break; //it's OK don't need to do anything
		default:
			return false;
		}
	}
	info.instructionSize = (int)(codePtr - startCodePtr);
	return true;
}
