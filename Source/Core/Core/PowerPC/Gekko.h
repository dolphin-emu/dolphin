// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Gekko related unions, structs, ...

#pragma once

#include "Common/BitField.h"
#include "Common/CommonTypes.h"

// --- Gekko Instruction ---

union UGeckoInstruction
{
  u32 hex;

  UGeckoInstruction(u32 _hex) : hex(_hex) {}
  UGeckoInstruction() : hex(0) {}
  struct
  {
    // Record bit
    // 1, if the condition register should be updated by this instruction
    u32 Rc : 1;
    u32 SUBOP10 : 10;
    // Source GPR
    u32 RB : 5;
    // Source or destination GPR
    u32 RA : 5;
    // Destination GPR
    u32 RD : 5;
    // Primary opcode
    u32 OPCD : 6;
  };
  struct
  {
    // Immediate, signed 16-bit
    signed SIMM_16 : 16;
    u32 : 5;
    // Conditions on which to trap
    u32 TO : 5;
    u32 OPCD_2 : 6;
  };
  struct
  {
    u32 Rc_2 : 1;
    u32 : 10;
    u32 : 5;
    u32 : 5;
    // Source GPR
    u32 RS : 5;
    u32 OPCD_3 : 6;
  };
  struct
  {
    // Immediate, unsigned 16-bit
    u32 UIMM : 16;
    u32 : 5;
    u32 : 5;
    u32 OPCD_4 : 6;
  };
  struct
  {
    // Link bit
    // 1, if branch instructions should put the address of the next instruction into the link
    // register
    u32 LK : 1;
    // Absolute address bit
    // 1, if the immediate field represents an absolute address
    u32 AA : 1;
    // Immediate, signed 24-bit
    u32 LI : 24;
    u32 OPCD_5 : 6;
  };
  struct
  {
    u32 LK_2 : 1;
    u32 AA_2 : 1;
    // Branch displacement, signed 14-bit (right-extended by 0b00)
    u32 BD : 14;
    // Branch condition
    u32 BI : 5;
    // Conditional branch control
    u32 BO : 5;
    u32 OPCD_6 : 6;
  };
  struct
  {
    u32 LK_3 : 1;
    u32 : 10;
    u32 : 5;
    u32 BI_2 : 5;
    u32 BO_2 : 5;
    u32 OPCD_7 : 6;
  };
  struct
  {
    u32 : 11;
    u32 RB_2 : 5;
    u32 RA_2 : 5;
    // ?
    u32 L : 1;
    u32 : 1;
    // Destination field in CR or FPSCR
    u32 CRFD : 3;
    u32 OPCD_8 : 6;
  };
  struct
  {
    signed SIMM_16_2 : 16;
    u32 RA_3 : 5;
    u32 L_2 : 1;
    u32 : 1;
    u32 CRFD_2 : 3;
    u32 OPCD_9 : 6;
  };
  struct
  {
    u32 UIMM_2 : 16;
    u32 RA_4 : 5;
    u32 L_3 : 1;
    u32 : 1;
    u32 CRFD_3 : 3;
    u32 OPCD_A : 6;
  };
  struct
  {
    u32 : 1;
    u32 SUBOP10_2 : 10;
    u32 RB_5 : 5;
    u32 RA_5 : 5;
    u32 L_4 : 1;
    u32 : 1;
    u32 CRFD_4 : 3;
    u32 OPCD_B : 6;
  };
  struct
  {
    u32 : 16;
    // Segment register
    u32 SR : 4;
    u32 : 1;
    u32 RS_2 : 5;
    u32 OPCD_C : 6;
  };

  // Table 59
  struct
  {
    u32 Rc_4 : 1;
    u32 SUBOP5 : 5;
    // ?
    u32 RC : 5;
    u32 : 5;
    u32 RA_6 : 5;
    u32 RD_2 : 5;
    u32 OPCD_D : 6;
  };

