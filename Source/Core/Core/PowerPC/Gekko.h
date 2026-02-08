// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Gekko related unions, structs, ...

#pragma once

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"

// --- Gekko Instruction ---

union UGeckoInstruction
{
  u32 hex = 0;

  UGeckoInstruction() = default;
  UGeckoInstruction(u32 hex_) : hex(hex_) {}
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

// Precondition: inst is a bx, bcx, bclrx, or bcctrx instruction.
constexpr bool BranchIsConditional(UGeckoInstruction inst)
{
  if (inst.OPCD == 18)  // bx
    return false;
  return (inst.BO & 0b10100) != 0b10100;  // 1z1zz - Branch always
}

// Used in implementations of rlwimi, rlwinm, and rlwnm
inline u32 MakeRotationMask(u32 mb, u32 me)
{
  // first make 001111111111111 part
  const u32 begin = 0xFFFFFFFF >> mb;
  // then make 000000000001111 part, which is used to flip the bits of the first one
  const u32 end = 0x7FFFFFFF >> me;
  // do the bitflip
  const u32 mask = begin ^ end;

  // and invert if backwards
  if (me < mb)
    return ~mask;

  return mask;
}

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

  u32 Hex = 0;

  UGQR() = default;
  explicit UGQR(u32 hex_) : Hex{hex_} {}
};

#define XER_CA_SHIFT 29
#define XER_OV_SHIFT 30
#define XER_SO_SHIFT 31
#define XER_OV_MASK 1
#define XER_SO_MASK 2
// XER
union UReg_XER
{
  BitField<0, 7, u32> BYTE_COUNT;
  BitField<7, 1, u32> reserved_1;
  BitField<8, 8, u32> BYTE_CMP;
  BitField<16, 13, u32> reserved_2;
  BitField<29, 1, u32> CA;
  BitField<30, 1, u32> OV;
  BitField<31, 1, u32> SO;

  u32 Hex = 0;

  UReg_XER() = default;
  explicit UReg_XER(u32 hex_) : Hex{hex_} {}
};

// Machine State Register
union UReg_MSR
{
  BitField<0, 1, u32> LE;
  BitField<1, 1, u32> RI;
  BitField<2, 1, u32> PM;
  BitField<3, 1, u32> reserved_1;
  BitField<4, 1, u32> DR;
  BitField<5, 1, u32> IR;
  BitField<6, 1, u32> IP;
  BitField<7, 1, u32> reserved_2;
  BitField<8, 1, u32> FE1;
  BitField<9, 1, u32> BE;
  BitField<10, 1, u32> SE;
  BitField<11, 1, u32> FE0;
  BitField<12, 1, u32> MCHECK;
  BitField<13, 1, u32> FP;
  BitField<14, 1, u32> PR;
  BitField<15, 1, u32> EE;
  BitField<16, 1, u32> ILE;
  BitField<17, 1, u32> reserved_3;
  BitField<18, 1, u32> POW;
  BitField<19, 13, u32> reserved_4;

  u32 Hex = 0;

  UReg_MSR() = default;
  explicit UReg_MSR(u32 hex_) : Hex{hex_} {}
};

#define FPRF_SHIFT 12
#define FPRF_WIDTH 5
#define FPRF_MASK (0x1F << FPRF_SHIFT)
#define FPCC_MASK (0xF << FPRF_SHIFT)

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
  FPSCR_OE = 1U << (31 - 25),
  FPSCR_UE = 1U << (31 - 26),
  FPSCR_ZE = 1U << (31 - 27),
  FPSCR_XE = 1U << (31 - 28),

  FPSCR_VX_ANY = FPSCR_VXSNAN | FPSCR_VXISI | FPSCR_VXIDI | FPSCR_VXZDZ | FPSCR_VXIMZ | FPSCR_VXVC |
                 FPSCR_VXSOFT | FPSCR_VXSQRT | FPSCR_VXCVI,

  FPSCR_ANY_X = FPSCR_OX | FPSCR_UX | FPSCR_ZX | FPSCR_XX | FPSCR_VX_ANY,

  FPSCR_ANY_E = FPSCR_VE | FPSCR_OE | FPSCR_UE | FPSCR_ZE | FPSCR_XE,
};

