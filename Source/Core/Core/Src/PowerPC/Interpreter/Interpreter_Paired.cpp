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

#include <math.h>
#include "Interpreter.h"
#include "../../HW/Memmap.h"

// dequantize table
const float m_dequantizeTable[] =
{
	1.0 / (1 <<  0),
	1.0 / (1 <<  1),
	1.0 / (1 <<  2),
	1.0 / (1 <<  3),
	1.0 / (1 <<  4),
	1.0 / (1 <<  5),
	1.0 / (1 <<  6),
	1.0 / (1 <<  7),
	1.0 / (1 <<  8),
	1.0 / (1 <<  9),
	1.0 / (1 << 10),
	1.0 / (1 << 11),
	1.0 / (1 << 12),
	1.0 / (1 << 13),
	1.0 / (1 << 14),
	1.0 / (1 << 15),
	1.0 / (1 << 16),
	1.0 / (1 << 17),
	1.0 / (1 << 18),
	1.0 / (1 << 19),
	1.0 / (1 << 20),
	1.0 / (1 << 21),
	1.0 / (1 << 22),
	1.0 / (1 << 23),
	1.0 / (1 << 24),
	1.0 / (1 << 25),
	1.0 / (1 << 26),
	1.0 / (1 << 27),
	1.0 / (1 << 28),
	1.0 / (1 << 29),
	1.0 / (1 << 30),
	1.0 / (1 << 31),
	(1ULL << 32),
	(1 << 31),
	(1 << 30),
	(1 << 29),
	(1 << 28),
	(1 << 27),
	(1 << 26),
	(1 << 25),
	(1 << 24),
	(1 << 23),
	(1 << 22),
	(1 << 21),
	(1 << 20),
	(1 << 19),
	(1 << 18),
	(1 << 17),
	(1 << 16),
	(1 << 15),
	(1 << 14),
	(1 << 13),
	(1 << 12),
	(1 << 11),
	(1 << 10),
	(1 <<  9),
	(1 <<  8),
	(1 <<  7),
	(1 <<  6),
	(1 <<  5),
	(1 <<  4),
	(1 <<  3),
	(1 <<  2),
	(1 <<  1),
}; 

// quantize table
const float m_quantizeTable[] =
{
	(1 <<  0),
	(1 <<  1),
	(1 <<  2),
	(1 <<  3),
	(1 <<  4),
	(1 <<  5),
	(1 <<  6),
	(1 <<  7),
	(1 <<  8),
	(1 <<  9),
	(1 << 10),
	(1 << 11),
	(1 << 12),
	(1 << 13),
	(1 << 14),
	(1 << 15),
	(1 << 16),
	(1 << 17),
	(1 << 18),
	(1 << 19),
	(1 << 20),
	(1 << 21),
	(1 << 22),
	(1 << 23),
	(1 << 24),
	(1 << 25),
	(1 << 26),
	(1 << 27),
	(1 << 28),
	(1 << 29),
	(1 << 30),
	(1 << 31),
	1.0 / (1ULL << 32),
	1.0 / (1 << 31),
	1.0 / (1 << 30),
	1.0 / (1 << 29),
	1.0 / (1 << 28),
	1.0 / (1 << 27),
	1.0 / (1 << 26),
	1.0 / (1 << 25),
	1.0 / (1 << 24),
	1.0 / (1 << 23),
	1.0 / (1 << 22),
	1.0 / (1 << 21),
	1.0 / (1 << 20),
	1.0 / (1 << 19),
	1.0 / (1 << 18),
	1.0 / (1 << 17),
	1.0 / (1 << 16),
	1.0 / (1 << 15),
	1.0 / (1 << 14),
	1.0 / (1 << 13),
	1.0 / (1 << 12),
	1.0 / (1 << 11),
	1.0 / (1 << 10),
	1.0 / (1 <<  9),
	1.0 / (1 <<  8),
	1.0 / (1 <<  7),
	1.0 / (1 <<  6),
	1.0 / (1 <<  5),
	1.0 / (1 <<  4),
	1.0 / (1 <<  3),
	1.0 / (1 <<  2),
	1.0 / (1 <<  1),
}; 

