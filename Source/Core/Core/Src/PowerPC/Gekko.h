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
// Gekko related unions, structs, ...
//

#ifndef _GEKKO_H
#define _GEKKO_H

#include "Common.h"


// --- Gekko Instruction ---


union UGeckoInstruction
{
	u32 hex;

	UGeckoInstruction(u32 _hex)	{ hex = _hex;}
	UGeckoInstruction()			{ hex = 0;}

	struct
	{
		unsigned Rc		:	1;
		unsigned SUBOP10:	10;
		unsigned RB		:	5;
		unsigned RA		:	5;
		unsigned RD		:	5;
		unsigned OPCD	:	6;
	}; // changed
	struct
	{
		signed SIMM_16	:	16;
		unsigned 		:	5;
		unsigned TO		:	5;
		unsigned OPCD_2	:	6;
	};
	struct
	{
		unsigned Rc_2	:	1;
		unsigned		:	10;
		unsigned 		:	5;
		unsigned 		:	5;
		unsigned RS		:	5;
		unsigned OPCD_3	:	6;
	};
	struct
	{
		unsigned UIMM	:	16;
		unsigned 		:	5;
		unsigned 		:	5;
		unsigned OPCD_4	:	6;
	};
	struct
	{
		unsigned LK		:	1;
		unsigned AA		:	1;
		unsigned LI		:	24;
		unsigned OPCD_5	:	6;
	};
	struct
	{     
		unsigned LK_2	:	1;
		unsigned AA_2	:	1;
		unsigned BD		:	14;
		unsigned BI		:	5;
		unsigned BO		:	5;
		unsigned OPCD_6	:	6;
	};
	struct
	{
		unsigned LK_3	:	1;
		unsigned		:	10;
		unsigned		:	5;
		unsigned BI_2	:	5;
		unsigned BO_2	:	5;
		unsigned OPCD_7	:	6;
	};
	struct
	{
		unsigned		:	11;
		unsigned RB_2	:	5;
		unsigned RA_2	:	5;
		unsigned L		:	1;
		unsigned 		:	1;
		unsigned CRFD	:	3;
		unsigned OPCD_8	:	6;
	};
	struct
	{
		signed  SIMM_16_2	:	16;
		unsigned RA_3	:	5;
		unsigned L_2	:	1;
		unsigned		:	1;
		unsigned CRFD_2	:	3;
		unsigned OPCD_9	:	6;
	};
	struct
	{     
		unsigned UIMM_2	:	16;
		unsigned RA_4	:	5;
		unsigned L_3	:	1;
		unsigned dummy2	:	1;
		unsigned CRFD_3	:	3;
		unsigned OPCD_A	:	6;
	};
	struct
	{
		unsigned		:	1;
		unsigned SUBOP10_2:	10;
		unsigned RB_5	:	5;
		unsigned RA_5	:	5;
		unsigned L_4	:	1;
		unsigned dummy3	:	1;
		unsigned CRFD_4	:	3;
		unsigned OPCD_B	:	6;
	};
	struct
	{
		unsigned		:	16;
		unsigned SR		:	4;
		unsigned 		:	1;
		unsigned RS_2	:	5;
		unsigned OPCD_C	:	6;
	};

	// Table 59
	struct
	{
		unsigned Rc_4	:	1;
		unsigned SUBOP5	:	5;
		unsigned RC		:	5;
		unsigned		:	5;
		unsigned RA_6	:	5;
		unsigned RD_2	:	5;
		unsigned OPCD_D	:	6;
	};

	struct
	{	unsigned		:	10;
		unsigned OE		:	1;
		unsigned SPR	:	10;
		unsigned		:	11;
	};
	struct
	{
		unsigned		:	10;
		unsigned OE_3	:	1;
		unsigned SPRU	:	5;
		unsigned SPRL	:	5;
		unsigned		:	11;
	};

	// rlwinmx
	struct
	{
		unsigned Rc_3	:	1;
		unsigned ME		:	5;
		unsigned MB		:	5;
		unsigned SH		:	5;
		unsigned		:	16;
	};

	// crxor
	struct 
	{
		unsigned		:	11;
		unsigned CRBB	:	5;
		unsigned CRBA	:	5;
		unsigned CRBD	:	5;		
		unsigned		:	6;
	};