// Floating Point Status and Control Register
union UReg_FPSCR
{
  // Rounding mode (towards: nearest, zero, +inf, -inf)
  BitField<0, 2, Common::FPU::RoundMode> RN;
  // Non-IEEE mode enable (aka flush-to-zero)
  BitField<2, 1, u32> NI;
  // Inexact exception enable
  BitField<3, 1, u32> XE;
  // IEEE division by zero exception enable
  BitField<4, 1, u32> ZE;
  // IEEE underflow exception enable
  BitField<5, 1, u32> UE;
  // IEEE overflow exception enable
  BitField<6, 1, u32> OE;
  // Invalid operation exception enable
  BitField<7, 1, u32> VE;
  // Invalid operation exception for integer conversion (sticky)
  BitField<8, 1, u32> VXCVI;
  // Invalid operation exception for square root (sticky)
  BitField<9, 1, u32> VXSQRT;
  // Invalid operation exception for software request (sticky)
  BitField<10, 1, u32> VXSOFT;
  // reserved
  BitField<11, 1, u32> reserved;
  // Floating point result flags (includes FPCC) (not sticky)
  // from more to less significand: class, <, >, =, ?
  BitField<12, 5, u32> FPRF;
  // Fraction inexact (not sticky)
  BitField<17, 1, u32> FI;
  // Fraction rounded (not sticky)
  BitField<18, 1, u32> FR;
  // Invalid operation exception for invalid comparison (sticky)
  BitField<19, 1, u32> VXVC;
  // Invalid operation exception for inf * 0 (sticky)
  BitField<20, 1, u32> VXIMZ;
  // Invalid operation exception for 0 / 0 (sticky)
  BitField<21, 1, u32> VXZDZ;
  // Invalid operation exception for inf / inf (sticky)
  BitField<22, 1, u32> VXIDI;
  // Invalid operation exception for inf - inf (sticky)
  BitField<23, 1, u32> VXISI;
  // Invalid operation exception for SNaN (sticky)
  BitField<24, 1, u32> VXSNAN;
  // Inexact exception (sticky)
  BitField<25, 1, u32> XX;
  // Division by zero exception (sticky)
  BitField<26, 1, u32> ZX;
  // Underflow exception (sticky)
  BitField<27, 1, u32> UX;
  // Overflow exception (sticky)
  BitField<28, 1, u32> OX;
  // Invalid operation exception summary (not sticky)
  BitField<29, 1, u32> VX;
  // Enabled exception summary (not sticky)
  BitField<30, 1, u32> FEX;
  // Exception summary (sticky)
  BitField<31, 1, u32> FX;

  u32 Hex = 0;

  // The FPSCR's 20th bit (11th from a little endian perspective)
  // is defined as reserved and set to zero. Attempts to modify it
  // are ignored by hardware, so we do the same.
  static constexpr u32 mask = 0xFFFFF7FF;

  UReg_FPSCR() = default;
  explicit UReg_FPSCR(u32 hex_) : Hex{hex_ & mask} {}

  UReg_FPSCR& operator=(u32 value)
  {
    Hex = value & mask;
    return *this;
  }

  UReg_FPSCR& operator|=(u32 value)
  {
    Hex |= value & mask;
    return *this;
  }

  UReg_FPSCR& operator&=(u32 value)
  {
    Hex &= value;
    return *this;
  }

  UReg_FPSCR& operator^=(u32 value)
  {
    Hex ^= value & mask;
    return *this;
  }

  void ClearFIFR()
  {
    FI = 0;
    FR = 0;
  }
};

// Hardware Implementation-Dependent Register 0
union UReg_HID0
{
  BitField<0, 1, u32> NOOPTI;
  BitField<1, 1, u32> reserved_1;
  BitField<2, 1, u32> BHT;
  BitField<3, 1, u32> ABE;
  BitField<4, 1, u32> reserved_2;
  BitField<5, 1, u32> BTIC;
  BitField<6, 1, u32> DCFA;
  BitField<7, 1, u32> SGE;
  BitField<8, 1, u32> IFEM;
  BitField<9, 1, u32> SPD;
  BitField<10, 1, u32> DCFI;
  BitField<11, 1, u32> ICFI;
  BitField<12, 1, u32> DLOCK;
  BitField<13, 1, u32> ILOCK;
  BitField<14, 1, u32> DCE;
  BitField<15, 1, u32> ICE;
  BitField<16, 1, u32> NHR;
  BitField<17, 3, u32> reserved_3;
  BitField<20, 1, u32> DPM;
  BitField<21, 1, u32> SLEEP;
  BitField<22, 1, u32> NAP;
  BitField<23, 1, u32> DOZE;
  BitField<24, 1, u32> PAR;
  BitField<25, 1, u32> ECLK;
  BitField<26, 1, u32> reserved_4;
  BitField<27, 1, u32> BCLK;
  BitField<28, 1, u32> EBD;
  BitField<29, 1, u32> EBA;
  BitField<30, 1, u32> DBP;
  BitField<31, 1, u32> EMCP;

  u32 Hex = 0;
};