  struct
  {
    u32 : 10;
    // Overflow enable
    u32 OE : 1;
    // Special-purpose register
    u32 SPR : 10;
    u32 : 11;
  };
  struct
  {
    u32 : 10;
    u32 OE_3 : 1;
    // Upper special-purpose register
    u32 SPRU : 5;
    // Lower special-purpose register
    u32 SPRL : 5;
    u32 : 11;
  };

  // rlwinmx
  struct
  {
    u32 Rc_3 : 1;
    // Mask end
    u32 ME : 5;
    // Mask begin
    u32 MB : 5;
    // Shift amount
    u32 SH : 5;
    u32 : 16;
  };

  // crxor
  struct
  {
    u32 : 11;
    // Source bit in the CR
    u32 CRBB : 5;
    // Source bit in the CR
    u32 CRBA : 5;
    // Destination bit in the CR
    u32 CRBD : 5;
    u32 : 6;
  };

  // mftb
  struct
  {
    u32 : 11;
    // Time base register
    u32 TBR : 10;
    u32 : 11;
  };

  struct
  {
    u32 : 11;
    // Upper time base register
    u32 TBRU : 5;
    // Lower time base register
    u32 TBRL : 5;
    u32 : 11;
  };

  struct
  {
    u32 : 18;
    // Source field in the CR or FPSCR
    u32 CRFS : 3;
    u32 : 2;
    u32 CRFD_5 : 3;
    u32 : 6;
  };

  struct
  {
    u32 : 12;
    // Field mask, identifies the CR fields to be updated by mtcrf
    u32 CRM : 8;
    u32 : 1;
    // Destination FPR
    u32 FD : 5;
    u32 : 6;
  };
  struct
  {
    u32 : 6;
    // Source FPR
    u32 FC : 5;
    // Source FPR
    u32 FB : 5;
    // Source FPR
    u32 FA : 5;
    // Source FPR
    u32 FS : 5;
    u32 : 6;
  };
  struct
  {
    u32 : 17;
    // Field mask, identifies the FPSCR fields to be updated by mtfsf
    u32 FM : 8;
    u32 : 7;
  };

  // paired single quantized load/store
  struct
  {
    u32 : 1;
    u32 SUBOP6 : 6;
    // Graphics quantization register to use
    u32 Ix : 3;
    // 0: paired single, 1: scalar
    u32 Wx : 1;
    u32 : 1;
    // Graphics quantization register to use
    u32 I : 3;
    // 0: paired single, 1: scalar
    u32 W : 1;
    u32 : 16;
  };

  struct
  {
    signed SIMM_12 : 12;
    u32 : 20;
  };

  struct
  {
    u32 : 11;
    // Number of bytes to use in lswi/stswi (0 means 32 bytes)
    u32 NB : 5;
  };
};

//
// --- Gekko Special Registers ---
//

// quantize types
enum EQuantizeType : u32
{
  QUANTIZE_FLOAT = 0,
  QUANTIZE_INVALID1 = 1,
  QUANTIZE_INVALID2 = 2,
  QUANTIZE_INVALID3 = 3,
  QUANTIZE_U8 = 4,
  QUANTIZE_U16 = 5,
  QUANTIZE_S8 = 6,
  QUANTIZE_S16 = 7,
};

// GQR Register
union UGQR
{
  BitField<0, 3, EQuantizeType> st_type;
  BitField<8, 6, u32> st_scale;
  BitField<16, 3, EQuantizeType> ld_type;
  BitField<24, 6, u32> ld_scale;

  u32 Hex;

  UGQR(u32 _hex) { Hex = _hex; }
  UGQR() { Hex = 0; }
};

// FPU Register
union UFPR
{
  u64 as_u64;
  s64 as_s64;
  double d;
  u32 as_u32[2];
  s32 as_s32[2];
  float f[2];
};

