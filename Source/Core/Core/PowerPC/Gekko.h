// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Gekko related unions, structs, ...

#pragma once

#include "Common/BitField.h"
#include "Common/BitField2.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"

// --- Gekko Instruction ---

union UGeckoInstruction
{
  u32 hex = 0;

  UGeckoInstruction() = default;
  UGeckoInstruction(u32 hex_) : hex(hex_) {}

  // Opcode
  BitField<26, 6, u32> OPCD;     // Primary opcode
  BitField<1, 5, u32> SUBOP5;    // A-Form Extended Opcode
  BitField<1, 10, u32> SUBOP10;  // X, XL, XFX, XFL-Form Extended Opcode
  BitField<1, 6, u32> SUBOP6;    // Variation of X-Form Extended Opcode (paired singles indexed)

  // Branch
  BitField<0, 1, bool, u32> LK;  // Link bit
                                 // 1, if branch instructions should put the address of the next
                                 // instruction into the link register
  BitField<1, 1, bool, u32> AA;  // Absolute address bit
                                 // 1, if the immediate field represents an absolute address
  BitField<2, 24, s32> LI;  // 24-bit signed immediate, right-extended by 0b00 (branch displacement)
  BitField<2, 14, s32> BD;  // 14-bit signed immediate, right-extended by 0b00 (branch displacement)
  BitField<16, 5, u32> BI;  // Branch condition
  BitField<21, 5, u32> BO;  // Conditional branch control

  // General-Purpose Register
  BitField<11, 5, u32> RB;  // Source GPR
  BitField<16, 5, u32> RA;  // Source GPR
  BitField<21, 5, u32> RS;  // Source GPR
  BitField<21, 5, u32> RD;  // Destination GPR

  // Floating-Point Register
  BitField<6, 5, u32> FC;   // Source FPR
  BitField<11, 5, u32> FB;  // Source FPR
  BitField<16, 5, u32> FA;  // Source FPR
  BitField<21, 5, u32> FS;  // Source FPR
  BitField<21, 5, u32> FD;  // Destination FPR

  // Compare Register Bit (crxor)
  BitField<11, 5, u32> CRBB;  // Source bit in the CR
  BitField<16, 5, u32> CRBA;  // Source bit in the CR
  BitField<21, 5, u32> CRBD;  // Destination bit in the CR

  // Compare Register Field
  BitField<18, 3, u32> CRFS;  // Source field in the CR or FPSCR
  BitField<23, 3, u32> CRFD;  // Destination field in CR or FPSCR

  // Other Register Types
  BitField<11, 10, u32> SPR;  // Special-purpose register
  BitField<11, 5, u32> SPRU;  // Upper special-purpose register
  BitField<16, 5, u32> SPRL;  // Lower special-purpose register
  BitField<11, 10, u32> TBR;  // Time base register (mftb)
  BitField<11, 5, u32> TBRU;  // Upper time base register
  BitField<16, 5, u32> TBRL;  // Lower time base register
  BitField<16, 4, u32> SR;    // Segment register

  // Immediate
  u16 UIMM;                      // 16-bit unsigned immediate
  s16 SIMM_16;                   // 16-bit signed immediate
  BitField<0, 12, s32> SIMM_12;  // 12-bit signed immediate (Paired-Singles Load/Store)
  BitField<11, 5, u32> NB;       // Number of bytes to use in lswi/stswi (0 means 32 bytes)

  // Shift / Rotate (rlwinmx)
  BitField<1, 5, u32> ME;   // Mask end
  BitField<6, 5, u32> MB;   // Mask begin
  BitField<11, 5, u32> SH;  // Shift amount

  // Paired Single Quantized Load/Store
  BitField<12, 3, u32> I;         // Graphics quantization register to use
  BitField<15, 1, bool, u32> W;   // 0: paired single, 1: scalar
  BitField<7, 3, u32> Ix;         // Graphics quantization register to use (indexed)
  BitField<10, 1, bool, u32> Wx;  // 0: paired single, 1: scalar (indexed)