	// mftb
	struct 
	{
		unsigned		:	11;
		unsigned TBR	:	10;
		unsigned		:	11;
	};

	struct 
	{
		unsigned		:	11;
		unsigned TBRU	:	5;
		unsigned TBRL	:	5;
		unsigned		:	11;
	};

	struct 
	{
		unsigned		:	18;
		unsigned CRFS	:	3;
		unsigned 		:	2;
		unsigned CRFD_5	:	3;
		unsigned		:	6;
	};

	// float
	struct 
	{
		unsigned		:	12;
		unsigned CRM	:	8;
		unsigned		:	1;
		unsigned FD		:	5;
		unsigned		:	6;
	};
	struct 
	{
		unsigned		:	6;
		unsigned FC		:	5;
		unsigned FB		:	5;
		unsigned FA		:	5;
		unsigned FS		:	5;
		unsigned		:	6;
	};
	struct 
	{
		unsigned OFS    :	16;
		unsigned        :	16;
	};
	struct
	{
		unsigned        :   17;
		unsigned FM     :   8;
		unsigned        :   7;
	};

	// paired
	struct 
	{
		unsigned	    :	7;
		unsigned Ix     :	3;
		unsigned Wx     :	1;
		unsigned	     :	1;
		unsigned I		:   3;
		unsigned W		:   1;
		unsigned	    :	16;
	};

	struct 
	{
		signed	SIMM_12	:	12;
		unsigned		:	20;
	};

	struct 
	{
		unsigned dummyX :   11;
		unsigned NB : 5;
	};
};


//
// --- Gekko Special Registers ---
//


// GQR Register
union UGQR
{
	u32 Hex;
	struct 
	{
		unsigned	ST_TYPE		: 3;
		unsigned				: 5;
		unsigned	ST_SCALE	: 6;
		unsigned				: 2;
		unsigned	LD_TYPE		: 3;
		unsigned				: 5;
		unsigned	LD_SCALE	: 6;
		unsigned				: 2;
	};

	UGQR(u32 _hex) { Hex = _hex; }
	UGQR()		{Hex = 0;  }
};

// FPU Register
union UFPR
{
	u64	as_u64;
	s64	as_s64;
	double	d;
	u32	as_u32[2];
	s32	as_s32[2];
	float	f[2];
};

#define XER_CA_MASK 0x20000000
// XER
union UReg_XER
{
	struct 
	{
		unsigned BYTE_COUNT	: 7;
		unsigned			: 22;
		unsigned CA			: 1;
		unsigned OV			: 1;
		unsigned SO			: 1;
	};
	u32 Hex;

	UReg_XER(u32 _hex) { Hex = _hex; }
	UReg_XER()		{ Hex = 0; }	
};

// Machine State Register
union UReg_MSR
{
	struct
	{
		unsigned LE		:	1;
		unsigned RI		:	1;
		unsigned PM		:	1;
		unsigned 		:	1; // res28
		unsigned DR		:	1;
		unsigned IR		:	1;
		unsigned IP		:	1;
		unsigned 		:	1; // res24
		unsigned FE1	:	1;
		unsigned BE 	:	1;
		unsigned SE  	:	1;
		unsigned FE0	:	1;
		unsigned MCHECK	:	1;
		unsigned FP		:	1;
		unsigned PR		:	1;
		unsigned EE		:	1;
		unsigned ILE	:	1;
		unsigned		:	1; // res14
		unsigned POW	:	1;
		unsigned res	:	13;
	};
	u32 Hex;

	UReg_MSR(u32 _hex) { Hex = _hex; }
	UReg_MSR()		{ Hex = 0; }	
};

// Floating Point Status and Control Register
union UReg_FPSCR
{
	struct
	{
		unsigned RN		:	2;
		unsigned NI		:	1;
		unsigned XE		:	1;
		unsigned ZE		:	1;
		unsigned UE		:	1;
		unsigned OE		:	1;
		unsigned VE		:	1;
		unsigned VXCVI	:	1;
		unsigned VXSQRT	:	1;
		unsigned VXSOFT	:	1;
		unsigned		:	1;
		unsigned FPRF	:	5;
		unsigned FI		:	1;
		unsigned FR		:	1;
		unsigned VXVC	:	1;
		unsigned VXIMZ	:	1;
		unsigned VXZDZ	:	1;
		unsigned VXIDI	:	1;
		unsigned VXISI	:	1;
		unsigned VXSNAN	:	1;
		unsigned XX		:	1;
		unsigned ZX		:	1;
		unsigned UX		:	1;
		unsigned OX		:	1;
		unsigned VX		:	1;
		unsigned FEX	:	1;
		unsigned FX		:	1;
	};
	u32 Hex;