#define XER_CA_SHIFT 29
#define XER_OV_SHIFT 30
#define XER_SO_SHIFT 31
#define XER_OV_MASK 1
#define XER_SO_MASK 2
// XER
union UReg_XER
{
  struct
  {
    u32 BYTE_COUNT : 7;
    u32 : 1;
    u32 BYTE_CMP : 8;
    u32 : 13;
    u32 CA : 1;
    u32 OV : 1;
    u32 SO : 1;
  };
  u32 Hex;

  UReg_XER(u32 _hex) { Hex = _hex; }
  UReg_XER() { Hex = 0; }
};

// Machine State Register
union UReg_MSR
{
  struct
  {
    u32 LE : 1;
    u32 RI : 1;
    u32 PM : 1;
    u32 : 1;  // res28
    u32 DR : 1;
    u32 IR : 1;
    u32 IP : 1;
    u32 : 1;  // res24
    u32 FE1 : 1;
    u32 BE : 1;
    u32 SE : 1;
    u32 FE0 : 1;
    u32 MCHECK : 1;
    u32 FP : 1;
    u32 PR : 1;
    u32 EE : 1;
    u32 ILE : 1;
    u32 : 1;  // res14
    u32 POW : 1;
    u32 res : 13;
  };
  u32 Hex;

  UReg_MSR(u32 _hex) { Hex = _hex; }
  UReg_MSR() { Hex = 0; }
};

#define FPRF_SHIFT 12
#define FPRF_MASK (0x1F << FPRF_SHIFT)

// FPSCR exception flags
enum FPSCRExceptionFlag : u32
{
  FPSCR_FX = 1U << (31 - 0),
  FPSCR_FEX = 1U << (31 - 1),
  FPSCR_VX = 1U << (31 - 2),
  FPSCR_OX = 1U << (31 - 3),
  FPSCR_UX = 1U << (31 - 4),
  FPSCR_ZX = 1U << (31 - 5),
  FPSCR_XX = 1U << (31 - 6),
  FPSCR_VXSNAN = 1U << (31 - 7),
  FPSCR_VXISI = 1U << (31 - 8),
  FPSCR_VXIDI = 1U << (31 - 9),
  FPSCR_VXZDZ = 1U << (31 - 10),
  FPSCR_VXIMZ = 1U << (31 - 11),
  FPSCR_VXVC = 1U << (31 - 12),
  FPSCR_VXSOFT = 1U << (31 - 21),
  FPSCR_VXSQRT = 1U << (31 - 22),
  FPSCR_VXCVI = 1U << (31 - 23),
  FPSCR_VE = 1U << (31 - 24),

  FPSCR_VX_ANY = FPSCR_VXSNAN | FPSCR_VXISI | FPSCR_VXIDI | FPSCR_VXZDZ | FPSCR_VXIMZ | FPSCR_VXVC |
                 FPSCR_VXSOFT | FPSCR_VXSQRT | FPSCR_VXCVI,

  FPSCR_ANY_X = FPSCR_OX | FPSCR_UX | FPSCR_ZX | FPSCR_XX | FPSCR_VX_ANY,
};