  // Other
  BitField<0, 1, bool, u32> Rc;   // Record bit
                                  // 1, if the condition register should be updated by this
                                  // instruction
  BitField<10, 1, bool, u32> OE;  // Overflow enable
  BitField<12, 8, u32> CRM;       // Field mask, identifies the CR fields to be updated by mtcrf
  BitField<17, 8, u32> FM;        // Field mask, identifies the FPSCR fields to be updated by mtfsf
  BitField<21, 5, u32> TO;        // Conditions on which to trap
  BitField<21, 1, bool, u32> L;   // Use low 32 bits for comparison (invalid for 32-bit CPUs)
};

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
struct UGQR : BitField2<u32>
{
  FIELD(EQuantizeType, 0, 3, st_type)
  FIELD(u32, 8, 6, st_scale)
  FIELD(EQuantizeType, 16, 3, ld_type)
  FIELD(u32, 24, 6, ld_scale)

  UGQR() = default;
  constexpr UGQR(u32 val) : BitField2{val} {}
};

#define XER_CA_SHIFT 29
#define XER_OV_SHIFT 30
#define XER_SO_SHIFT 31
#define XER_OV_MASK 1
#define XER_SO_MASK 2
// XER
struct UReg_XER : BitField2<u32>
{
  FIELD(u32, 0, 7, BYTE_COUNT)
  FIELD(u32, 7, 1, reserved_1)
  FIELD(u32, 8, 8, BYTE_CMP)
  FIELD(u32, 16, 13, reserved_2)
  FIELD(u32, 29, 1, CA)
  FIELD(u32, 30, 1, OV)
  FIELD(u32, 31, 1, SO)

  UReg_XER() = default;
  constexpr UReg_XER(u32 val) : BitField2{val} {}
};

// Machine State Register
struct UReg_MSR : BitField2<u32>
{
  FIELD(bool, 0, 1, LE);
  FIELD(bool, 1, 1, RI);
  FIELD(bool, 2, 1, PM);
  FIELD(u32, 3, 1, reserved_1);
  FIELD(bool, 4, 1, DR);
  FIELD(bool, 5, 1, IR);
  FIELD(bool, 6, 1, IP);
  FIELD(u32, 7, 1, reserved_2);
  FIELD(bool, 8, 1, FE1);
  FIELD(bool, 9, 1, BE);
  FIELD(bool, 10, 1, SE);
  FIELD(bool, 11, 1, FE0);
  FIELD(bool, 12, 1, MCHECK);
  FIELD(bool, 13, 1, FP);
  FIELD(bool, 14, 1, PR);
  FIELD(bool, 15, 1, EE);
  FIELD(bool, 16, 1, ILE);
  FIELD(u32, 17, 1, reserved_3);
  FIELD(bool, 18, 1, POW);
  FIELD(u32, 19, 13, reserved_4);

  UReg_MSR() = default;
  constexpr UReg_MSR(u32 val) : BitField2{val} {}
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
struct UReg_FPSCR : BitField2<u32>
{
  // Rounding mode (towards: nearest, zero, +inf, -inf)
  FIELD(FPURoundMode::RoundMode, 0, 2, RN);
  // Non-IEEE mode enable (aka flush-to-zero)
  FIELD(bool, 2, 1, NI);
  // Inexact exception enable
  FIELD(bool, 3, 1, XE);
  // IEEE division by zero exception enable
  FIELD(bool, 4, 1, ZE);
  // IEEE underflow exception enable
  FIELD(bool, 5, 1, UE);
  // IEEE overflow exception enable
  FIELD(bool, 6, 1, OE);
  // Invalid operation exception enable
  FIELD(bool, 7, 1, VE);
  // Invalid operation exception for integer conversion (sticky)
  FIELD(bool, 8, 1, VXCVI);
  // Invalid operation exception for square root (sticky)
  FIELD(bool, 9, 1, VXSQRT);
  // Invalid operation exception for software request (sticky)
  FIELD(bool, 10, 1, VXSOFT);
  // reserved
  FIELD(bool, 11, 1, reserved);
  // Floating point result flags (includes FPCC) (not sticky)
  // from more to less significand: class, <, >, =, ?
  FIELD(bool, 12, 5, FPRF);
  // Fraction inexact (not sticky)
  FIELD(bool, 17, 1, FI);
  // Fraction rounded (not sticky)
  FIELD(bool, 18, 1, FR);
  // Invalid operation exception for invalid comparison (sticky)
  FIELD(bool, 19, 1, VXVC);
  // Invalid operation exception for inf * 0 (sticky)
  FIELD(bool, 20, 1, VXIMZ);
  // Invalid operation exception for 0 / 0 (sticky)
  FIELD(bool, 21, 1, VXZDZ);
  // Invalid operation exception for inf / inf (sticky)
  FIELD(bool, 22, 1, VXIDI);
  // Invalid operation exception for inf - inf (sticky)
  FIELD(bool, 23, 1, VXISI);
  // Invalid operation exception for SNaN (sticky)
  FIELD(bool, 24, 1, VXSNAN);
  // Inexact exception (sticky)
  FIELD(bool, 25, 1, XX);
  // Division by zero exception (sticky)
  FIELD(bool, 26, 1, ZX);
  // Underflow exception (sticky)
  FIELD(bool, 27, 1, UX);
  // Overflow exception (sticky)
  FIELD(bool, 28, 1, OX);
  // Invalid operation exception summary (not sticky)
  FIELD(bool, 29, 1, VX);
  // Enabled exception summary (not sticky)
  FIELD(bool, 30, 1, FEX);
  // Exception summary (sticky)
  FIELD(bool, 31, 1, FX);