template <class T>
inline T CLAMP(T a, T bottom, T top) {
	if (a > top) return top;
	if (a < bottom) return bottom;
	return a;
}
void CInterpreter::Helper_Quantize(const u32 _Addr, const float _fValue, 
							  const EQuantizeType _quantizeType, const unsigned int _uScale)
{
	switch(_quantizeType) 
	{
	case QUANTIZE_FLOAT:
		Memory::Write_U32(*(u32*)&_fValue,_Addr);
		break;

	// used for THP player
	case QUANTIZE_U8:
		{
			float fResult = CLAMP(_fValue * m_quantizeTable[_uScale], 0.0f, 255.0f);
			Memory::Write_U8((u8)fResult, _Addr); 
		}
		break;

	case QUANTIZE_U16:
		{
			float fResult = CLAMP(_fValue * m_quantizeTable[_uScale], 0.0f, 65535.0f);
			Memory::Write_U16((u16)fResult, _Addr); 
		}
		break;

	case QUANTIZE_S8:
		{
			float fResult = CLAMP(_fValue * m_quantizeTable[_uScale], -128.0f, 127.0f);
			Memory::Write_U8((u8)(s8)fResult, _Addr); 
		}
		break;

	case QUANTIZE_S16:
		{
			float fResult = CLAMP(_fValue * m_quantizeTable[_uScale], -32768.0f, 32767.0f);
			Memory::Write_U16((u16)(s16)fResult, _Addr); 
		}
		break;

	default:
		_dbg_assert_msg_(GEKKO,0,"PS dequantize","Unknown type to read");
		break;
	}
}

float CInterpreter::Helper_Dequantize(const u32 _Addr, const EQuantizeType _quantizeType, 
								const unsigned int _uScale)
{
	// dequantize the value
	float fResult;
	switch(_quantizeType)
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
		_dbg_assert_msg_(GEKKO,0,"PS dequantize","Unknown type to read");
		fResult = 0;
		break;
	}

	return fResult;
}

void CInterpreter::psq_l(UGeckoInstruction _inst) 
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.I));
	const EQuantizeType ldType = static_cast<EQuantizeType>(gqr.LD_TYPE);
	const unsigned int ldScale = gqr.LD_SCALE;
	const u32 EA = _inst.RA ? (m_GPR[_inst.RA] + _inst.SIMM_12) : _inst.SIMM_12;

	int c = 4;
	if ((ldType == QUANTIZE_U8)  || (ldType == QUANTIZE_S8))  c = 0x1;
	if ((ldType == QUANTIZE_U16) || (ldType == QUANTIZE_S16)) c = 0x2;

	if (_inst.W == 0)
	{
		rPS0(_inst.RS) = Helper_Dequantize(EA,   ldType, ldScale);
		rPS1(_inst.RS) = Helper_Dequantize(EA+c, ldType, ldScale);
	}
	else
	{
		rPS0(_inst.RS) = Helper_Dequantize(EA,   ldType, ldScale);
		rPS1(_inst.RS) = 1.0f;
	}
}

void CInterpreter::psq_lu(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.I));
	const EQuantizeType ldType = static_cast<EQuantizeType>(gqr.LD_TYPE);
	const unsigned int ldScale = gqr.LD_SCALE;
	const u32 EA = m_GPR[_inst.RA] + _inst.SIMM_12;

	int c = 4;
	if ((ldType == 4) || (ldType == 6)) c = 0x1;
	if ((ldType == 5) || (ldType == 7)) c = 0x2;

	if (_inst.W == 0)
	{
		rPS0(_inst.RS) = Helper_Dequantize( EA,   ldType, ldScale );
		rPS1(_inst.RS) = Helper_Dequantize( EA+c, ldType, ldScale );
	}
	else
	{
		rPS0(_inst.RS) = Helper_Dequantize( EA,   ldType, ldScale );
		rPS1(_inst.RS) = 1.0f;
	}
	m_GPR[_inst.RA] = EA;
}

void CInterpreter::psq_st(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.I));
	const EQuantizeType stType = static_cast<EQuantizeType>(gqr.ST_TYPE);
	const unsigned int stScale = gqr.ST_SCALE;
	const u32 EA = _inst.RA ? (m_GPR[_inst.RA] + _inst.SIMM_12) : _inst.SIMM_12;

	int c = 4;
	if ((stType == 4) || (stType == 6)) c = 0x1;
	if ((stType == 5) || (stType == 7)) c = 0x2;

	if (_inst.W == 0)
	{
		Helper_Quantize( EA,   (float)rPS0(_inst.RS), stType, stScale );
		Helper_Quantize( EA+c, (float)rPS1(_inst.RS), stType, stScale );
	}
	else
	{
		Helper_Quantize( EA,   (float)rPS0(_inst.RS), stType, stScale );
	}
}

void CInterpreter::psq_stu(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.I));
	const EQuantizeType stType = static_cast<EQuantizeType>(gqr.ST_TYPE);
	const unsigned int stScale = gqr.ST_SCALE;
	const u32 EA = m_GPR[_inst.RA] + _inst.SIMM_12;

	int c = 4;
	if ((stType == 4) || (stType == 6)) c = 0x1;
	if ((stType == 5) || (stType == 7)) c = 0x2;

	if (_inst.W == 0)
	{
		Helper_Quantize(EA,   (float)rPS0(_inst.RS), stType, stScale);
		Helper_Quantize(EA+c, (float)rPS1(_inst.RS), stType, stScale);
	}
	else
	{
		Helper_Quantize(EA,   (float)rPS0(_inst.RS), stType, stScale);
	}
	m_GPR[_inst.RA] = EA;
}