// Floating Point Status and Control Register
union UReg_FPSCR
{
  struct
  {
    // Rounding mode (towards: nearest, zero, +inf, -inf)
    u32 RN : 2;
    // Non-IEEE mode enable (aka flush-to-zero)
    u32 NI : 1;
    // Inexact exception enable
    u32 XE : 1;
    // IEEE division by zero exception enable
    u32 ZE : 1;
    // IEEE underflow exception enable
    u32 UE : 1;
    // IEEE overflow exception enable
    u32 OE : 1;
    // Invalid operation exception enable
    u32 VE : 1;
    // Invalid operation exception for integer conversion (sticky)
    u32 VXCVI : 1;
    // Invalid operation exception for square root (sticky)
    u32 VXSQRT : 1;
    // Invalid operation exception for software request (sticky)
    u32 VXSOFT : 1;
    // reserved
    u32 : 1;
    // Floating point result flags (includes FPCC) (not sticky)
    // from more to less significand: class, <, >, =, ?
    u32 FPRF : 5;
    // Fraction inexact (not sticky)
    u32 FI : 1;
    // Fraction rounded (not sticky)
    u32 FR : 1;
    // Invalid operation exception for invalid comparison (sticky)
    u32 VXVC : 1;
    // Invalid operation exception for inf * 0 (sticky)
    u32 VXIMZ : 1;
    // Invalid operation exception for 0 / 0 (sticky)
    u32 VXZDZ : 1;
    // Invalid operation exception for inf / inf (sticky)
    u32 VXIDI : 1;
    // Invalid operation exception for inf - inf (sticky)
    u32 VXISI : 1;
    // Invalid operation exception for SNaN (sticky)
    u32 VXSNAN : 1;
    // Inexact exception (sticky)
    u32 XX : 1;
    // Division by zero exception (sticky)
    u32 ZX : 1;
    // Underflow exception (sticky)
    u32 UX : 1;
    // Overflow exception (sticky)
    u32 OX : 1;
    // Invalid operation exception summary (not sticky)
    u32 VX : 1;
    // Enabled exception summary (not sticky)
    u32 FEX : 1;
    // Exception summary (sticky)
    u32 FX : 1;
  };
  u32 Hex;

  UReg_FPSCR(u32 _hex) { Hex = _hex; }
  UReg_FPSCR() { Hex = 0; }
};

// Hardware Implementation-Dependent Register 0
union UReg_HID0
{
  struct
  {
    u32 NOOPTI : 1;
    u32 : 1;
    u32 BHT : 1;
    u32 ABE : 1;
    u32 : 1;
    u32 BTIC : 1;
    u32 DCFA : 1;
    u32 SGE : 1;
    u32 IFEM : 1;
    u32 SPD : 1;
    u32 DCFI : 1;
    u32 ICFI : 1;
    u32 DLOCK : 1;
    u32 ILOCK : 1;
    u32 DCE : 1;
    u32 ICE : 1;
    u32 NHR : 1;
    u32 : 3;
    u32 DPM : 1;
    u32 SLEEP : 1;
    u32 NAP : 1;
    u32 DOZE : 1;
    u32 PAR : 1;
    u32 ECLK : 1;
    u32 : 1;
    u32 BCLK : 1;
    u32 EBD : 1;
    u32 EBA : 1;
    u32 DBP : 1;
    u32 EMCP : 1;
  };
  u32 Hex;
};

// Hardware Implementation-Dependent Register 2
union UReg_HID2
{
  struct
  {
    u32 : 16;
    u32 DQOEE : 1;
    u32 DCMEE : 1;
    u32 DNCEE : 1;
    u32 DCHEE : 1;
    u32 DQOERR : 1;
    u32 DCMERR : 1;
    u32 DNCERR : 1;
    u32 DCHERR : 1;
    u32 DMAQL : 4;
    u32 LCE : 1;
    u32 PSE : 1;
    u32 WPE : 1;
    u32 LSQE : 1;
  };
  u32 Hex;

  UReg_HID2(u32 _hex) { Hex = _hex; }
  UReg_HID2() { Hex = 0; }
};

// Hardware Implementation-Dependent Register 4
union UReg_HID4
{
  struct
  {
    u32 : 20;
    u32 L2CFI : 1;
    u32 L2MUM : 1;
    u32 DBP : 1;
    u32 LPE : 1;
    u32 ST0 : 1;
    u32 SBE : 1;
    u32 : 1;
    u32 BPD : 2;
    u32 L2FM : 2;
    u32 : 1;
  };
  u32 Hex;

  UReg_HID4(u32 _hex) { Hex = _hex; }
  UReg_HID4() { Hex = 0; }
};