// Hardware Implementation-Dependent Register 2
union UReg_HID2
{
  BitField<0, 16, u32> reserved;
  BitField<16, 1, u32> DQOEE;
  BitField<17, 1, u32> DCMEE;
  BitField<18, 1, u32> DNCEE;
  BitField<19, 1, u32> DCHEE;
  BitField<20, 1, u32> DQOERR;
  BitField<21, 1, u32> DCMERR;
  BitField<22, 1, u32> DNCERR;
  BitField<23, 1, u32> DCHERR;
  BitField<24, 4, u32> DMAQL;
  BitField<28, 1, u32> LCE;
  BitField<29, 1, u32> PSE;
  BitField<30, 1, u32> WPE;
  BitField<31, 1, u32> LSQE;

  u32 Hex = 0;

  UReg_HID2() = default;
  explicit UReg_HID2(u32 hex_) : Hex{hex_} {}
};

// Hardware Implementation-Dependent Register 4
union UReg_HID4
{
  BitField<0, 20, u32> reserved_1;
  BitField<20, 1, u32> L2CFI;
  BitField<21, 1, u32> L2MUM;
  BitField<22, 1, u32> DBP;
  BitField<23, 1, u32> LPE;
  BitField<24, 1, u32> ST0;
  BitField<25, 1, u32> SBE;
  BitField<26, 1, u32> reserved_2;
  BitField<27, 2, u32> BPD;
  BitField<29, 2, u32> L2FM;
  BitField<31, 1, u32> reserved_3;

  u32 Hex = 0;

  UReg_HID4() = default;
  explicit UReg_HID4(u32 hex_) : Hex{hex_} {}
};

// SDR1 - Page Table format
union UReg_SDR1
{
  BitField<0, 9, u32> htabmask;
  BitField<9, 7, u32> reserved;
  BitField<16, 16, u32> htaborg;

  u32 Hex = 0;

  UReg_SDR1() = default;
  explicit UReg_SDR1(u32 hex_) : Hex{hex_} {}
};

// MMCR0 - Monitor Mode Control Register 0 format
union UReg_MMCR0
{
  BitField<0, 6, u32> PMC2SELECT;
  BitField<6, 7, u32> PMC1SELECT;
  BitField<13, 1, u32> PMCTRIGGER;
  BitField<14, 1, u32> PMCINTCONTROL;
  BitField<15, 1, u32> PMC1INTCONTROL;
  BitField<16, 6, u32> THRESHOLD;
  BitField<22, 1, u32> INTONBITTRANS;
  BitField<23, 2, u32> RTCSELECT;
  BitField<25, 1, u32> DISCOUNT;
  BitField<26, 1, u32> ENINT;
  BitField<27, 1, u32> DMR;
  BitField<28, 1, u32> DMS;
  BitField<29, 1, u32> DU;
  BitField<30, 1, u32> DP;
  BitField<31, 1, u32> DIS;

  u32 Hex = 0;
};

// MMCR1 - Monitor Mode Control Register 1 format
union UReg_MMCR1
{
  BitField<0, 22, u32> reserved;
  BitField<22, 5, u32> PMC4SELECT;
  BitField<27, 5, u32> PMC3SELECT;

  u32 Hex = 0;
};

// Write Pipe Address Register
union UReg_WPAR
{
  BitField<0, 1, u32> BNE;
  BitField<1, 4, u32> reserved;
  BitField<5, 27, u32> GB_ADDR;

  u32 Hex = 0;

  UReg_WPAR() = default;
  explicit UReg_WPAR(u32 hex_) : Hex{hex_} {}
};

// Direct Memory Access Upper register
union UReg_DMAU
{
  BitField<0, 5, u32> DMA_LEN_U;
  BitField<5, 27, u32> MEM_ADDR;

  u32 Hex = 0;

  UReg_DMAU() = default;
  explicit UReg_DMAU(u32 hex_) : Hex{hex_} {}
};

// Direct Memory Access Lower (DMAL) register
union UReg_DMAL
{
  BitField<0, 1, u32> DMA_F;
  BitField<1, 1, u32> DMA_T;
  BitField<2, 2, u32> DMA_LEN_L;
  BitField<4, 1, u32> DMA_LD;
  BitField<5, 27, u32> LC_ADDR;

  u32 Hex = 0;

  UReg_DMAL() = default;
  explicit UReg_DMAL(u32 hex_) : Hex{hex_} {}
};

union UReg_BAT_Up
{
  BitField<0, 1, u32> VP;
  BitField<1, 1, u32> VS;
  BitField<2, 11, u32> BL;  // Block length (aka block size mask)
  BitField<13, 4, u32> reserved;
  BitField<17, 15, u32> BEPI;

  u32 Hex = 0;

  UReg_BAT_Up() = default;
  explicit UReg_BAT_Up(u32 hex_) : Hex{hex_} {}
};

