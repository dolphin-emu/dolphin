// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/MathUtil.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

// dequantize table
const float m_dequantizeTable[] =
{
	1.0 / (1ULL <<  0), 1.0 / (1ULL <<  1), 1.0 / (1ULL <<  2), 1.0 / (1ULL <<  3),
	1.0 / (1ULL <<  4), 1.0 / (1ULL <<  5), 1.0 / (1ULL <<  6), 1.0 / (1ULL <<  7),
	1.0 / (1ULL <<  8), 1.0 / (1ULL <<  9), 1.0 / (1ULL << 10), 1.0 / (1ULL << 11),
	1.0 / (1ULL << 12), 1.0 / (1ULL << 13), 1.0 / (1ULL << 14), 1.0 / (1ULL << 15),
	1.0 / (1ULL << 16), 1.0 / (1ULL << 17), 1.0 / (1ULL << 18), 1.0 / (1ULL << 19),
	1.0 / (1ULL << 20), 1.0 / (1ULL << 21), 1.0 / (1ULL << 22), 1.0 / (1ULL << 23),
	1.0 / (1ULL << 24), 1.0 / (1ULL << 25), 1.0 / (1ULL << 26), 1.0 / (1ULL << 27),
	1.0 / (1ULL << 28), 1.0 / (1ULL << 29), 1.0 / (1ULL << 30), 1.0 / (1ULL << 31),
	(1ULL << 32), (1ULL << 31), (1ULL << 30), (1ULL << 29),
	(1ULL << 28), (1ULL << 27), (1ULL << 26), (1ULL << 25),
	(1ULL << 24), (1ULL << 23), (1ULL << 22), (1ULL << 21),
	(1ULL << 20), (1ULL << 19), (1ULL << 18), (1ULL << 17),
	(1ULL << 16), (1ULL << 15), (1ULL << 14), (1ULL << 13),
	(1ULL << 12), (1ULL << 11), (1ULL << 10), (1ULL <<  9),
	(1ULL <<  8), (1ULL <<  7), (1ULL <<  6), (1ULL <<  5),
	(1ULL <<  4), (1ULL <<  3), (1ULL <<  2), (1ULL <<  1),
};

// quantize table
const float m_quantizeTable[] =
{
	(1ULL <<  0), (1ULL <<  1), (1ULL <<  2), (1ULL <<  3),
	(1ULL <<  4), (1ULL <<  5), (1ULL <<  6), (1ULL <<  7),
	(1ULL <<  8), (1ULL <<  9), (1ULL << 10), (1ULL << 11),
	(1ULL << 12), (1ULL << 13), (1ULL << 14), (1ULL << 15),
	(1ULL << 16), (1ULL << 17), (1ULL << 18), (1ULL << 19),
	(1ULL << 20), (1ULL << 21), (1ULL << 22), (1ULL << 23),
	(1ULL << 24), (1ULL << 25), (1ULL << 26), (1ULL << 27),
	(1ULL << 28), (1ULL << 29), (1ULL << 30), (1ULL << 31),
	1.0 / (1ULL << 32), 1.0 / (1ULL << 31), 1.0 / (1ULL << 30), 1.0 / (1ULL << 29),
	1.0 / (1ULL << 28), 1.0 / (1ULL << 27), 1.0 / (1ULL << 26), 1.0 / (1ULL << 25),
	1.0 / (1ULL << 24), 1.0 / (1ULL << 23), 1.0 / (1ULL << 22), 1.0 / (1ULL << 21),
	1.0 / (1ULL << 20), 1.0 / (1ULL << 19), 1.0 / (1ULL << 18), 1.0 / (1ULL << 17),
	1.0 / (1ULL << 16), 1.0 / (1ULL << 15), 1.0 / (1ULL << 14), 1.0 / (1ULL << 13),
	1.0 / (1ULL << 12), 1.0 / (1ULL << 11), 1.0 / (1ULL << 10), 1.0 / (1ULL <<  9),
	1.0 / (1ULL <<  8), 1.0 / (1ULL <<  7), 1.0 / (1ULL <<  6), 1.0 / (1ULL <<  5),
	1.0 / (1ULL <<  4), 1.0 / (1ULL <<  3), 1.0 / (1ULL <<  2), 1.0 / (1ULL <<  1),
};