void CInterpreter::psq_lx(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.Ix));
	const EQuantizeType ldType = static_cast<EQuantizeType>(gqr.LD_TYPE);
	const unsigned int ldScale = gqr.LD_SCALE;
	const u32 EA = _inst.RA ? (m_GPR[_inst.RA] + m_GPR[_inst.RB]) : m_GPR[_inst.RB];

	int c = 4;
	if ((ldType == 4) || (ldType == 6)) c = 0x1;
	if ((ldType == 5) || (ldType == 7)) c = 0x2;

	if (_inst.Wx == 0)
	{
		rPS0(_inst.RS) = Helper_Dequantize( EA,   ldType, ldScale );
		rPS1(_inst.RS) = Helper_Dequantize( EA+c, ldType, ldScale );
	}
	else
	{
		rPS0(_inst.RS) = Helper_Dequantize( EA, ldType, ldScale );
		rPS1(_inst.RS) = 1.0f;
	}
}

void CInterpreter::psq_stx(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.Ix));
	const EQuantizeType stType = static_cast<EQuantizeType>(gqr.ST_TYPE);
	const unsigned int stScale = gqr.ST_SCALE;
	const u32 EA = _inst.RA ? (m_GPR[_inst.RA] + m_GPR[_inst.RB]) : m_GPR[_inst.RB];

	int c = 4;
	if ((stType == 4) || (stType == 6)) c = 0x1;
	if ((stType == 5) || (stType == 7)) c = 0x2;

	if (_inst.Wx == 0)
	{
		Helper_Quantize(EA,   (float)rPS0(_inst.RS), stType, stScale);
		Helper_Quantize(EA+c, (float)rPS1(_inst.RS), stType, stScale);
	}
	else
	{
		Helper_Quantize(EA,   (float)rPS0(_inst.RS), stType, stScale);
	}
}

void CInterpreter::psq_lux(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.Ix));
	const EQuantizeType ldType = static_cast<EQuantizeType>(gqr.LD_TYPE);
	const unsigned int ldScale = gqr.LD_SCALE;
	const u32 EA = m_GPR[_inst.RA] + m_GPR[_inst.RB];

	int c = 4;
	if ((ldType == 4) || (ldType == 6)) c = 0x1;
	if ((ldType == 5) || (ldType == 7)) c = 0x2;

	if (_inst.Wx == 0)
	{
		rPS0(_inst.RS) = Helper_Dequantize( EA,   ldType, ldScale );
		rPS1(_inst.RS) = Helper_Dequantize( EA+c, ldType, ldScale );
	}
	else
	{
		rPS0(_inst.RS) = Helper_Dequantize( EA, ldType, ldScale );
		rPS1(_inst.RS) = 1.0f;
	}
	m_GPR[_inst.RA] = EA;
}

void CInterpreter::psq_stux(UGeckoInstruction _inst)
{
	const UGQR gqr(rSPR(SPR_GQR0 + _inst.Ix));
	const EQuantizeType stType = static_cast<EQuantizeType>(gqr.ST_TYPE);
	const unsigned int stScale = gqr.ST_SCALE;
	const u32 EA = m_GPR[_inst.RA] + m_GPR[_inst.RB];

	int c = 4;
	if ((stType == 4) || (stType == 6)) c = 0x1;
	if ((stType == 5) || (stType == 7)) c = 0x2;

	if (_inst.Wx == 0)
	{
		Helper_Quantize(EA,   (float)rPS0(_inst.RS), stType, stScale);
		Helper_Quantize(EA+c, (float)rPS1(_inst.RS), stType, stScale);
	}
	else
	{
		Helper_Quantize(EA,   (float)rPS0(_inst.RS), stType, stScale);
	}
	m_GPR[_inst.RA] = EA;
}

void CInterpreter::ps_div(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) / rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FA) / rPS1(_inst.FB);
}

void CInterpreter::ps_sub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) - rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FA) - rPS1(_inst.FB);
}

void CInterpreter::ps_add(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) + rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FA) + rPS1(_inst.FB);
}

void CInterpreter::ps_sel(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) >= 0.0f) ? rPS0(_inst.FC) : rPS0(_inst.FB);
	rPS1(_inst.FD) = (rPS1(_inst.FA) >= 0.0f) ? rPS1(_inst.FC) : rPS1(_inst.FB);
}

