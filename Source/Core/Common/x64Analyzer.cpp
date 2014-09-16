// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/x64Analyzer.h"

bool DisassembleMov(const unsigned char *codePtr, InstructionInfo *info)
{
	unsigned const char *startCodePtr = codePtr;
	u8 rex = 0;
	u32 opcode;
	int opcode_length;

	//Check for regular prefix
	info->operandSize = 4;
	info->zeroExtend = false;
	info->signExtend = false;
	info->hasImmediate = false;
	info->isMemoryWrite = false;
	info->byteSwap = false;

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

	opcode = *codePtr++;
	opcode_length = 1;
	if (opcode == 0x0F)
	{
		opcode = (opcode << 8) | *codePtr++;
		opcode_length = 2;
		if ((opcode & 0xFB) == 0x38)
		{
			opcode = (opcode << 8) | *codePtr++;
			opcode_length = 3;
		}
	}

	switch (opcode_length)
	{
		case 1:
			if ((opcode & 0xF0) == 0x80 ||
			    ((opcode & 0xF8) == 0xC0 && (opcode & 0x0E) != 0x02))
			{
				modRMbyte = *codePtr++;
				hasModRM = true;
			}
			break;
		case 2:
			if (((opcode & 0xF0) == 0x00 && (opcode & 0x0F) >= 0x04 && (opcode & 0x0D) != 0x0D) ||
			    ((opcode & 0xF0) == 0xA0 && (opcode & 0x07) <= 0x02) ||
			    (opcode & 0xF0) == 0x30 ||
			    (opcode & 0xFF) == 0x77 ||
			    (opcode & 0xF0) == 0x80 ||
			    (opcode & 0xF8) == 0xC8)
			{
				// No mod R/M byte
			}
			else
			{
				modRMbyte = *codePtr++;
				hasModRM = true;
			}
			break;
		case 3:
			// TODO: support more 3-byte opcode instructions
			if ((opcode & 0xFE) == 0xF0)
			{
				modRMbyte = *codePtr++;
				hasModRM = true;
			}
			break;
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
		info->displacement = *((s32*)codePtr);
	codePtr += displacementSize;

	switch (opcode)
	{
	case 0xC6: // mem <- imm8
		info->isMemoryWrite = true;
		info->hasImmediate = true;
		info->immediate = *codePtr;
		info->operandSize = 1;
		codePtr++;
		break;

	case 0xC7: // mem <- imm16/32
		info->isMemoryWrite = true;
		switch (info->operandSize)
		{
		case 2:
			info->hasImmediate = true;
			info->immediate = *(u16*)codePtr;
			codePtr += 2;
			break;

		case 4:
			info->hasImmediate = true;
			info->immediate = *(u32*)codePtr;
			codePtr += 4;
			break;

		case 8:
			info->zeroExtend = true;
			info->immediate = *(u32*)codePtr;
			codePtr += 4;
			break;
		}
		break;

	case 0x88: // mem <- r8
		info->isMemoryWrite = true;
		if (info->operandSize != 4)
		{
			return false;
		}
		info->operandSize = 1;
		break;

	case 0x89: // mem <- r16/32/64
		info->isMemoryWrite = true;
		break;

	case 0x8A: // r8 <- mem
		if (info->operandSize != 4)
		{
			return false;
		}
		info->operandSize = 1;
		break;

	case 0x8B: // r16/32/64 <- mem
		break;

	case 0x0FB6: // movzx on byte
		info->zeroExtend = true;
		info->operandSize = 1;
		break;

	case 0x0FB7: // movzx on short
		info->zeroExtend = true;
		info->operandSize = 2;
		break;

	case 0x0FBE: // movsx on byte
		info->signExtend = true;
		info->operandSize = 1;
		break;

	case 0x0FBF: // movsx on short
		info->signExtend = true;
		info->operandSize = 2;
		break;

	case 0x0F38F0: // movbe read
		info->byteSwap = true;
		break;

	case 0x0F38F1: // movbe write
		info->byteSwap = true;
		info->isMemoryWrite = true;
		break;

	default:
		return false;
	}
	info->instructionSize = (int)(codePtr - startCodePtr);
	return true;
}