// SPR1 - Page Table format
union UReg_SPR1
{
  u32 Hex;
  struct
  {
    u32 htaborg : 16;
    u32 : 7;
    u32 htabmask : 9;
  };
};

// MMCR0 - Monitor Mode Control Register 0 format
union UReg_MMCR0
{
  u32 Hex;
  struct
  {
    u32 PMC2SELECT : 6;
    u32 PMC1SELECT : 7;
    u32 PMCTRIGGER : 1;
    u32 PMCINTCONTROL : 1;
    u32 PMC1INTCONTROL : 1;
    u32 THRESHOLD : 6;
    u32 INTONBITTRANS : 1;
    u32 RTCSELECT : 2;
    u32 DISCOUNT : 1;
    u32 ENINT : 1;
    u32 DMR : 1;
    u32 DMS : 1;
    u32 DU : 1;
    u32 DP : 1;
    u32 DIS : 1;
  };
};

// MMCR1 - Monitor Mode Control Register 1 format
union UReg_MMCR1
{
  u32 Hex;
  struct
  {
    u32 : 22;
    u32 PMC4SELECT : 5;
    u32 PMC3SELECT : 5;
  };
};

// Write Pipe Address Register
union UReg_WPAR
{
  struct
  {
    u32 BNE : 1;
    u32 : 4;
    u32 GB_ADDR : 27;
  };
  u32 Hex;

  UReg_WPAR(u32 _hex) { Hex = _hex; }
  UReg_WPAR() { Hex = 0; }
};

// Direct Memory Access Upper register
union UReg_DMAU
{
  struct
  {
    u32 DMA_LEN_U : 5;
    u32 MEM_ADDR : 27;
  };
  u32 Hex;

  UReg_DMAU(u32 _hex) { Hex = _hex; }
  UReg_DMAU() { Hex = 0; }
};

// Direct Memory Access Lower (DMAL) register
union UReg_DMAL
{
  struct
  {
    u32 DMA_F : 1;
    u32 DMA_T : 1;
    u32 DMA_LEN_L : 2;
    u32 DMA_LD : 1;
    u32 LC_ADDR : 27;
  };
  u32 Hex;

  UReg_DMAL(u32 _hex) { Hex = _hex; }
  UReg_DMAL() { Hex = 0; }
};

union UReg_BAT_Up
{
  struct
  {
    u32 VP : 1;
    u32 VS : 1;
    u32 BL : 11;  // Block length (aka block size mask)
    u32 : 4;
    u32 BEPI : 15;
  };
  u32 Hex;

  UReg_BAT_Up(u32 _hex) { Hex = _hex; }
  UReg_BAT_Up() { Hex = 0; }
};

union UReg_BAT_Lo
{
  struct
  {
    u32 PP : 2;
    u32 : 1;
    u32 WIMG : 4;
    u32 : 10;
    u32 BRPN : 15;  // Physical Block Number
  };
  u32 Hex;

  UReg_BAT_Lo(u32 _hex) { Hex = _hex; }
  UReg_BAT_Lo() { Hex = 0; }
};

union UReg_PTE
{
  struct
  {
    u64 API : 6;
    u64 H : 1;
    u64 VSID : 24;
    u64 V : 1;
    u64 PP : 2;
    u64 : 1;
    u64 WIMG : 4;
    u64 C : 1;
    u64 R : 1;
    u64 : 3;
    u64 RPN : 20;
  };

  u64 Hex;
  u32 Hex32[2];
};

//
// --- Gekko Types and Defs ---
//

// branches
enum
{
  BO_BRANCH_IF_CTR_0 = 2,        // 3
  BO_DONT_DECREMENT_FLAG = 4,    // 2
  BO_BRANCH_IF_TRUE = 8,         // 1
  BO_DONT_CHECK_CONDITION = 16,  // 0
};