void CInterpreter::ps_res(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = 1.0f / rPS0(_inst.FB);
	rPS1(_inst.FD) = 1.0f / rPS1(_inst.FB);
}

void CInterpreter::ps_mul(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) * rPS0(_inst.FC);
	rPS1(_inst.FD) = rPS1(_inst.FA) * rPS1(_inst.FC);
}

void CInterpreter::ps_rsqrte(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = 1.0 / sqrt(rPS0(_inst.FB));
	rPS1(_inst.FD) = 1.0 / sqrt(rPS1(_inst.FB));
}

void CInterpreter::ps_msub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB);
	rPS1(_inst.FD) = (rPS1(_inst.FA) * rPS1(_inst.FC)) - rPS1(_inst.FB);
}

void CInterpreter::ps_madd(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);
	rPS1(_inst.FD) = (rPS1(_inst.FA) * rPS1(_inst.FC)) + rPS1(_inst.FB);
}

void CInterpreter::ps_nmsub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -(rPS0(_inst.FA) * rPS0(_inst.FC) - rPS0(_inst.FB));
	rPS1(_inst.FD) = -(rPS1(_inst.FA) * rPS1(_inst.FC) - rPS1(_inst.FB));
}

void CInterpreter::ps_nmadd(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -(rPS0(_inst.FA) * rPS0(_inst.FC) + rPS0(_inst.FB));
	rPS1(_inst.FD) = -(rPS1(_inst.FA) * rPS1(_inst.FC) + rPS1(_inst.FB));
}

void CInterpreter::ps_neg(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);
	riPS1(_inst.FD) = riPS1(_inst.FB) ^ (1ULL << 63);
}

void CInterpreter::ps_mr(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FB);
}

void CInterpreter::ps_nabs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63); 
	riPS1(_inst.FD) = riPS1(_inst.FB) | (1ULL << 63); 
}

void CInterpreter::ps_abs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) &~ (1ULL << 63); 
	riPS1(_inst.FD) = riPS1(_inst.FB) &~ (1ULL << 63); 
}

void CInterpreter::ps_sum0(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA) + rPS1(_inst.FB);
	double p1 = rPS1(_inst.FC);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_sum1(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FC);
	double p1 = rPS0(_inst.FA) + rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_muls0(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA) * rPS0(_inst.FC);
	double p1 = rPS1(_inst.FA) * rPS0(_inst.FC);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_muls1(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA) * rPS1(_inst.FC);
	double p1 = rPS1(_inst.FA) * rPS1(_inst.FC);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_madds0(UGeckoInstruction _inst)
{
	double p0 = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);
	double p1 = (rPS1(_inst.FA) * rPS0(_inst.FC)) + rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_madds1(UGeckoInstruction _inst)
{
	double p0 = (rPS0(_inst.FA) * rPS1(_inst.FC)) + rPS0(_inst.FB);
	double p1 = (rPS1(_inst.FA) * rPS1(_inst.FC)) + rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_cmpu0(UGeckoInstruction _inst)
{
	double fa = rPS0(_inst.FA);
	double fb = rPS0(_inst.FB);
	int compareResult;
	if (fa < fb)		compareResult = 8; 
	else if (fa > fb) 	compareResult = 4; 
	else				compareResult = 2;
	SetCRField(_inst.CRFD, compareResult);
}

void CInterpreter::ps_cmpo0(UGeckoInstruction _inst)
{
	// for now HACK
	ps_cmpu0(_inst);
}

void CInterpreter::ps_cmpu1(UGeckoInstruction _inst)
{
	double fa = rPS1(_inst.FA);
	double fb = rPS1(_inst.FB);
	int compareResult;
	if (fa < fb)		compareResult = 8; 
	else if (fa > fb)	compareResult = 4; 
	else				compareResult = 2;

	SetCRField(_inst.CRFD, compareResult);
}

void CInterpreter::ps_cmpo1(UGeckoInstruction _inst)
{
	// for now HACK
	ps_cmpu1(_inst);
}

void CInterpreter::ps_merge00(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_merge01(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_merge10(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_merge11(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

// __________________________________________________________________________________________________
// dcbz_l
// TODO(ector) check docs
void 
CInterpreter::dcbz_l(UGeckoInstruction _inst)
{
	/*
	addr_t ea = Helper_Get_EA(_inst);

	u32 blockStart = ea & (~(CACHEBLOCKSIZE-1));
	u32 blockEnd = blockStart + CACHEBLOCKSIZE;

	//FAKE: clear memory instead of clearing the cache block
	for (int i=blockStart; i<blockEnd; i+=4)
	Memory::Write_U32(0,i);
	*/
}