  // The FPSCR's 20th bit (11th from a little endian perspective)
  // is defined as reserved and set to zero. Attempts to modify it
  // are ignored by hardware, so we do the same.
  static constexpr u32 mask = 0xFFFFF7FF;

  UReg_FPSCR() = default;
  constexpr UReg_FPSCR(u32 val) : BitField2{val & mask} {}

  UReg_FPSCR& operator=(u32 value)
  {
    BitField2::operator=(value& mask);
    return *this;
  }

  UReg_FPSCR& operator|=(u32 value)
  {
    BitField2::operator|=(value& mask);
    return *this;
  }

  UReg_FPSCR& operator&=(u32 value)
  {
    BitField2::operator&=(value& mask);
    return *this;
  }

  UReg_FPSCR& operator^=(u32 value)
  {
    BitField2::operator^=(value& mask);
    return *this;
  }

  void ClearFIFR()
  {
    FI() = 0;
    FR() = 0;
  }
};

// Hardware Implementation-Dependent Register 0
struct UReg_HID0 : BitField2<u32>
{
  FIELD(bool, 0, 1, NOOPTI);
  FIELD(bool, 1, 1, reserved_1);
  FIELD(bool, 2, 1, BHT);
  FIELD(bool, 3, 1, ABE);
  FIELD(bool, 4, 1, reserved_2);
  FIELD(bool, 5, 1, BTIC);
  FIELD(bool, 6, 1, DCFA);
  FIELD(bool, 7, 1, SGE);
  FIELD(bool, 8, 1, IFEM);
  FIELD(bool, 9, 1, SPD);
  FIELD(bool, 10, 1, DCFI);
  FIELD(bool, 11, 1, ICFI);
  FIELD(bool, 12, 1, DLOCK);
  FIELD(bool, 13, 1, ILOCK);
  FIELD(bool, 14, 1, DCE);
  FIELD(bool, 15, 1, ICE);
  FIELD(bool, 16, 1, NHR);
  FIELD(bool, 17, 3, reserved_3);
  FIELD(bool, 20, 1, DPM);
  FIELD(bool, 21, 1, SLEEP);
  FIELD(bool, 22, 1, NAP);
  FIELD(bool, 23, 1, DOZE);
  FIELD(bool, 24, 1, PAR);
  FIELD(bool, 25, 1, ECLK);
  FIELD(bool, 26, 1, reserved_4);
  FIELD(bool, 27, 1, BCLK);
  FIELD(bool, 28, 1, EBD);
  FIELD(bool, 29, 1, EBA);
  FIELD(bool, 30, 1, DBP);
  FIELD(bool, 31, 1, EMCP);

