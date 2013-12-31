// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "x64Analyzer.h"

bool DisassembleMov(const unsigned char *codePtr, InstructionInfo *info)
{
	unsigned const char *startCodePtr = codePtr;
	u8 rex = 0;
	u8 codeByte = 0;
	u8 codeByte2 = 0;

	//Check for regular prefix
	info->operandSize = 4;
	info->zeroExtend = false;
	info->signExtend = false;
	info->hasImmediate = false;
	info->isMemoryWrite = false;

	u8 modRMbyte = 0;
	u8 sibByte = 0;
	bool hasModRM = false;

	int displacementSize = 0;

	if (*codePtr == 0x66)
	{
		info->operandSize = 2;
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
			info->operandSize = 8;
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
		info->regOperandReg = mrm.reg;
		if (mrm.mod < 3)
		{
			if (mrm.rm == 4)
			{
				//SIB byte
				sibByte = *codePtr++;
				info->scaledReg = (sibByte >> 3) & 7;
				info->otherReg = (sibByte & 7);
				if (rex & 2) info->scaledReg += 8;
				if (rex & 1) info->otherReg += 8;
			}
			else
			{
				//info->scaledReg =
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
		info->displacement = (s32)(s8)*codePtr;
	else
		info->displacement = *((s32 *)codePtr);
	codePtr += displacementSize;


	switch (codeByte)
	{
	// writes
	case 0xC6: // mem <- imm8
		{
			info->isMemoryWrite = true;
			info->hasImmediate = true;
			info->immediate = *codePtr;
			codePtr++; //move past immediate
		}
		break;

	case 0xC7: // mem <- imm16/32
		{
			info->isMemoryWrite = true;
			if (info->operandSize == 2)
			{
				info->hasImmediate = true;
				info->immediate = *(u16*)codePtr;
				codePtr += 2;
			}
			else if (info->operandSize == 4)
			{
				info->hasImmediate = true;
				info->immediate = *(u32*)codePtr;
				codePtr += 4;
			}
			else if (info->operandSize == 8)
			{
				info->zeroExtend = true;
				info->immediate = *(u32*)codePtr;
				codePtr += 4;
			}
		}

	case 0x88: // mem <- r8
		{
			info->isMemoryWrite = true;
			if (info->operandSize == 4)
			{
				info->operandSize = 1;
				break;
			}
			else
				return false;
			break;
		}

	case 0x89: // mem <- r16/32/64
		{
			info->isMemoryWrite = true;
			break;
		}

	case 0x0F: // two-byte escape
		{
			info->isMemoryWrite = false;
			switch (codeByte2)
			{
			case 0xB6: // movzx on byte
				info->zeroExtend = true;
				info->operandSize = 1;
				break;
			case 0xB7: // movzx on short
				info->zeroExtend = true;
				info->operandSize = 2;
				break;
			case 0xBE: // movsx on byte
				info->signExtend = true;
				info->operandSize = 1;
				break;
			case 0xBF: // movsx on short
				info->signExtend = true;
				info->operandSize = 2;
				break;
			default:
				return false;
			}
			break;
		}

	case 0x8A: // r8 <- mem
		{
			info->isMemoryWrite = false;
			if (info->operandSize == 4)
			{
				info->operandSize = 1;
				break;
			}
			else
				return false;
		}

	case 0x8B: // r16/32/64 <- mem
		{
			info->isMemoryWrite = false;
			break;
		}

		break;

	default:
		return false;
	}
	info->instructionSize = (int)(codePtr - startCodePtr);
	return true;
}