	UReg_FPSCR(u32 _hex)	{ Hex = _hex; }
	UReg_FPSCR()		{ Hex = 0;}
};

// Hardware Implementation-Dependent Register 0
union UReg_HID0
{
	struct
	{
		unsigned NOOPTI		:	1;
		unsigned			:	1;
		unsigned BHT		:	1;
		unsigned ABE		:	1;
		unsigned			:	1;
		unsigned BTIC		:	1;
		unsigned DCFA		:	1;
		unsigned SGE		:	1;
		unsigned IFEM		:	1;
		unsigned SPD		:	1;
		unsigned DCFI		:	1;
		unsigned ICFI		:	1;
		unsigned DLOCK		:	1;
		unsigned ILOCK		:	1;
		unsigned DCE		:	1;
		unsigned ICE		:	1;
		unsigned NHR		:	1;
		unsigned			:	3;
		unsigned DPM		:	1;
		unsigned SLEEP		:	1;
		unsigned NAP		:	1;
		unsigned DOZE		:	1;
		unsigned PAR		:	1;
		unsigned ECLK		:	1;
		unsigned			:	1;
		unsigned BCLK		:	1;
		unsigned EBD		:	1;
		unsigned EBA		:	1;
		unsigned DBP		:	1;
		unsigned EMCP		:	1;
	};
	u32 Hex;
};

// Hardware Implementation-Dependent Register 2
union UReg_HID2
{
	struct
	{
		unsigned			:	16;
		unsigned DQOMEE		:	1;
		unsigned DCMEE		:	1;
		unsigned DNCEE		:	1;
		unsigned DCHEE		:	1;
		unsigned DQOERR		:	1;
		unsigned DCEMERR	:	1;
		unsigned DNCERR		:	1;
		unsigned DCHERR		:	1;
		unsigned DMAQL		:	4;
		unsigned LCE		:	1;
		unsigned PSE		:	1;
		unsigned WPE		:	1;
		unsigned LSQE		:	1;
	};
	u32 Hex;

	UReg_HID2(u32 _hex)	{ Hex = _hex; }
	UReg_HID2()			{ Hex = 0; }
};

// SPR1 - Page Table format
union UReg_SPR1
{
	u32 Hex;
	struct 
	{
		unsigned htaborg  : 16;
		unsigned          : 7;
		unsigned htabmask : 9;
	};
};



// Write Pipe Address Register
union UReg_WPAR
{
	struct
	{
		unsigned BNE		: 1;
		unsigned			: 4;
		unsigned GB_ADDR	: 27;
	};
	u32 Hex;

	UReg_WPAR(u32 _hex)	{ Hex = _hex; }
	UReg_WPAR()			{ Hex = 0; }
};

// Direct Memory Access Upper register
union UReg_DMAU
{
	struct
	{
		unsigned DMA_LEN_U	:	5;
		unsigned MEM_ADDR	:	27;		
	};
	u32 Hex;

	UReg_DMAU(u32 _hex)	{ Hex = _hex; }
	UReg_DMAU()			{ Hex = 0; }
};

// Direct Memory Access Lower (DMAL) register
union UReg_DMAL
{
	struct
	{
		unsigned DMA_F		:	1;
		unsigned DMA_T		:	1;
		unsigned DMA_LEN_L	:	2;
		unsigned DMA_LD		:	1;
		unsigned LC_ADDR	:	27;		
	};
	u32 Hex;

	UReg_DMAL(u32 _hex)	{ Hex = _hex; }
	UReg_DMAL()			{ Hex = 0; }
};

union UReg_BAT_Up
{
	struct 
	{
		unsigned VP         :   1;
		unsigned VS         :   1;
		unsigned BL			:	11;
		unsigned			:	4;
		unsigned BEPI		:	15;		
	};
	u32 Hex;

