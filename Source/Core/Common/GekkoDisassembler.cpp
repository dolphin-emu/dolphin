/* $VER: ppc_disasm.c V1.5 (27.05.2009)
 *
 * Disassembler module for the PowerPC microprocessor family
 * Copyright (c) 1998-2001,2009,2011 Frank Wille
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// Modified for use with Dolphin

#include <string>

#include "Common/CommonTypes.h"
#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"

// version/revision
#define PPCDISASM_VER 1
#define PPCDISASM_REV 6

// general defines
#define PPCIDXMASK      0xfc000000
#define PPCIDX2MASK     0x000007fe
#define PPCDMASK        0x03e00000
#define PPCAMASK        0x001f0000
#define PPCBMASK        0x0000f800
#define PPCCMASK        0x000007c0
#define PPCMMASK        0x0000003e
#define PPCCRDMASK      0x03800000
#define PPCCRAMASK      0x001c0000
#define PPCLMASK        0x00600000
#define PPCOE           0x00000400
#define PPCVRC          0x00000400
#define PPCDST          0x02000000
#define PPCSTRM         0x00600000

#define PPCIDXSH        26
#define PPCDSH          21
#define PPCASH          16
#define PPCBSH          11
#define PPCCSH          6
#define PPCMSH          1
#define PPCCRDSH        23
#define PPCCRASH        18
#define PPCLSH          21
#define PPCIDX2SH       1

#define PPCGETIDX(x)    (((x)&PPCIDXMASK)>>PPCIDXSH)
#define PPCGETD(x)      (((x)&PPCDMASK)>>PPCDSH)
#define PPCGETA(x)      (((x)&PPCAMASK)>>PPCASH)
#define PPCGETB(x)      (((x)&PPCBMASK)>>PPCBSH)
#define PPCGETC(x)      (((x)&PPCCMASK)>>PPCCSH)
#define PPCGETM(x)      (((x)&PPCMMASK)>>PPCMSH)
#define PPCGETCRD(x)    (((x)&PPCCRDMASK)>>PPCCRDSH)
#define PPCGETCRA(x)    (((x)&PPCCRAMASK)>>PPCCRASH)
#define PPCGETL(x)      (((x)&PPCLMASK)>>PPCLSH)
#define PPCGETIDX2(x)   (((x)&PPCIDX2MASK)>>PPCIDX2SH)
#define PPCGETSTRM(x)   (((x)&PPCSTRM)>>PPCDSH)


static const char* trap_condition[32] = {
	nullptr, "lgt",   "llt",   nullptr, "eq",    "lge",   "lle",   nullptr,
	"gt",    nullptr, nullptr, nullptr, "ge",    nullptr, nullptr, nullptr,
	"lt",    nullptr, nullptr, nullptr, "le",    nullptr, nullptr, nullptr,
	"ne",    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

static const char* cmpname[4] = {
	"cmpw", "cmpd", "cmplw", "cmpld"
};

static const char* ps_cmpname[4] = {
	"ps_cmpu0", "ps_cmpo0", "ps_cmpu1", "ps_cmpo1"
};

static const char* b_ext[4] = {
	"", "l", "a", "la"
};

static const char* b_condition[8] = {
	"ge", "le", "ne", "ns", "lt", "gt", "eq", "so"
};

static const char* b_decr[16] = {
	"nzf", "zf", nullptr, nullptr, "nzt", "zt", nullptr, nullptr,
	"nz",  "z",  nullptr, nullptr, "nz",  "z",  nullptr, nullptr
};

static const char* regsel[2] = {
	"", "r"
};

static const char* oesel[2] = {
	"", "o"
};

static const char* rcsel[2] = {
	"", "."
};

static const char* ldstnames[24] = {
	"lwz", "lwzu", "lbz", "lbzu", "stw", "stwu", "stb", "stbu", "lhz", "lhzu",
	"lha", "lhau", "sth", "sthu", "lmw", "stmw", "lfs", "lfsu", "lfd", "lfdu",
	"stfs", "stfsu", "stfd", "stfdu"
};

static const char* regnames[32] = {
	"r0", "sp", "rtoc", "r3", "r4", "r5", "r6", "r7",
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
	"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
	"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
};

// Initialize static class variables.
u32* GekkoDisassembler::m_instr = nullptr;
u32* GekkoDisassembler::m_iaddr = nullptr;
std::string GekkoDisassembler::m_opcode = "";
std::string GekkoDisassembler::m_operands = "";
unsigned char GekkoDisassembler::m_type = 0;
unsigned char GekkoDisassembler::m_flags = PPCF_ILLEGAL;
unsigned short GekkoDisassembler::m_sreg = 0;
u32 GekkoDisassembler::m_displacement = 0;


static std::string spr_name(int i)
{
	switch (i)
	{
	case 1: return "XER";
	case 8: return "LR";
	case 9: return "CTR";
	case 18: return "DSIR";
	case 19: return "DAR";
	case 22: return "DEC";
	case 25: return "SDR1";
	case 26: return "SRR0";
	case 27: return "SRR1";
	case 272: return "SPRG0";
	case 273: return "SPRG1";
	case 274: return "SPRG2";
	case 275: return "SPRG3";
	case 282: return "EAR";
	case 287: return "PVR";
	case 528: return "IBAT0U";
	case 529: return "IBAT0L";
	case 530: return "IBAT1U";
	case 531: return "IBAT1L";
	case 532: return "IBAT2U";
	case 533: return "IBAT2L";
	case 534: return "IBAT3U";
	case 535: return "IBAT3L";
	case 536: return "DBAT0U";
	case 537: return "DBAT0L";
	case 538: return "DBAT1U";
	case 539: return "DBAT1L";
	case 540: return "DBAT2U";
	case 541: return "DBAT2L";
	case 542: return "DBAT3U";
	case 543: return "DBAT3L";
	case 912: return "GQR0";
	case 913: return "GQR1";
	case 914: return "GQR2";
	case 915: return "GQR3";
	case 916: return "GQR4";
	case 917: return "GQR5";
	case 918: return "GQR6";
	case 919: return "GQR7";
	case 920: return "HID2";
	case 921: return "WPAR";
	case 922: return "DMA_U";
	case 923: return "DMA_L";
	case 924: return "ECID_U";
	case 925: return "ECID_M";
	case 926: return "ECID_L";
	case 936: return "UMMCR0";
	case 937: return "UPMC1";
	case 938: return "UPMC2";
	case 939: return "USIA";
	case 940: return "UMMCR1";
	case 941: return "UPMC3";
	case 942: return "UPMC4";
	case 943: return "USDA";
	case 952: return "MMCR0";
	case 953: return "PMC1";
	case 954: return "PMC2";
	case 955: return "SIA";
	case 956: return "MMCR1";
	case 957: return "PMC3";
	case 958: return "PMC4";
	case 959: return "SDA";
	case 1008: return "HID0";
	case 1009: return "HID1";
	case 1010: return "IABR";
	case 1011: return "HID4";
	case 1013: return "DABR";
	case 1017: return "L2CR";
	case 1019: return "ICTC";
	case 1020: return "THRM1";
	case 1021: return "THRM2";
	case 1022: return "THRM3";
	}

	return StringFromFormat("%d", i);
}


static u32 swapda(u32 w)
{
	return ((w & 0xfc00ffff) | ((w&PPCAMASK) << 5) | ((w&PPCDMASK) >> 5));
}


static u32 swapab(u32 w)
{
	return ((w & 0xffe007ff) | ((w&PPCBMASK) << 5) | ((w&PPCAMASK) >> 5));
}


void GekkoDisassembler::ill(u32 in)
{
	if (in == 0)
	{
		m_opcode = "";
		m_operands = "---";
	}
	else
	{
		m_opcode = "(ill)";
		m_operands =  StringFromFormat("%08x", in);
	}

	m_flags |= PPCF_ILLEGAL;
}

// Generate immediate instruction operand.
//
// Type 0: D-mode, D,A,imm
// Type 1: S-mode, A,S,imm
// Type 2: S/D register is ignored (trap,cmpi)
// Type 3: A register is ignored (li)
std::string GekkoDisassembler::imm(u32 in, int uimm, int type, bool hex)
{
	int i = (int)(in & 0xffff);

	m_type = PPCINSTR_IMM;

	if (uimm == 0)
	{
		if (i > 0x7fff)
			i -= 0x10000;
	}
	else
	{
		m_flags |= PPCF_UNSIGNED;
	}
	m_displacement = i;

	switch (type)
	{
	case 0:
		return StringFromFormat("%s, %s, %d", regnames[(int)PPCGETD(in)], regnames[(int)PPCGETA(in)], i);

	case 1:
		if (hex)
			return StringFromFormat("%s, %s, 0x%.4X", regnames[(int)PPCGETA(in)], regnames[(int)PPCGETD(in)], i);
		else
			return StringFromFormat("%s, %s, %d", regnames[(int)PPCGETA(in)], regnames[(int)PPCGETD(in)], i);

	case 2:
		return StringFromFormat("%s, %d", regnames[(int)PPCGETA(in)], i);

	case 3:
		if (hex)
			return StringFromFormat("%s, 0x%.4X", regnames[(int)PPCGETD(in)], i);
		else
			return StringFromFormat("%s, %d", regnames[(int)PPCGETD(in)], i);

	default:
		return StringFromFormat("%s", "imm(): Wrong type");
	}
}


std::string GekkoDisassembler::ra_rb(u32 in)
{
	return StringFromFormat("%s, %s", regnames[(int)PPCGETA(in)], regnames[(int)PPCGETB(in)]);
}


std::string GekkoDisassembler::rd_ra_rb(u32 in, int mask)
{
	std::string result;

	if (mask)
	{
		if (mask & 4)
			result += StringFromFormat("%s, ", regnames[(int)PPCGETD(in)]);
		if (mask & 2)
			result += StringFromFormat("%s, ", regnames[(int)PPCGETA(in)]);
		if (mask & 1)
			result += StringFromFormat("%s, ", regnames[(int)PPCGETB(in)]);

		size_t pos = result.rfind(", ");
		if (pos != std::string::npos)
		{
			result.erase(pos, result.length() - pos);
		}
	}

	return result;
}


std::string GekkoDisassembler::fd_ra_rb(u32 in, int mask)
{
	std::string result;

	if (mask)
	{
		if (mask & 4)
			result += StringFromFormat("f%d,", (int)PPCGETD(in));
		if (mask & 2)
			result += StringFromFormat("%s,", regnames[(int)PPCGETA(in)]);
		if (mask & 1)
			result += StringFromFormat("%s,", regnames[(int)PPCGETB(in)]);

		// Drop the trailing comma
		result.pop_back();
	}

	return result;
}


void GekkoDisassembler::trapi(u32 in, unsigned char dmode)
{
	const char* cnd = trap_condition[PPCGETD(in)];


	m_flags |= dmode;
	if (cnd != nullptr)
	{
		m_opcode = StringFromFormat("t%c%s", dmode ? 'd' : 'w', cnd);
	}
	else
	{
		m_opcode = StringFromFormat("t%ci", dmode ? 'd' : 'w');
		m_operands = StringFromFormat("%d, ", PPCGETD(in));
	}
	m_operands += imm(in, 0, 2, false);
}


void GekkoDisassembler::cmpi(u32 in, int uimm)
{
	int i = (int)PPCGETL(in);

	if (i < 2)
	{
		if (i != 0)
			m_flags |= PPCF_64;

		m_opcode = StringFromFormat("%si", cmpname[uimm * 2 + i]);

		i = (int)PPCGETCRD(in);
		if (i != 0)
		{
			m_operands += StringFromFormat("cr%c, ", '0' + i);
		}

		m_operands += imm(in, uimm, 2, false);
	}
	else
	{
		ill(in);
	}
}


void GekkoDisassembler::addi(u32 in, const std::string& ext)
{
	if ((in & 0x08000000) && !PPCGETA(in))
	{
		m_opcode = StringFromFormat("l%s", ext.c_str());  // li, lis

		if (ext == "i")
			m_operands = imm(in, 0, 3, false);
		else
			m_operands = imm(in, 1, 3, true);
	}
	else
	{
		m_opcode = StringFromFormat("%s%s", (in & 0x8000) ? "sub" : "add", ext.c_str());

		if (in & 0x8000)
			in = (in ^ 0xffff) + 1;

		m_operands = imm(in, 1, 0, false);
	}
}

// Build a branch instr. and return number of chars written to operand.
size_t GekkoDisassembler::branch(u32 in, const char* bname, int aform, int bdisp)
{
	int bo = (int)PPCGETD(in);
	int bi = (int)PPCGETA(in);
	char y = (char)(bo & 1);
	const char* ext = b_ext[aform * 2 + (int)(in & 1)];

	if (bdisp < 0)
		y ^= 1;
	y = (y != 0) ? '+' : '-';

	if (bo & 4)
	{
		// standard case - no decrement
		if (bo & 16)
		{
			// branch always
			if (PPCGETIDX(in) != 16)
			{
				m_opcode = StringFromFormat("b%s%s", bname, ext);
			}
			else
			{
				m_opcode = StringFromFormat("bc%s", ext);
				m_operands = StringFromFormat("%d, %d", bo, bi);
			}
		}
		else // Branch conditional
		{
			m_opcode = StringFromFormat("b%s%s%s%c", b_condition[((bo & 8) >> 1) + (bi & 3)], bname, ext, y);

			if (bi >= 4)
			{
				m_operands = StringFromFormat("cr%d", bi >> 2);
			}
		}
	}
	else
	{
		// CTR is decremented and checked
		m_opcode = StringFromFormat("bd%s%s%s%c", b_decr[bo >> 1], bname, ext, y);

		if ((bo & 16) == 0)
		{
			m_operands = StringFromFormat("%d", bi);
		}
	}

	return m_operands.length();
}


void GekkoDisassembler::bc(u32 in)
{
	unsigned int d = (int)(in & 0xfffc);

	if (d & 0x8000)
		d |= 0xffff0000;

	branch(in, "", (in & 2) ? 1 : 0, d);

	if (in & 2)  // AA ?
		m_operands = StringFromFormat("->0x%.8X", d);
	else
		m_operands = StringFromFormat("->0x%.8X", *m_iaddr + d);

	m_type = PPCINSTR_BRANCH;
	m_displacement = d;
}


void GekkoDisassembler::bli(u32 in)
{
	unsigned int d = (unsigned int)(in & 0x3fffffc);

	if (d & 0x02000000)
		d |= 0xfc000000;

	m_opcode = StringFromFormat("b%s", b_ext[in & 3]);

	if (in & 2)  // AA ?
		m_operands = StringFromFormat("->0x%.8X", d);
	else
		m_operands = StringFromFormat("->0x%.8X", *m_iaddr + d);

	m_type = PPCINSTR_BRANCH;
	m_displacement = d;
}


void GekkoDisassembler::mcrf(u32 in, char c)
{
	if ((in & 0x0063f801) == 0)
	{
		m_opcode = StringFromFormat("mcrf%c", c);
		m_operands = StringFromFormat("cr%d, cr%d", (int)PPCGETCRD(in), (int)PPCGETCRA(in));
	}
	else
	{
		ill(in);
	}
}


void GekkoDisassembler::crop(u32 in, const char* n1, const char* n2)
{
	int crd = (int)PPCGETD(in);
	int cra = (int)PPCGETA(in);
	int crb = (int)PPCGETB(in);

	if ((in & 1) == 0)
	{
		m_opcode = StringFromFormat("cr%s", (cra == crb && n2) ? n2 : n1);
		if (cra == crb && n2)
			m_operands = StringFromFormat("%d, %d", crd, cra);
		else
			m_operands = StringFromFormat("%d, %d, %d", crd, cra, crb);
	}
	else
	{
		ill(in);
	}
}


void GekkoDisassembler::nooper(u32 in, const char* name, unsigned char dmode)
{
	if (in & (PPCDMASK | PPCAMASK | PPCBMASK | 1))
	{
		ill(in);
	}
	else
	{
		m_flags |= dmode;
		m_opcode = name;
	}
}

void GekkoDisassembler::rlw(u32 in, const char* name, int i)
{
	int s = (int)PPCGETD(in);
	int a = (int)PPCGETA(in);
	int bsh = (int)PPCGETB(in);
	int mb = (int)PPCGETC(in);
	int me = (int)PPCGETM(in);

	m_opcode = StringFromFormat("rlw%s%c", name, in & 1 ? '.' : '\0');
	m_operands = StringFromFormat("%s, %s, %s%d, %d, %d (%08x)", regnames[a], regnames[s], regsel[i], bsh, mb, me, HelperRotateMask(bsh, mb, me));
}


void GekkoDisassembler::ori(u32 in, const char* name)
{
	m_opcode = name;
	m_operands = imm(in, 1, 1, true);
}


void GekkoDisassembler::rld(u32 in, const char* name, int i)
{
	int s = (int)PPCGETD(in);
	int a = (int)PPCGETA(in);
	int bsh = i ? (int)PPCGETB(in) : (int)(((in & 2) << 4) + PPCGETB(in));
	int m = (int)(in & 0x7e0) >> 5;

	m_flags |= PPCF_64;
	m_opcode = StringFromFormat("rld%s%c", name, in & 1 ? '.' : '\0');
	m_operands = StringFromFormat("%s, %s, %s%d, %d", regnames[a], regnames[s], regsel[i], bsh, m);
}


void GekkoDisassembler::cmp(u32 in)
{
	int i = (int)PPCGETL(in);

	if (i < 2)
	{
		if (i != 0)
			m_flags |= PPCF_64;

		m_opcode = cmpname[((in&PPCIDX2MASK) ? 2 : 0) + i];

		i = (int)PPCGETCRD(in);
		if (i != 0)
			m_operands += StringFromFormat("cr%c,", '0' + i);

		m_operands += ra_rb(in);
	}
	else
	{
		ill(in);
	}
}


void GekkoDisassembler::trap(u32 in, unsigned char dmode)
{
	int to = (int)PPCGETD(in);
	const char* cnd = trap_condition[to];

	if (cnd != nullptr)
	{
		m_flags |= dmode;
		m_opcode = StringFromFormat("t%c%s", dmode ? 'd' : 'w', cnd);
		m_operands = ra_rb(in);
	}
	else
	{
		if (to == 31)
		{
			if (dmode)
			{
				m_flags |= dmode;
				m_opcode = "td";
				m_operands = "31,0,0";
			}
			else
			{
				m_opcode = "trap";
			}
		}
		else
		{
			ill(in);
		}
	}
}

// Standard instruction: xxxx rD,rA,rB
void GekkoDisassembler::dab(u32 in, const char* name, int mask, int smode, int chkoe, int chkrc, unsigned char dmode)
{
	if (chkrc >= 0 && ((in & 1) != (unsigned int)chkrc))
	{
		ill(in);
	}
	else
	{
		m_flags |= dmode;

		// rA,rS,rB
		if (smode)
			in = swapda(in);

		m_opcode = StringFromFormat("%s%s%s", name, oesel[chkoe && (in&PPCOE)], rcsel[(chkrc < 0) && (in & 1)]);
		m_operands = rd_ra_rb(in, mask);
	}
}

// Last operand is no register: xxxx rD,rA,NB
void GekkoDisassembler::rrn(u32 in, const char* name, int smode, int chkoe, int chkrc, unsigned char dmode)
{
	if (chkrc >= 0 && ((in & 1) != (unsigned int)chkrc))
	{
		ill(in);
	}
	else
	{
		m_flags |= dmode;

		// rA,rS,NB
		if (smode)
			in = swapda(in);

		m_opcode = StringFromFormat("%s%s%s", name, oesel[chkoe && (in&PPCOE)], rcsel[(chkrc < 0) && (in & 1)]);

		m_operands = rd_ra_rb(in, 6);
		m_operands += StringFromFormat(",%d",(int)PPCGETB(in));
	}
}


void GekkoDisassembler::mtcr(u32 in)
{
	int s = (int)PPCGETD(in);
	int crm = (int)(in & 0x000ff000) >> 12;

	if (in & 0x00100801)
	{
		ill(in);
	}
	else
	{
		m_opcode = StringFromFormat("mtcr%c", crm == 0xff ? '\0' : 'f');

		if (crm != 0xff)
			m_operands += StringFromFormat("0x%02x,", crm);

		m_operands += regnames[s];
	}
}


void GekkoDisassembler::msr(u32 in, int smode)
{
	int s = (int)PPCGETD(in);
	int sr = (int)(in & 0x000f0000) >> 16;

	if (in & 0x0010f801)
	{
		ill(in);
	}
	else
	{
		m_flags |= PPCF_SUPER;
		m_opcode = StringFromFormat("m%csr", smode ? 't' : 'f');

		if (smode)
			m_operands = StringFromFormat("%d, %s", sr, regnames[s]);
		else
			m_operands = StringFromFormat("%s, %d", regnames[s], sr);
	}
}


void GekkoDisassembler::mspr(u32 in, int smode)
{
	int d = (int)PPCGETD(in);
	int spr = (int)((PPCGETB(in) << 5) + PPCGETA(in));
	int fmt = 0;

	if (in & 1)
	{
		ill(in);
	}
	else
	{
		if (spr != 1 && spr != 8 && spr != 9)
			m_flags |= PPCF_SUPER;

		const char* x;
		switch (spr)
		{
		case 1:
			x = "xer";
			break;

		case 8:
			x = "lr";
			break;

		case 9:
			x = "ctr";
			break;

		default:
			x = "spr";
			fmt = 1;
			break;
		}

		m_opcode = StringFromFormat("m%c%s", smode ? 't' : 'f', x);

		if (fmt)
		{
			if (smode)
				m_operands = StringFromFormat("%s, %s", spr_name(spr).c_str(), regnames[d]);
			else
				m_operands = StringFromFormat("%s, %s", regnames[d], spr_name(spr).c_str());
		}
		else
		{
			m_operands = regnames[d];
		}
	}
}


void GekkoDisassembler::mtb(u32 in)
{
	int d = (int)PPCGETD(in);
	int tbr = (int)((PPCGETB(in) << 5) + PPCGETA(in));

	if (in & 1)
	{
		ill(in);
	}
	else
	{
		m_operands += regnames[d];

		char x;
		switch (tbr)
		{
		case 268:
			x = 'l';
			break;

		case 269:
			x = 'u';
			break;

		default:
			x = '\0';
			m_flags |= PPCF_SUPER;
			m_operands += StringFromFormat(",%d", tbr);
			break;
		}

		m_opcode = StringFromFormat("mftb%c", x);
	}
}


void GekkoDisassembler::sradi(u32 in)
{
	int s = (int)PPCGETD(in);
	int a = (int)PPCGETA(in);
	int bsh = (int)(((in & 2) << 4) + PPCGETB(in));

	m_flags |= PPCF_64;
	m_opcode = StringFromFormat("sradi%c", in & 1 ? '.' : '\0');
	m_operands = StringFromFormat("%s, %s, %d", regnames[a], regnames[s], bsh);
}

void GekkoDisassembler::ldst(u32 in, const char* name, char reg, unsigned char dmode)
{
	int s = (int)PPCGETD(in);
	int a = (int)PPCGETA(in);
	int d = (u32)(in & 0xffff);

	m_type = PPCINSTR_LDST;
	m_flags |= dmode;
	m_sreg = (short)a;
	//  if (d >= 0x8000)
	//    d -= 0x10000;
	m_displacement = (u32)d;
	m_opcode = name;

	if (reg == 'r')
	{
		m_operands = StringFromFormat("%s, %s (%s)", regnames[s], ldst_offs(d).c_str(), regnames[a]);
	}
	else
	{
		m_operands = StringFromFormat("%c%d, %s (%s)", reg, s, ldst_offs(d).c_str(), regnames[a]);
	}
}

// Standard floating point instruction: xxxx fD,fA,fC,fB
void GekkoDisassembler::fdabc(u32 in, const char* name, int mask, unsigned char dmode)
{
	int err = 0;

	m_flags |= dmode;
	m_opcode = StringFromFormat("f%s%s", name, rcsel[in & 1]);
	m_operands += StringFromFormat("f%d,", (int)PPCGETD(in));

	if (mask & 4)
		m_operands += StringFromFormat("f%d,", (int)PPCGETA(in));
	else
		err |= (int)PPCGETA(in);

	if (mask & 2)
		m_operands += StringFromFormat("f%d,", (int)PPCGETC(in));
	else if (PPCGETC(in))
		err |= (int)PPCGETC(in);

	if (mask & 1)
		m_operands += StringFromFormat("f%d,", (int)PPCGETB(in));
	else if (!(mask & 8))
		err |= (int)PPCGETB(in);

	// Drop the trailing comma
	m_operands.pop_back();

	if (err)
		ill(in);
}

void GekkoDisassembler::fmr(u32 in)
{
	m_opcode = StringFromFormat("fmr%s", rcsel[in & 1]);
	m_operands = StringFromFormat("f%d, f%d", (int)PPCGETD(in), (int)PPCGETB(in));
}

// Indexed float instruction: xxxx fD,rA,rB
void GekkoDisassembler::fdab(u32 in, const char* name, int mask)
{
	m_opcode = name;
	m_operands = fd_ra_rb(in, mask);
}


void GekkoDisassembler::fcmp(u32 in, char c)
{
	if (in & 0x00600001)
	{
		ill(in);
	}
	else
	{
		m_opcode = StringFromFormat("fcmp%c", c);
		m_operands = StringFromFormat("cr%d,f%d,f%d", (int)PPCGETCRD(in), (int)PPCGETA(in), (int)PPCGETB(in));
	}
}


void GekkoDisassembler::mtfsb(u32 in, int n)
{
	if (in & (PPCAMASK | PPCBMASK))
	{
		ill(in);
	}
	else
	{
		m_opcode = StringFromFormat("mtfsb%d%s", n, rcsel[in & 1]);
		m_operands = StringFromFormat("%d", (int)PPCGETD(in));
	}
}


// Paired instructions

#define RA ((inst >> 16) & 0x1f)
#define RB ((inst >> 11) & 0x1f)
#define RC ((inst >> 6) & 0x1f)
#define RD ((inst >> 21) & 0x1f)
#define RS ((inst >> 21) & 0x1f)
#define FA ((inst >> 16) & 0x1f)
#define FB ((inst >> 11) & 0x1f)
#define FC ((inst >> 6) & 0x1f)
#define FD ((inst >> 21) & 0x1f)
#define FS ((inst >> 21) & 0x1f)
#define IMM (inst & 0xffff)
#define UIMM (inst & 0xffff)
#define OFS (inst & 0xffff)
#define OPCD ((inst >> 26) & 0x3f)
#define XO_10 ((inst >> 1) & 0x3ff)
#define XO_9 ((inst >> 1) & 0x1ff)
#define XO_5 ((inst >> 1) & 0x1f)
#define Rc (inst & 1)
#define SH ((inst >> 11) & 0x1f)
#define MB ((inst >> 6) & 0x1f)
#define ME ((inst >> 1) & 0x1f)
#define OE ((inst >> 10) & 1)
#define TO ((inst >> 21) & 0x1f)
#define CRFD ((inst >> 23) & 0x7)
#define CRFS ((inst >> 18) & 0x7)
#define CRBD ((inst >> 21) & 0x1f)
#define CRBA ((inst >> 16) & 0x1f)
#define CRBB ((inst >> 11) & 0x1f)
#define L ((inst >> 21) & 1)
#define NB ((inst >> 11) & 0x1f)
#define AA ((inst >> 1) & 1)
#define LK (inst & 1)
#define LI ((inst >> 2) & 0xffffff)
#define BO ((inst >> 21) & 0x1f)
#define BI ((inst >> 16) & 0x1f)
#define BD ((inst >> 2) & 0x3fff)

#define MTFSFI_IMM ((inst >> 12) & 0xf)
#define FM ((inst >> 17) & 0xff)
#define SR ((inst >> 16) & 0xf)
#define SPR ((inst >> 11) & 0x3ff)
#define TBR ((inst >> 11) & 0x3ff)
#define CRM ((inst >> 12) & 0xff)


void GekkoDisassembler::ps(u32 inst)
{
	switch ((inst >> 1) & 0x1F)
	{
	case 6:
		m_opcode = inst & 0x40 ? "psq_lux" : "psq_lx";
		m_operands = StringFromFormat("p%u, (r%u + r%u)", FD, RA, RB);
		return;

	case 7:
		m_opcode = inst & 0x40 ? "psq_stux" : "psq_stx";
		m_operands = StringFromFormat("(r%u + r%u), p%u", RA, RB, FS);
		return;

	case 18:
		m_opcode = "ps_div";
		m_operands = StringFromFormat("p%u, p%u/p%u", FD, FA, FB);
		return;

	case 20:
		m_opcode = "ps_sub";
		m_operands = StringFromFormat("p%u, p%u-p%u", FD, FA, FB);
		return;

	case 21:
		m_opcode = "ps_add";
		m_operands = StringFromFormat("p%u, p%u+p%u", FD, FA, FB);
		return;

	case 23:
		m_opcode = "ps_sel";
		m_operands = StringFromFormat("p%u>=0?p%u:p%u", FD, FA, FC);
		return;

	case 24:
		m_opcode = "ps_res";
		m_operands = StringFromFormat("p%u, (1/p%u)", FD, FB);
		return;

	case 25:
		m_opcode = "ps_mul";
		m_operands = StringFromFormat("p%u, p%u*p%u", FD, FA, FC);
		return;

	case 26: // rsqrte
		m_opcode = "ps_rsqrte";
		m_operands = StringFromFormat("p%u, p%u", FD, FB);
		return;

	case 28: // msub
		m_opcode = "ps_msub";
		m_operands = StringFromFormat("p%u, p%u*p%u-p%u", FD, FA, FC, FB);
		return;

	case 29: // madd
		m_opcode = "ps_madd";
		m_operands = StringFromFormat("p%u, p%u*p%u+p%u", FD, FA, FC, FB);
		return;

	case 30: // nmsub
		m_opcode = "ps_nmsub";
		m_operands = StringFromFormat("p%u, -(p%u*p%u-p%u)", FD, FA, FC, FB);
		return;

	case 31: // nmadd
		m_opcode = "ps_nmadd";
		m_operands = StringFromFormat("p%u, -(p%u*p%u+p%u)", FD, FA, FC, FB);
		return;

	case 10:
		m_opcode = "ps_sum0";
		m_operands = StringFromFormat("p%u, 0=p%u+p%u, 1=p%u", FD, FA, FB, FC);
		return;

	case 11:
		m_opcode = "ps_sum1";
		m_operands = StringFromFormat("p%u, 0=p%u, 1=p%u+p%u", FD, FC, FA, FB);
		return;

	case 12:
		m_opcode = "ps_muls0";
		m_operands = StringFromFormat("p%u, p%u*p%u[0]", FD, FA, FC);
		return;

	case 13:
		m_opcode = "ps_muls1";
		m_operands = StringFromFormat("p%u, p%u*p%u[1]", FD, FA, FC);
		return;

	case 14:
		m_opcode = "ps_madds0";
		m_operands = StringFromFormat("p%u, p%u*p%u[0]+p%u", FD, FA, FC, FB);
		return;

	case 15:
		m_opcode = "ps_madds1";
		m_operands = StringFromFormat("p%u, p%u*p%u[1]+p%u", FD, FA, FC, FB);
		return;
	}

	switch ((inst >> 1) & 0x3FF)
	{
		// 10-bit suckers  (?)
	case 40: // nmadd
		m_opcode = "ps_neg";
		m_operands = StringFromFormat("p%u, -p%u", FD, FB);
		return;

	case 72: // nmadd
		m_opcode = "ps_mr";
		m_operands = StringFromFormat("p%u, p%u", FD, FB);
		return;

	case 136:
		m_opcode = "ps_nabs";
		m_operands = StringFromFormat("p%u, -|p%u|", FD, FB);
		return;

	case 264:
		m_opcode = "ps_abs";
		m_operands = StringFromFormat("p%u, |p%u|", FD, FB);
		return;

	case 0:
	case 32:
	case 64:
	case 96:
	{
		m_opcode = ps_cmpname[(inst >> 6) & 0x3];

		int i = (int)PPCGETCRD(inst);
		if (i != 0)
			m_operands += StringFromFormat("cr%c, ", '0' + i);
		m_operands += StringFromFormat("p%u, p%u", FA, FB);
		return;
	}
	case 528:
		m_opcode = "ps_merge00";
		m_operands = StringFromFormat("p%u, p%u[0],p%u[0]", FD, FA, FB);
		return;

	case 560:
		m_opcode = "ps_merge01";
		m_operands = StringFromFormat("p%u, p%u[0],p%u[1]", FD, FA, FB);
		return;

	case 592:
		m_opcode = "ps_merge10";
		m_operands = StringFromFormat("p%u, p%u[1],p%u[0]", FD, FA, FB);
		return;

	case 624:
		m_opcode = "ps_merge11";
		m_operands = StringFromFormat("p%u, p%u[1],p%u[1]", FD, FA, FB);
		return;

	case 1014:
		if (inst & PPCDMASK)
			ill(inst);
		else
			dab(inst, "dcbz_l", 3, 0, 0, 0, 0);
	}

	//	default:
	m_opcode = StringFromFormat("ps_%i", ((inst >> 1) & 0x1f));
	m_operands = "---";
}

void GekkoDisassembler::ps_mem(u32 inst)
{
	switch (PPCGETIDX(inst))
	{
	case 56:
		m_opcode = "psq_l";
		m_operands = StringFromFormat("p%u, %i(r%u)", RS, SEX12(inst & 0xFFF), RA);
		break;

	case 57:
		m_opcode = "psq_lu";
		m_operands = StringFromFormat("p%u, %i(r%u)", RS, SEX12(inst & 0xFFF), RA);;
		break;

	case 60:
		m_opcode = "psq_st";
		m_operands = StringFromFormat("%i(r%u), p%u", SEX12(inst & 0xFFF), RA, RS);
		break;

	case 61:
		m_opcode = "psq_stu";
		m_operands = StringFromFormat("%i(r%u), p%u", SEX12(inst & 0xFFF), RA, RS);
		break;
	}
}

// Disassemble PPC instruction and return a pointer to the next
// instruction, or nullptr if an error occured.
u32* GekkoDisassembler::DoDisassembly(bool big_endian)
{
	u32 in = *m_instr;

	if (!big_endian)
	{
		in = (in & 0xff) << 24 | (in & 0xff00) << 8 | (in & 0xff0000) >> 8 |
		     (in & 0xff000000) >> 24;
	}

	m_opcode.clear();
	m_operands.clear();
	m_type = PPCINSTR_OTHER;
	m_flags = 0;

	switch (PPCGETIDX(in))
	{
	case 0:
		{
			int block = in & 0x3FFFFFF;
			if (block)
			{
				m_opcode = "JITblock";
				m_operands = StringFromFormat("%i", block);
			}
			else
			{
				m_opcode = "";
				m_operands = "---";
			}
		}
		break;

	case 1: // HLE call
		m_opcode = "HLE";
		break;

	case 2:
		trapi(in, PPCF_64); // tdi
		break;

	case 3:
		trapi(in, 0); // twi
		break;

	case 4:
		ps(in);
		break;

	case 56:
	case 57:
	case 60:
	case 61:
		ps_mem(in);
		break;

	case 7:
		m_opcode = "mulli";
		m_operands = imm(in, 0, 0, false);
		break;

	case 8:
		m_opcode = "subfic";
		m_operands = imm(in, 0, 0, false);
		break;

	case 10:
		cmpi(in, 1);      // cmpli
		break;

	case 11:
		cmpi(in, 0);      // cmpi
		break;

	case 12:
		addi(in, "ic");   // addic
		break;

	case 13:
		addi(in, "ic.");  // addic.
		break;

	case 14:
		addi(in, "i");    // addi
		break;

	case 15:
		addi(in, "is");   // addis
		break;

	case 16:
		bc(in);
		break;

	case 17:
		if ((in & ~PPCIDXMASK) == 2)
			m_opcode = "sc";
		else
			ill(in);
		break;

	case 18:
		bli(in);
		break;

	case 19:
		switch (PPCGETIDX2(in))
		{
		case 0:
			mcrf(in, '\0');  // mcrf
			break;

		case 16:
			branch(in, "lr", 0, 0);    // bclr
			break;

		case 33:
			crop(in, "nor", "not");    // crnor
			break;

		case 50:
			nooper(in, "rfi", PPCF_SUPER);
			break;

		case 129:
			crop(in, "andc", nullptr); // crandc
			break;

		case 150:
			nooper(in, "isync", 0);
			break;

		case 193:
			crop(in, "xor", "clr");    // crxor
			break;

		case 225:
			crop(in, "nand", nullptr); // crnand
			break;

		case 257:
			crop(in, "and", nullptr);  // crand
			break;

		case 289:
			crop(in, "eqv", "set");    // creqv
			break;

		case 417:
			crop(in, "orc", nullptr);  // crorc
			break;

		case 449:
			crop(in, "or", "move");    // cror
			break;

		case 528:
			branch(in, "ctr", 0, 0);   // bcctr
			break;

		default:
			ill(in);
			break;
		}
		break;

	case 20:
		rlw(in, "imi", 0); // rlwimi
		break;

	case 21:
		rlw(in, "inm", 0); // rlwinm
		break;

	case 23:
		rlw(in, "nm", 1);  // rlwnm
		break;

	case 24:
		if (in & ~PPCIDXMASK)
			ori(in, "ori");
		else
			m_opcode = "nop";
		break;

	case 25:
		ori(in, "oris");
		break;

	case 26:
		ori(in, "xori");
		break;

	case 27:
		ori(in, "xoris");
		break;

	case 28:
		ori(in, "andi.");
		break;

	case 29:
		ori(in, "andis.");
		break;

	case 30:
		switch (in & 0x1c)
		{
		case 0:
			rld(in, "icl", 0);  // rldicl
			break;
		case 1:
			rld(in, "icr", 0);  // rldicr
			break;
		case 2:
			rld(in, "ic", 0);   // rldic
			break;
		case 3:
			rld(in, "imi", 0);  // rldimi
			break;
		case 4:
			rld(in, in & 2 ? "cl" : "cr", 1); // rldcl, rldcr
			break;
		default:
			ill(in);
			break;
		}
		break;

	case 31:
		switch (PPCGETIDX2(in))
		{
		case 0:
		case 32:
			if (in & 1)
				ill(in);
			else
				cmp(in); // cmp, cmpl
			break;

		case 4:
			if (in & 1)
				ill(in);
			else
				trap(in, 0); // tw
			break;

		case 8:
		case (PPCOE >> 1) + 8:
			dab(swapab(in), "subc", 7, 0, 1, -1, 0);
			break;

		case 9:
			dab(in, "mulhdu", 7, 0, 0, -1, PPCF_64);
			break;

		case 10:
		case (PPCOE >> 1) + 10:
			dab(in, "addc", 7, 0, 1, -1, 0);
			break;

		case 11:
			dab(in, "mulhwu", 7, 0, 0, -1, 0);
			break;

		case 19:
			if (in & (PPCAMASK | PPCBMASK))
				ill(in);
			else
				dab(in, "mfcr", 4, 0, 0, 0, 0);
			break;

		case 20:
			dab(in, "lwarx", 7, 0, 0, 0, 0);
			break;

		case 21:
			dab(in, "ldx", 7, 0, 0, 0, PPCF_64);
			break;

		case 23:
			dab(in, "lwzx", 7, 0, 0, 0, 0);
			break;

		case 24:
			dab(in, "slw", 7, 1, 0, -1, 0);
			break;

		case 26:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "cntlzw", 6, 1, 0, -1, 0);
			break;

		case 27:
			dab(in, "sld", 7, 1, 0, -1, PPCF_64);
			break;

		case 28:
			dab(in, "and", 7, 1, 0, -1, 0);
			break;

		case 40:
		case (PPCOE >> 1) + 40:
			dab(swapab(in), "sub", 7, 0, 1, -1, 0);
			break;

		case 53:
			dab(in, "ldux", 7, 0, 0, 0, PPCF_64);
			break;

		case 54:
			if (in & PPCDMASK)
				ill(in);
			else
				dab(in, "dcbst", 3, 0, 0, 0, 0);
			break;

		case 55:
			dab(in, "lwzux", 7, 0, 0, 0, 0);
			break;

		case 58:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "cntlzd", 6, 1, 0, -1, PPCF_64);
			break;

		case 60:
			dab(in, "andc", 7, 1, 0, -1, 0);
			break;

		case 68:
			trap(in, PPCF_64); // td
			break;

		case 73:
			dab(in, "mulhd", 7, 0, 0, -1, PPCF_64);
			break;

		case 75:
			dab(in, "mulhw", 7, 0, 0, -1, 0);
			break;

		case 83:
			if (in & (PPCAMASK | PPCBMASK))
				ill(in);
			else
				dab(in, "mfmsr", 4, 0, 0, 0, PPCF_SUPER);
			break;

		case 84:
			dab(in, "ldarx", 7, 0, 0, 0, PPCF_64);
			break;

		case 86:
			if (in & PPCDMASK)
				ill(in);
			else
				dab(in, "dcbf", 3, 0, 0, 0, 0);
			break;

		case 87:
			dab(in, "lbzx", 7, 0, 0, 0, 0);
			break;

		case 104:
		case (PPCOE >> 1) + 104:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "neg", 6, 0, 1, -1, 0);
			break;

		case 119:
			dab(in, "lbzux", 7, 0, 0, 0, 0);
			break;

		case 124:
			if (PPCGETD(in) == PPCGETB(in))
				dab(in, "not", 6, 1, 0, -1, 0);
			else
				dab(in, "nor", 7, 1, 0, -1, 0);
			break;

		case 136:
		case (PPCOE >> 1) + 136:
			dab(in, "subfe", 7, 0, 1, -1, 0);
			break;

		case 138:
		case (PPCOE >> 1) + 138:
			dab(in, "adde", 7, 0, 1, -1, 0);
			break;

		case 144:
			mtcr(in);
			break;

		case 146:
			if (in & (PPCAMASK | PPCBMASK))
				ill(in);
			else
				dab(in, "mtmsr", 4, 0, 0, 0, PPCF_SUPER);
			break;

		case 149:
			dab(in, "stdx", 7, 0, 0, 0, PPCF_64);
			break;

		case 150:
			dab(in, "stwcx.", 7, 0, 0, 1, 0);
			break;

		case 151:
			dab(in, "stwx", 7, 0, 0, 0, 0);
			break;

		case 181:
			dab(in, "stdux", 7, 0, 0, 0, PPCF_64);
			break;

		case 183:
			dab(in, "stwux", 7, 0, 0, 0, 0);
			break;

		case 200:
		case (PPCOE >> 1) + 200:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "subfze", 6, 0, 1, -1, 0);
			break;

		case 202:
		case (PPCOE >> 1) + 202:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "addze", 6, 0, 1, -1, 0);
			break;

		case 210:
			msr(in, 1); // mfsr
			break;

		case 214:
			dab(in, "stdcx.", 7, 0, 0, 1, PPCF_64);
			break;

		case 215:
			dab(in, "stbx", 7, 0, 0, 0, 0);
			break;

		case 232:
		case (PPCOE >> 1) + 232:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "subfme", 6, 0, 1, -1, 0);
			break;

		case 233:
		case (PPCOE >> 1) + 233:
			dab(in, "mulld", 7, 0, 1, -1, PPCF_64);
			break;

		case 234:
		case (PPCOE >> 1) + 234:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "addme", 6, 0, 1, -1, 0);
			break;

		case 235:
		case (PPCOE >> 1) + 235:
			dab(in, "mullw", 7, 0, 1, -1, 0);
			break;

		case 242:
			if (in & PPCAMASK)
				ill(in);
			else
				dab(in, "mtsrin", 5, 0, 0, 0, PPCF_SUPER);
			break;

		case 246:
			if (in & PPCDMASK)
				ill(in);
			else
				dab(in, "dcbtst", 3, 0, 0, 0, 0);
			break;

		case 247:
			dab(in, "stbux", 7, 0, 0, 0, 0);
			break;

		case 266:
		case (PPCOE >> 1) + 266:
			dab(in, "add", 7, 0, 1, -1, 0);
			break;

		case 278:
			if (in & PPCDMASK)
				ill(in);
			else
				dab(in, "dcbt", 3, 0, 0, 0, 0);
			break;

		case 279:
			dab(in, "lhzx", 7, 0, 0, 0, 0);
			break;

		case 284:
			dab(in, "eqv", 7, 1, 0, -1, 0);
			break;

		case 306:
			if (in & (PPCDMASK | PPCAMASK))
				ill(in);
			else
				dab(in, "tlbie", 1, 0, 0, 0, PPCF_SUPER);
			break;

		case 310:
			dab(in, "eciwx", 7, 0, 0, 0, 0);
			break;

		case 311:
			dab(in, "lhzux", 7, 0, 0, 0, 0);
			break;

		case 316:
			dab(in, "xor", 7, 1, 0, -1, 0);
			break;

		case 339:
			mspr(in, 0); // mfspr
			break;

		case 341:
			dab(in, "lwax", 7, 0, 0, 0, PPCF_64);
			break;

		case 343:
			dab(in, "lhax", 7, 0, 0, 0, 0);
			break;

		case 370:
			nooper(in, "tlbia", PPCF_SUPER);
			break;

		case 371:
			mtb(in); // mftb
			break;

		case 373:
			dab(in, "lwaux", 7, 0, 0, 0, PPCF_64);
			break;

		case 375:
			dab(in, "lhaux", 7, 0, 0, 0, 0);
			break;

		case 407:
			dab(in, "sthx", 7, 0, 0, 0, 0);
			break;

		case 412:
			dab(in, "orc", 7, 1, 0, -1, 0);
			break;

		case 413:
			sradi(in); // sradi
			break;

		case 434:
			if (in & (PPCDMASK | PPCAMASK))
				ill(in);
			else
				dab(in, "slbie", 1, 0, 0, 0, PPCF_SUPER | PPCF_64);
			break;

		case 438:
			dab(in, "ecowx", 7, 0, 0, 0, 0);
			break;

		case 439:
			dab(in, "sthux", 7, 0, 0, 0, 0);
			break;

		case 444:
			if (PPCGETD(in) == PPCGETB(in))
				dab(in, "mr", 6, 1, 0, -1, 0);
			else
				dab(in, "or", 7, 1, 0, -1, 0);
			break;

		case 457:
		case (PPCOE >> 1) + 457:
			dab(in, "divdu", 7, 0, 1, -1, PPCF_64);
			break;

		case 459:
		case (PPCOE >> 1) + 459:
			dab(in, "divwu", 7, 0, 1, -1, 0);
			break;

		case 467:
			mspr(in, 1); // mtspr
			break;

		case 470:
			if (in & PPCDMASK)
				ill(in);
			else
				dab(in, "dcbi", 3, 0, 0, 0, 0);
			break;

		case 476:
			dab(in, "nand", 7, 1, 0, -1, 0);
			break;

		case 489:
		case (PPCOE >> 1) + 489:
			dab(in, "divd", 7, 0, 1, -1, PPCF_64);
			break;

		case 491:
		case (PPCOE >> 1) + 491:
			dab(in, "divw", 7, 0, 1, -1, 0);
			break;

		case 498:
			nooper(in, "slbia", PPCF_SUPER | PPCF_64);
			break;

		case 512:
			if (in & 0x007ff801)
			{
				ill(in);
			}
			else
			{
				m_opcode = "mcrxr";
				m_operands = StringFromFormat("cr%d", (int)PPCGETCRD(in));
			}
			break;

		case 533:
			dab(in, "lswx", 7, 0, 0, 0, 0);
			break;

		case 534:
			dab(in, "lwbrx", 7, 0, 0, 0, 0);
			break;

		case 535:
			fdab(in, "lfsx", 7);
			break;

		case 536:
			dab(in, "srw", 7, 1, 0, -1, 0);
			break;

		case 539:
			dab(in, "srd", 7, 1, 0, -1, PPCF_64);
			break;

		case 566:
			nooper(in, "tlbsync", PPCF_SUPER);
			break;

		case 567:
			fdab(in, "lfsux", 7);
			break;

		case 595:
			msr(in, 0); // mfsr
			break;

		case 597:
			rrn(in, "lswi", 0, 0, 0, 0);
			break;

		case 598:
			nooper(in, "sync", PPCF_SUPER);
			break;

		case 599:
			fdab(in, "lfdx", 7);
			break;

		case 631:
			fdab(in, "lfdux", 7);
			break;

		case 659:
			if (in & PPCAMASK)
				ill(in);
			else
				dab(in, "mfsrin", 5, 0, 0, 0, PPCF_SUPER);
			break;

		case 661:
			dab(in, "stswx", 7, 0, 0, 0, 0);
			break;

		case 662:
			dab(in, "stwbrx", 7, 0, 0, 0, 0);
			break;

		case 663:
			fdab(in, "stfsx", 7);
			break;

		case 695:
			fdab(in, "stfsux", 7);
			break;

		case 725:
			rrn(in, "stswi", 0, 0, 0, 0);
			break;

		case 727:
			fdab(in, "stfdx", 7);
			break;

		case 759:
			fdab(in, "stfdux", 7);
			break;

		case 790:
			dab(in, "lhbrx", 7, 0, 0, 0, 0);
			break;

		case 792:
			dab(in, "sraw", 7, 1, 0, -1, 0);
			break;

		case 794:
			dab(in, "srad", 7, 1, 0, -1, PPCF_64);
			break;

		case 824:
			rrn(in, "srawi", 1, 0, -1, 0);
			break;

		case 854:
			nooper(in, "eieio", PPCF_SUPER);
			break;

		case 918:
			dab(in, "sthbrx", 7, 0, 0, 0, 0);
			break;

		case 922:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "extsh", 6, 1, 0, -1, 0);
			break;

		case 954:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "extsb", 6, 1, 0, -1, 0);
			break;

		case 982:
			if (in & PPCDMASK)
				ill(in);
			else
				dab(in, "icbi", 3, 0, 0, 0, 0);
			break;

		case 983:
			fdab(in, "stfiwx", 7);
			break;

		case 986:
			if (in & PPCBMASK)
				ill(in);
			else
				dab(in, "extsw", 6, 1, 0, -1, PPCF_64);
			break;

		case 1014:
			if (in & PPCDMASK)
				ill(in);
			else
				dab(in, "dcbz", 3, 0, 0, 0, 0);
			break;

		default:
			ill(in);
			break;
		}
		break;

	case 32:
	case 33:
	case 34:
	case 35:
	case 36:
	case 37:
	case 38:
	case 39:
	case 40:
	case 41:
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
		ldst(in, ldstnames[PPCGETIDX(in) - 32], 'r', 0);
		break;

	case 48:
	case 49:
	case 50:
	case 51:
	case 52:
	case 53:
	case 54:
	case 55:
		ldst(in, ldstnames[PPCGETIDX(in) - 32], 'f', 0);
		break;

	case 58:
		switch (in & 3)
		{
		case 0:
			ldst(in&~3, "ld", 'r', PPCF_64);
			break;
		case 1:
			ldst(in&~3, "ldu", 'r', PPCF_64);
			break;
		case 2:
			ldst(in&~3, "lwa", 'r', PPCF_64);
			break;
		default:
			ill(in);
			break;
		}
		break;

	case 59:
		switch (in & 0x3e)
		{
		case 36:
			fdabc(in, "divs", 5, 0);
			break;

		case 40:
			fdabc(in, "subs", 5, 0);
			break;

		case 42:
			fdabc(in, "adds", 5, 0);
			break;

		case 44:
			fdabc(in, "sqrts", 1, 0);
			break;

		case 48:
			fdabc(in, "res", 1, 0);
			break;

		case 50:
			fdabc(in, "muls", 6, 0);
			break;

		case 56:
			fdabc(in, "msubs", 7, 0);
			break;

		case 58:
			fdabc(in, "madds", 7, 0);
			break;

		case 60:
			fdabc(in, "nmsubs", 7, 0);
			break;

		case 62:
			fdabc(in, "nmadds", 7, 0);
			break;

		default:
			ill(in);
			break;
		}
		break;

	case 62:
		switch (in & 3)
		{
		case 0:
			ldst(in&~3, "std", 'r', PPCF_64);
			break;
		case 1:
			ldst(in&~3, "stdu", 'r', PPCF_64);
			break;
		default:
			ill(in);
			break;
		}
		break;

	case 63:
		if (in & 32)
		{
			switch (in & 0x1e)
			{
			case 4:
				fdabc(in, "div", 5, 0);
				break;

			case 8:
				fdabc(in, "sub", 5, 0);
				break;

			case 10:
				fdabc(in, "add", 5, 0);
				break;

			case 12:
				fdabc(in, "sqrt", 1, 0);
				break;

			case 14:
				fdabc(in, "sel", 7, 0);
				break;

			case 18:
				fdabc(in, "mul", 6, 0);
				break;

			case 20:
				fdabc(in, "rsqrte", 1, 0);
				break;

			case 24:
				fdabc(in, "msub", 7, 0);
				break;

			case 26:
				fdabc(in, "madd", 7, 0);
				break;

			case 28:
				fdabc(in, "nmsub", 7, 0);
				break;

			case 30:
				fdabc(in, "nmadd", 7, 0);
				break;

			default:
				ill(in);
				break;
			}
		}
		else
		{
			switch (PPCGETIDX2(in))
			{
			case 0:
				fcmp(in, 'u');
				break;

			case 12:
				fdabc(in, "rsp", 1, 0);
				break;

			case 14:
				fdabc(in, "ctiw", 1, 0);
				break;

			case 15:
				fdabc(in, "ctiwz", 1, 0);
				break;

			case 32:
				fcmp(in, 'o');
				break;

			case 38:
				mtfsb(in, 1);
				break;

			case 40:
				fdabc(in, "neg", 10, 0);
				break;

			case 64:
				mcrf(in, 's'); // mcrfs
				break;

			case 70:
				mtfsb(in, 0);
				break;

			case 72:
				fmr(in);
				break;

			case 134:
				if ((in & 0x006f0800) == 0)
				{
					m_opcode = StringFromFormat("mtfsfi%s", rcsel[in & 1]);
					m_operands = StringFromFormat("cr%d,%d", (int)PPCGETCRD(in), (int)(in & 0xf000) >> 12);
				}
				else
				{
					ill(in);
				}
				break;

			case 136:
				fdabc(in, "nabs", 10, 0);
				break;

			case 264:
				fdabc(in, "abs", 10, 0);
				break;

			case 583:
				if (in & (PPCAMASK | PPCBMASK))
					ill(in);
				else
					dab(in, "mffs", 4, 0, 0, -1, 0);
				break;

			case 711:
				if ((in & 0x02010000) == 0)
				{
					m_opcode = StringFromFormat("mtfsf%s", rcsel[in & 1]);
					m_operands = StringFromFormat("0x%x,%u", (unsigned int)(in & 0x01fe) >> 17, (int)PPCGETB(in));
				}
				else
				{
					ill(in);
				}
				break;

			case 814:
				fdabc(in, "fctid", 10, PPCF_64);
				break;

			case 815:
				fdabc(in, "fctidz", 10, PPCF_64);
				break;

			case 846:
				fdabc(in, "fcfid", 10, PPCF_64);
				break;

			default:
				ill(in);
				break;
			}
		}
		break;

	default:
		ill(in);
		break;
	}
	return (m_instr + 1);
}

// simplified interface
std::string GekkoDisassembler::Disassemble(u32 opcode, u32 current_instruction_address, bool big_endian)
{
	u32 opc = opcode;
	u32 addr = current_instruction_address;

	m_instr = (u32*)&opc;
	m_iaddr = (u32*)&addr;

	DoDisassembly(big_endian);

	return m_opcode.append("\t").append(m_operands);
}

static const char* gprnames[] =
{
	" r0", " r1", " r2", " r3", " r4", " r5", " r6", " r7",
	" r8", " r9", "r10", "r11", "r12", "r13", "r14", "r15",
	"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
	"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
};

const char* GekkoDisassembler::GetGPRName(u32 index)
{
	if (index < 32)
		return gprnames[index];

	return 0;
}

static const char* fprnames[] =
{
	" f0", " f1", " f2", " f3", " f4", " f5", " f6", " f7",
	" f8", " f9", "f10", "f11", "f12", "f13", "f14", "f15",
	"f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
	"f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"
};

const char* GekkoDisassembler::GetFPRName(u32 index)
{
	if (index < 32)
		return fprnames[index];

	return 0;
}