// Special purpose register indices
enum
{
  SPR_XER = 1,
  SPR_LR = 8,
  SPR_CTR = 9,
  SPR_DSISR = 18,
  SPR_DAR = 19,
  SPR_DEC = 22,
  SPR_SDR = 25,
  SPR_SRR0 = 26,
  SPR_SRR1 = 27,
  SPR_TL = 268,
  SPR_TU = 269,
  SPR_TL_W = 284,
  SPR_TU_W = 285,
  SPR_PVR = 287,
  SPR_SPRG0 = 272,
  SPR_SPRG1 = 273,
  SPR_SPRG2 = 274,
  SPR_SPRG3 = 275,
  SPR_EAR = 282,
  SPR_IBAT0U = 528,
  SPR_IBAT0L = 529,
  SPR_IBAT1U = 530,
  SPR_IBAT1L = 531,
  SPR_IBAT2U = 532,
  SPR_IBAT2L = 533,
  SPR_IBAT3U = 534,
  SPR_IBAT3L = 535,
  SPR_DBAT0U = 536,
  SPR_DBAT0L = 537,
  SPR_DBAT1U = 538,
  SPR_DBAT1L = 539,
  SPR_DBAT2U = 540,
  SPR_DBAT2L = 541,
  SPR_DBAT3U = 542,
  SPR_DBAT3L = 543,
  SPR_IBAT4U = 560,
  SPR_IBAT4L = 561,
  SPR_IBAT5U = 562,
  SPR_IBAT5L = 563,
  SPR_IBAT6U = 564,
  SPR_IBAT6L = 565,
  SPR_IBAT7U = 566,
  SPR_IBAT7L = 567,
  SPR_DBAT4U = 568,
  SPR_DBAT4L = 569,
  SPR_DBAT5U = 570,
  SPR_DBAT5L = 571,
  SPR_DBAT6U = 572,
  SPR_DBAT6L = 573,
  SPR_DBAT7U = 574,
  SPR_DBAT7L = 575,
  SPR_GQR0 = 912,
  SPR_HID0 = 1008,
  SPR_HID1 = 1009,
  SPR_HID2 = 920,
  SPR_HID4 = 1011,
  SPR_WPAR = 921,
  SPR_DMAU = 922,
  SPR_DMAL = 923,
  SPR_ECID_U = 924,
  SPR_ECID_M = 925,
  SPR_ECID_L = 926,
  SPR_L2CR = 1017,

  SPR_UMMCR0 = 936,
  SPR_MMCR0 = 952,
  SPR_PMC1 = 953,
  SPR_PMC2 = 954,

  SPR_UMMCR1 = 940,
  SPR_MMCR1 = 956,
  SPR_PMC3 = 957,
  SPR_PMC4 = 958,
};

// Exceptions
enum
{
  EXCEPTION_DECREMENTER = 0x00000001,
  EXCEPTION_SYSCALL = 0x00000002,
  EXCEPTION_EXTERNAL_INT = 0x00000004,
  EXCEPTION_DSI = 0x00000008,
  EXCEPTION_ISI = 0x00000010,
  EXCEPTION_ALIGNMENT = 0x00000020,
  EXCEPTION_FPU_UNAVAILABLE = 0x00000040,
  EXCEPTION_PROGRAM = 0x00000080,
  EXCEPTION_PERFORMANCE_MONITOR = 0x00000100,

  EXCEPTION_FAKE_MEMCHECK_HIT = 0x00000200,
};

#if defined(_MSC_VER) && _MSC_VER <= 1800
inline s32 SignExt16(s16 x)
{
  return (s32)(s16)x;
}
inline s32 SignExt26(u32 x)
{
  return x & 0x2000000 ? (s32)(x | 0xFC000000) : (s32)(x);
}
#else
constexpr s32 SignExt16(s16 x)
{
  return (s32)x;
}
constexpr s32 SignExt26(u32 x)
{
  return x & 0x2000000 ? (s32)(x | 0xFC000000) : (s32)(x);
}
#endif