  UReg_HID0() = default;
  constexpr UReg_HID0(u32 val) : BitField2(val) {}
};

// Hardware Implementation-Dependent Register 2
struct UReg_HID2 : BitField2<u32>
{
  FIELD(u32, 0, 16, reserved);
  FIELD(bool, 16, 1, DQOEE);
  FIELD(bool, 17, 1, DCMEE);
  FIELD(bool, 18, 1, DNCEE);
  FIELD(bool, 19, 1, DCHEE);
  FIELD(bool, 20, 1, DQOERR);
  FIELD(bool, 21, 1, DCMERR);
  FIELD(bool, 22, 1, DNCERR);
  FIELD(bool, 23, 1, DCHERR);
  FIELD(bool, 24, 4, DMAQL);
  FIELD(bool, 28, 1, LCE);
  FIELD(bool, 29, 1, PSE);
  FIELD(bool, 30, 1, WPE);
  FIELD(bool, 31, 1, LSQE);

  UReg_HID2() = default;
  constexpr UReg_HID2(u32 val) : BitField2(val) {}
};

// Hardware Implementation-Dependent Register 4
struct UReg_HID4 : BitField2<u32>
{
  FIELD(u32, 0, 20, reserved_1);
  FIELD(bool, 20, 1, L2CFI);
  FIELD(bool, 21, 1, L2MUM);
  FIELD(bool, 22, 1, DBP);
  FIELD(bool, 23, 1, LPE);
  FIELD(bool, 24, 1, ST0);
  FIELD(bool, 25, 1, SBE);
  FIELD(u32, 26, 1, reserved_2);
  FIELD(bool, 27, 2, BPD);
  FIELD(bool, 29, 2, L2FM);
  FIELD(u32, 31, 1, reserved_3);

  UReg_HID4() = default;
  constexpr UReg_HID4(u32 val) : BitField2(val) {}
};

// SDR1 - Page Table format
struct UReg_SDR1 : BitField2<u32>
{
  FIELD(u32, 0, 9, htabmask);
  FIELD(u32, 9, 7, reserved);
  FIELD(u32, 16, 16, htaborg);

  UReg_SDR1() = default;
  constexpr UReg_SDR1(u32 val) : BitField2(val) {}
};

// MMCR0 - Monitor Mode Control Register 0 format
struct UReg_MMCR0 : BitField2<u32>
{
  FIELD(u32, 0, 6, PMC2SELECT);
  FIELD(u32, 6, 7, PMC1SELECT);
  FIELD(bool, 13, 1, PMCTRIGGER);
  FIELD(bool, 14, 1, PMCINTCONTROL);
  FIELD(bool, 15, 1, PMC1INTCONTROL);
  FIELD(u32, 16, 6, THRESHOLD);
  FIELD(bool, 22, 1, INTONBITTRANS);
  FIELD(u32, 23, 2, RTCSELECT);
  FIELD(bool, 25, 1, DISCOUNT);
  FIELD(bool, 26, 1, ENINT);
  FIELD(bool, 27, 1, DMR);
  FIELD(bool, 28, 1, DMS);
  FIELD(bool, 29, 1, DU);
  FIELD(bool, 30, 1, DP);
  FIELD(bool, 31, 1, DIS);

  UReg_MMCR0() = default;
  constexpr UReg_MMCR0(u32 val) : BitField2(val) {}
};

// MMCR1 - Monitor Mode Control Register 1 format
struct UReg_MMCR1 : BitField2<u32>
{
  FIELD(u32, 0, 22, reserved);
  FIELD(u32, 22, 5, PMC4SELECT);
  FIELD(u32, 27, 5, PMC3SELECT);

  UReg_MMCR1() = default;
  UReg_MMCR1(u32 val) : BitField2(val) {}
};

// Write Pipe Address Register
struct UReg_WPAR : BitField2<u32>
{
  FIELD(bool, 0, 1, BNE);
  FIELD(u32, 1, 4, reserved);
  FIELD(u32, 5, 27, GB_ADDR);

  UReg_WPAR() = default;
  constexpr UReg_WPAR(u32 val) : BitField2(val) {}
};

// Direct Memory Access Upper register
struct UReg_DMAU : BitField2<u32>
{
  FIELD(u32, 0, 5, DMA_LEN_U);
  FIELD(u32, 5, 27, MEM_ADDR);

  UReg_DMAU() = default;
  constexpr UReg_DMAU(u32 val) : BitField2(val) {}
};

// Direct Memory Access Lower (DMAL) register
struct UReg_DMAL : BitField2<u32>
{
  FIELD(bool, 0, 1, DMA_F);
  FIELD(bool, 1, 1, DMA_T);
  FIELD(u32, 2, 2, DMA_LEN_L);
  FIELD(u32, 4, 1, DMA_LD);
  FIELD(u32, 5, 27, LC_ADDR);