	UReg_BAT_Up(u32 _hex)	{ Hex = _hex; }
	UReg_BAT_Up()		{ Hex = 0; }
};

union UReg_BAT_Lo
{
	struct 
	{
		unsigned PP         :   2;
		unsigned            :   1;
		unsigned WIMG		:	4;
		unsigned			:	10;
		unsigned BRPN		:	15;
	};
	u32 Hex;

	UReg_BAT_Lo(u32 _hex)	{ Hex = _hex; }
	UReg_BAT_Lo()		{ Hex = 0; }
};

union UReg_PTE
{
	struct 
	{
		unsigned API		:	6;
		unsigned H			:	1;
		unsigned VSID		:	24;
		unsigned V			:	1;
		unsigned PP			:	2;
		unsigned			:	1;
		unsigned WIMG		:	4;
		unsigned C			:	1;
		unsigned R			:	1;
		unsigned			:   3;
		unsigned RPN		:	20;

	};

	u64 Hex;
	u32 Hex32[2];
};


//
// --- Gekko Types and Defs ---
//


// quantize types
enum EQuantizeType
{
	QUANTIZE_FLOAT	=	0,
	QUANTIZE_U8		=	4,
	QUANTIZE_U16	=	5,
	QUANTIZE_S8		=	6,
	QUANTIZE_S16	=	7,
};

// branches
enum 
{
	BO_BRANCH_IF_CTR_0		=  2, // 3
	BO_DONT_DECREMENT_FLAG	=  4, // 2
	BO_BRANCH_IF_TRUE		=  8, // 1
	BO_DONT_CHECK_CONDITION	= 16, // 0
};

// Special purpose register indices
enum
{
	SPR_XER   =	1,
	SPR_LR    = 8,
	SPR_CTR   = 9,
	SPR_DSISR = 18,
	SPR_DAR	  = 19,
	SPR_DEC   = 22,
	SPR_SDR	  = 25,
	SPR_SRR0  = 26,
	SPR_SRR1  = 27,
	SPR_TL	  = 268,
	SPR_TU    = 269,
	SPR_TL_W  = 284,
	SPR_TU_W  = 285,
	SPR_PVR   = 287,
	SPR_SPRG0 = 272,
	SPR_SPRG1 = 273,
	SPR_SPRG2 = 274,
	SPR_SPRG3 = 275,
	SPR_IBAT0U  = 528,
	SPR_IBAT0L  = 529,
	SPR_IBAT1U  = 530,
	SPR_IBAT1L  = 531,
	SPR_IBAT2U  = 532,
	SPR_IBAT2L  = 533,
	SPR_IBAT3L  = 534,
	SPR_IBAT3U  = 535,
	SPR_DBAT0U  = 536,
	SPR_DBAT0L  = 537,
	SPR_DBAT1U  = 538,
	SPR_DBAT1L  = 539,
	SPR_DBAT2U  = 540,
	SPR_DBAT2L  = 541,
	SPR_DBAT3L  = 542,
	SPR_DBAT3U  = 543,
	SPR_GQR0  = 912,
	SPR_HID0  = 1008,
	SPR_HID1  = 1009,
	SPR_HID2  = 920,
	SPR_WPAR  = 921,
	SPR_DMAU  = 922,
	SPR_DMAL  = 923,
	SPR_ECID_U = 924,
	SPR_ECID_M = 925,
	SPR_ECID_L = 926,
	SPR_L2CR  = 1017
};

// Exceptions
#define EXCEPTION_DECREMENTER			0x00000001
#define EXCEPTION_SYSCALL				0x00000002
#define EXCEPTION_EXTERNAL_INT			0x00000004
#define EXCEPTION_DSI					0x00000008
#define EXCEPTION_ISI					0x00000010
#define EXCEPTION_ALIGNMENT				0x00000020
#define EXCEPTION_FPU_UNAVAILABLE       0x00000040
#define EXCEPTION_PROGRAM				0x00000080

inline s32 SignExt16(s16 x) {return (s32)(s16)x;}
inline s32 SignExt26(u32 x) {return x & 0x2000000 ? (s32)x | 0xFC000000 : (s32)(x);}

#endif