void Interpreter::Helper_Quantize(const u32 _Addr, const double _fValue, const EQuantizeType _quantizeType, const unsigned int _uScale)
{
	switch (_quantizeType)
	{
	case QUANTIZE_FLOAT:
		Memory::Write_U32(ConvertToSingleFTZ(*(u64*)&_fValue), _Addr);
		break;

	// used for THP player
	case QUANTIZE_U8:
		{
			float fResult = (float)_fValue * m_quantizeTable[_uScale];
			MathUtil::Clamp(&fResult, 0.0f, 255.0f);
			Memory::Write_U8((u8)fResult, _Addr);
		}
		break;

	case QUANTIZE_U16:
		{
			float fResult = (float)_fValue * m_quantizeTable[_uScale];
			MathUtil::Clamp(&fResult, 0.0f, 65535.0f);
			Memory::Write_U16((u16)fResult, _Addr);
		}
		break;

	case QUANTIZE_S8:
		{
			float fResult = (float)_fValue * m_quantizeTable[_uScale];
			MathUtil::Clamp(&fResult, -128.0f, 127.0f);
			Memory::Write_U8((u8)(s8)fResult, _Addr);
		}
		break;

	case QUANTIZE_S16:
		{
			float fResult = (float)_fValue * m_quantizeTable[_uScale];
			MathUtil::Clamp(&fResult, -32768.0f, 32767.0f);
			Memory::Write_U16((u16)(s16)fResult, _Addr);
		}
		break;

	default:
		_dbg_assert_msg_(POWERPC, 0, "PS dequantize - unknown type to read");
		break;
	}
}

float Interpreter::Helper_Dequantize(const u32 _Addr, const EQuantizeType _quantizeType, const unsigned int _uScale)
{
	// dequantize the value
	float fResult;
	switch (_quantizeType)
	{
	case QUANTIZE_FLOAT:
		{
			u32 dwValue = Memory::Read_U32(_Addr);
			fResult = *(float*)&dwValue;
		}
		break;

	case QUANTIZE_U8:
		fResult = static_cast<float>(Memory::Read_U8(_Addr)) * m_dequantizeTable[_uScale];
		break;

	case QUANTIZE_U16:
		fResult = static_cast<float>(Memory::Read_U16(_Addr)) * m_dequantizeTable[_uScale];
		break;

	case QUANTIZE_S8:
		fResult = static_cast<float>((s8)Memory::Read_U8(_Addr)) * m_dequantizeTable[_uScale];
		break;

		// used for THP player
	case QUANTIZE_S16:
		fResult = static_cast<float>((s16)Memory::Read_U16(_Addr)) * m_dequantizeTable[_uScale];
		break;

	default:
		_dbg_assert_msg_(POWERPC, 0, "PS dequantize - unknown type to read");
		fResult = 0;
		break;
	}
	return fResult;
}

void Interpreter::psq_l(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.I));
	const EQuantizeType ldType = gqr.ld_type;
	const unsigned int ldScale = gqr.ld_scale;
	const u32 EA = _inst.RA ?
		(m_GPR[_inst.RA] + _inst.SIMM_12) : (u32)_inst.SIMM_12;
	printf("psq_l at offset %d\n", _inst.SIMM_12);

	int c = 4;
	if (ldType == QUANTIZE_U8  || ldType == QUANTIZE_S8)
		c = 0x1;
	else if (ldType == QUANTIZE_U16 || ldType == QUANTIZE_S16)
		c = 0x2;

	if (_inst.W == 0)
	{
		float ps0 = Helper_Dequantize(EA,     ldType, ldScale);
		float ps1 = Helper_Dequantize(EA + c, ldType, ldScale);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			return;
		}
		rPS0(_inst.RS) = ps0;
		rPS1(_inst.RS) = ps1;
	}
	else
	{
		float ps0 = Helper_Dequantize(EA, ldType, ldScale);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			return;
		}
		rPS0(_inst.RS) = ps0;
		rPS1(_inst.RS) = 1.0f;
	}
}

void Interpreter::psq_lu(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.I));
	const EQuantizeType ldType = gqr.ld_type;
	const unsigned int ldScale = gqr.ld_scale;
	const u32 EA = m_GPR[_inst.RA] + _inst.SIMM_12;

	int c = 4;
	if (ldType == QUANTIZE_U8 || ldType == QUANTIZE_S8)
		c = 0x1;
	else if (ldType == QUANTIZE_U16 || ldType == QUANTIZE_S16)
		c = 0x2;

	if (_inst.W == 0)
	{
		float ps0 = Helper_Dequantize(EA,     ldType, ldScale);
		float ps1 = Helper_Dequantize(EA + c, ldType, ldScale);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			return;
		}
		rPS0(_inst.RS) = ps0;
		rPS1(_inst.RS) = ps1;
	}
	else
	{
		float ps0 = Helper_Dequantize(EA, ldType, ldScale);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			return;
		}
		rPS0(_inst.RS) = ps0;
		rPS1(_inst.RS) = 1.0f;
	}
	m_GPR[_inst.RA] = EA;
}

void Interpreter::psq_st(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.I));
	const EQuantizeType stType = gqr.st_type;
	const unsigned int stScale = gqr.st_scale;
	const u32 EA = _inst.RA ?
		(m_GPR[_inst.RA] + _inst.SIMM_12) : (u32)_inst.SIMM_12;

	int c = 4;
	if (stType == QUANTIZE_U8 || stType == QUANTIZE_S8)
		c = 0x1;
	else if (stType == QUANTIZE_U16 || stType == QUANTIZE_S16)
		c = 0x2;

	if (_inst.W == 0)
	{
		Helper_Quantize(EA,     rPS0(_inst.RS), stType, stScale);
		Helper_Quantize(EA + c, rPS1(_inst.RS), stType, stScale);
	}
	else
	{
		Helper_Quantize(EA, rPS0(_inst.RS), stType, stScale);
	}
}