union UReg_BAT_Lo
{
  BitField<0, 2, u32> PP;
  BitField<2, 1, u32> reserved_1;
  BitField<3, 4, u32> WIMG;
  BitField<7, 10, u32> reserved_2;
  BitField<17, 15, u32> BRPN;  // Physical Block Number

  u32 Hex = 0;

  UReg_BAT_Lo() = default;
  explicit UReg_BAT_Lo(u32 hex_) : Hex{hex_} {}
};

// Segment register
union UReg_SR
{
  BitField<0, 24, u32> VSID;      // Virtual segment ID
  BitField<24, 4, u32> reserved;  // Reserved
  BitField<28, 1, u32> N;         // No-execute protection
  BitField<29, 1, u32> Kp;        // User-state protection
  BitField<30, 1, u32> Ks;        // Supervisor-state protection
  BitField<31, 1, u32> T;         // Segment register format selector

  // These override other fields if T = 1

  BitField<0, 20, u32> CNTLR_SPEC;  // Device-specific data for I/O controller
  BitField<20, 9, u32> BUID;        // Bus unit ID

  u32 Hex = 0;

  UReg_SR() = default;
  explicit UReg_SR(u32 hex_) : Hex{hex_} {}
};

union UReg_THRM12
{
  BitField<0, 1, u32> V;    // Valid
  BitField<1, 1, u32> TIE;  // Thermal Interrupt Enable
  BitField<2, 1, u32> TID;  // Thermal Interrupt Direction
  BitField<3, 20, u32> reserved;
  BitField<23, 7, u32> THRESHOLD;  // Temperature Threshold, 0-127°C
  BitField<30, 1, u32> TIV;        // Thermal Interrupt Valid
  BitField<31, 1, u32> TIN;        // Thermal Interrupt

  u32 Hex = 0;

  UReg_THRM12() = default;
  explicit UReg_THRM12(u32 hex_) : Hex{hex_} {}
};

union UReg_THRM3
{
  BitField<0, 1, u32> E;      // Enable
  BitField<1, 13, u32> SITV;  // Sample Interval Timer Value
  BitField<14, 18, u32> reserved;

  u32 Hex = 0;

  UReg_THRM3() = default;
  explicit UReg_THRM3(u32 hex_) : Hex{hex_} {}
};

union UPTE_Lo
{
  BitField<0, 6, u32> API;
  BitField<6, 1, u32> H;
  BitField<7, 24, u32> VSID;
  BitField<31, 1, u32> V;

  u32 Hex = 0;

  UPTE_Lo() = default;
  explicit UPTE_Lo(u32 hex_) : Hex{hex_} {}
};

union UPTE_Hi
{
  BitField<0, 2, u32> PP;
  BitField<2, 1, u32> reserved_1;
  BitField<3, 4, u32> WIMG;
  BitField<7, 1, u32> C;
  BitField<8, 1, u32> R;
  BitField<9, 3, u32> reserved_2;
  BitField<12, 20, u32> RPN;

  u32 Hex = 0;

  UPTE_Hi() = default;
  explicit UPTE_Hi(u32 hex_) : Hex{hex_} {}
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
  SPR_ASR = 280,
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
  SPR_IABR = 1010,
  SPR_DABR = 1013,
  SPR_WPAR = 921,
  SPR_DMAU = 922,
  SPR_DMAL = 923,
  SPR_ECID_U = 924,
  SPR_ECID_M = 925,
  SPR_ECID_L = 926,
  SPR_UPMC1 = 937,
  SPR_UPMC2 = 938,
  SPR_UPMC3 = 941,
  SPR_UPMC4 = 942,
  SPR_USIA = 939,
  SPR_SIA = 955,
  SPR_L2CR = 1017,
  SPR_ICTC = 1019,

  SPR_UMMCR0 = 936,
  SPR_MMCR0 = 952,
  SPR_PMC1 = 953,
  SPR_PMC2 = 954,

  SPR_UMMCR1 = 940,
  SPR_MMCR1 = 956,
  SPR_PMC3 = 957,
  SPR_PMC4 = 958,

  SPR_THRM1 = 1020,
  SPR_THRM2 = 1021,
  SPR_THRM3 = 1022,
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

enum CPUEmuFeatureFlags : u32
{
  FEATURE_FLAG_MSR_DR = 1 << 0,
  FEATURE_FLAG_MSR_IR = 1 << 1,
  FEATURE_FLAG_PERFMON = 1 << 2,
  FEATURE_FLAG_END_OF_ENUMERATION,
};

constexpr s32 SignExt16(s16 x)
{
  return (s32)x;
}
constexpr s32 SignExt26(u32 x)
{
  return x & 0x2000000 ? (s32)(x | 0xFC000000) : (s32)(x);
}