  UReg_DMAL() = default;
  constexpr UReg_DMAL(u32 val) : BitField2(val) {}
};

struct UReg_BAT_Up : BitField2<u32>
{
  FIELD(bool, 0, 1, VP);
  FIELD(bool, 1, 1, VS);
  FIELD(u32, 2, 11, BL);  // Block length (aka block size mask)
  FIELD(u32, 13, 4, reserved);
  FIELD(u32, 17, 15, BEPI);

  UReg_BAT_Up() = default;
  constexpr UReg_BAT_Up(u32 val) : BitField2(val) {}
};

struct UReg_BAT_Lo : BitField2<u32>
{
  FIELD(u32, 0, 2, PP);
  FIELD(u32, 2, 1, reserved_1);
  FIELD(u32, 3, 4, WIMG);
  FIELD(u32, 7, 10, reserved_2);
  FIELD(u32, 17, 15, BRPN);  // Physical Block Number

  UReg_BAT_Lo() = default;
  constexpr UReg_BAT_Lo(u32 val) : BitField2(val) {}
};

// Segment register
struct UReg_SR : BitField2<u32>
{
  FIELD(u32, 0, 24, VSID);      // Virtual segment ID
  FIELD(u32, 24, 4, reserved);  // Reserved
  FIELD(bool, 28, 1, N);        // No-execute protection
  FIELD(bool, 29, 1, Kp);       // User-state protection
  FIELD(bool, 30, 1, Ks);       // Supervisor-state protection
  FIELD(bool, 31, 1, T);        // Segment register format selector

  // These override other fields if T = 1

  FIELD(u32, 0, 20, CNTLR_SPEC);  // Device-specific data for I/O controller
  FIELD(u32, 20, 9, BUID);        // Bus unit ID

  UReg_SR() = default;
  constexpr UReg_SR(u32 val) : BitField2(val) {}
};

struct UReg_THRM12 : BitField2<u32>
{
  FIELD(bool, 0, 1, V);    // Valid
  FIELD(bool, 1, 1, TIE);  // Thermal Interrupt Enable
  FIELD(bool, 2, 1, TID);  // Thermal Interrupt Direction
  FIELD(u32, 3, 20, reserved);
  FIELD(u32, 23, 7, THRESHOLD);  // Temperature Threshold, 0-127°C
  FIELD(bool, 30, 1, TIV);       // Thermal Interrupt Valid
  FIELD(bool, 31, 1, TIN);       // Thermal Interrupt

  UReg_THRM12() = default;
  constexpr UReg_THRM12(u32 val) : BitField2(val) {}
};

struct UReg_THRM3 : BitField2<u32>
{
  FIELD(bool, 0, 1, E);     // Enable
  FIELD(u32, 1, 13, SITV);  // Sample Interval Timer Value
  FIELD(u32, 14, 18, reserved);

  UReg_THRM3() = default;
  constexpr UReg_THRM3(u32 val) : BitField2(val) {}
};

struct UPTE_Lo : BitField2<u32>
{
  FIELD(u32, 0, 6, API);
  FIELD(bool, 6, 1, H);
  FIELD(u32, 7, 24, VSID);
  FIELD(bool, 31, 1, V);

  UPTE_Lo() = default;
  constexpr UPTE_Lo(u32 val) : BitField2(val) {}
};

struct UPTE_Hi : BitField2<u32>
{
  FIELD(u32, 0, 2, PP);
  FIELD(u32, 2, 1, reserved_1);
  FIELD(u32, 3, 4, WIMG);
  FIELD(bool, 7, 1, C);
  FIELD(bool, 8, 1, R);
  FIELD(u32, 9, 3, reserved_2);
  FIELD(u32, 12, 20, RPN);

  UPTE_Hi() = default;
  constexpr UPTE_Hi(u32 val) : BitField2(val) {}
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

constexpr s32 SignExt16(s16 x)
{
  return (s32)x;
}
constexpr s32 SignExt26(u32 x)
{
  return x & 0x2000000 ? (s32)(x | 0xFC000000) : (s32)(x);
}