void Interpreter::psq_stu(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.I));
	const EQuantizeType stType = gqr.st_type;
	const unsigned int stScale = gqr.st_scale;
	const u32 EA = m_GPR[_inst.RA] + _inst.SIMM_12;

	int c = 4;
	if (stType == QUANTIZE_U8 || stType == QUANTIZE_S8)
		c = 0x1;
	else if (stType == QUANTIZE_U16 || stType == QUANTIZE_S16)
		c = 0x2;

	if (_inst.W == 0)
	{
		Helper_Quantize(EA,     rPS0(_inst.RS), stType, stScale);
		Helper_Quantize(EA + c, rPS1(_inst.RS), stType, stScale);
	}
	else
	{
		Helper_Quantize(EA, rPS0(_inst.RS), stType, stScale);
	}
	if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
	{
		return;
	}
	m_GPR[_inst.RA] = EA;
}

void Interpreter::psq_lx(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.Ix));
	const EQuantizeType ldType = gqr.ld_type;
	const unsigned int ldScale = gqr.ld_scale;
	const u32 EA = _inst.RA ? (m_GPR[_inst.RA] + m_GPR[_inst.RB]) : m_GPR[_inst.RB];

	int c = 4;
	if (ldType == QUANTIZE_U8 || ldType == QUANTIZE_S8)
		c = 0x1;
	else if (ldType == QUANTIZE_U16 || ldType == QUANTIZE_S16)
		c = 0x2;

	if (_inst.Wx == 0)
	{
		float ps0 = Helper_Dequantize(EA,     ldType, ldScale);
		float ps1 = Helper_Dequantize(EA + c, ldType, ldScale);

		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			return;
		}

		rPS0(_inst.RS) = ps0;
		rPS1(_inst.RS) = ps1;
	}
	else
	{
		float ps0 = Helper_Dequantize(EA, ldType, ldScale);
		float ps1 = 1.0f;

		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			return;
		}

		rPS0(_inst.RS) = ps0;
		rPS1(_inst.RS) = ps1;
	}
}

void Interpreter::psq_stx(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.Ix));
	const EQuantizeType stType = gqr.st_type;
	const unsigned int stScale = gqr.st_scale;
	const u32 EA = _inst.RA ? (m_GPR[_inst.RA] + m_GPR[_inst.RB]) : m_GPR[_inst.RB];

	int c = 4;
	if (stType == QUANTIZE_U8 || stType == QUANTIZE_S8)
		c = 0x1;
	else if (stType == QUANTIZE_U16 || stType == QUANTIZE_S16)
		c = 0x2;

	if (_inst.Wx == 0)
	{
		Helper_Quantize(EA,     rPS0(_inst.RS), stType, stScale);
		Helper_Quantize(EA + c, rPS1(_inst.RS), stType, stScale);
	}
	else
	{
		Helper_Quantize(EA, rPS0(_inst.RS), stType, stScale);
	}
}

void Interpreter::psq_lux(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.Ix));
	const EQuantizeType ldType = gqr.ld_type;
	const unsigned int ldScale = gqr.ld_scale;
	const u32 EA = m_GPR[_inst.RA] + m_GPR[_inst.RB];

	int c = 4;
	if (ldType == QUANTIZE_U8 || ldType == QUANTIZE_S8)
		c = 0x1;
	else if (ldType == QUANTIZE_U16 || ldType == QUANTIZE_S16)
		c = 0x2;

	if (_inst.Wx == 0)
	{
		float ps0 = Helper_Dequantize(EA,     ldType, ldScale);
		float ps1 = Helper_Dequantize(EA + c, ldType, ldScale);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			return;
		}
		rPS0(_inst.RS) = ps0;
		rPS1(_inst.RS) = ps1;
	}
	else
	{
		float ps0 = Helper_Dequantize(EA, ldType, ldScale);
		if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
		{
			return;
		}
		rPS0(_inst.RS) = ps0;
		rPS1(_inst.RS) = 1.0f;
	}
	m_GPR[_inst.RA] = EA;
}

void Interpreter::psq_stux(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.Ix));
	const EQuantizeType stType = gqr.st_type;
	const unsigned int stScale = gqr.st_scale;
	const u32 EA = m_GPR[_inst.RA] + m_GPR[_inst.RB];

	int c = 4;
	if (stType == QUANTIZE_U8 || stType == QUANTIZE_S8)
		c = 0x1;
	else if (stType == QUANTIZE_U16 || stType == QUANTIZE_S16)
		c = 0x2;

	if (_inst.Wx == 0)
	{
		Helper_Quantize(EA,     rPS0(_inst.RS), stType, stScale);
		Helper_Quantize(EA + c, rPS1(_inst.RS), stType, stScale);
	}
	else
	{
		Helper_Quantize(EA, rPS0(_inst.RS), stType, stScale);
	}
	if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
	{
		return;
	}
	m_GPR[_inst.RA] = EA;

}  // namespace=======
