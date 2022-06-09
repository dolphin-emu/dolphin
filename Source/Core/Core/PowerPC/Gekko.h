// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Gekko related unions, structs, ...

#pragma once

#include "Common/BitFieldView.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"

// --- Gekko Instruction ---

struct UGeckoInstruction
{
  u32 hex = 0;

  UGeckoInstruction() = default;
  UGeckoInstruction(u32 hex_) : hex(hex_) {}

  // Opcode
  BFVIEW_M(hex, u32, 26, 6, OPCD);     // Primary opcode
  BFVIEW_M(hex, u32, 1, 5, SUBOP5);    // A-Form Extended Opcode
  BFVIEW_M(hex, u32, 1, 10, SUBOP10);  // X, XL, XFX, XFL-Form Extended Opcode
  BFVIEW_M(hex, u32, 1, 6, SUBOP6);  // Variation of X-Form Extended Opcode (paired singles indexed)

  // Branch
  BFVIEW_M(hex, bool, 0, 1, LK);  // Link bit
                                  // 1, if branch instructions should put the address of the next
                                  // instruction into the link register
  BFVIEW_M(hex, bool, 1, 1, AA);  // Absolute address bit
                                  // 1, if the immediate field represents an absolute address
  BFVIEW_M(hex, s32, 2, 24, LI);  // 24-bit signed immediate, right-extended by 0b00
                                  // (branch displacement)
  BFVIEW_M(hex, s32, 2, 14, BD);  // 14-bit signed immediate, right-extended by 0b00
                                  // (branch displacement)
  BFVIEW_M(hex, u32, 16, 5, BI);  // Branch condition
  BFVIEW_M(hex, u32, 21, 5, BO);  // Conditional branch control

  // General-Purpose Register
  BFVIEW_M(hex, u32, 11, 5, RB);  // Source GPR
  BFVIEW_M(hex, u32, 16, 5, RA);  // Source GPR
  BFVIEW_M(hex, u32, 21, 5, RS);  // Source GPR
  BFVIEW_M(hex, u32, 21, 5, RD);  // Destination GPR

  // Floating-Point Register
  BFVIEW_M(hex, u32, 6, 5, FC);   // Source FPR
  BFVIEW_M(hex, u32, 11, 5, FB);  // Source FPR
  BFVIEW_M(hex, u32, 16, 5, FA);  // Source FPR
  BFVIEW_M(hex, u32, 21, 5, FS);  // Source FPR
  BFVIEW_M(hex, u32, 21, 5, FD);  // Destination FPR

  // Compare Register Bit (crxor)
  BFVIEW_M(hex, u32, 11, 5, CRBB);  // Source bit in the CR
  BFVIEW_M(hex, u32, 16, 5, CRBA);  // Source bit in the CR
  BFVIEW_M(hex, u32, 21, 5, CRBD);  // Destination bit in the CR

  // Compare Register Field
  BFVIEW_M(hex, u32, 18, 3, CRFS);  // Source field in the CR or FPSCR
  BFVIEW_M(hex, u32, 23, 3, CRFD);  // Destination field in CR or FPSCR

  // Other Register Types
  BFVIEW_M(hex, u32, 11, 10, SPR);  // Special-purpose register
  BFVIEW_M(hex, u32, 11, 5, SPRU);  // Upper special-purpose register
  BFVIEW_M(hex, u32, 16, 5, SPRL);  // Lower special-purpose register
  BFVIEW_M(hex, u32, 11, 10, TBR);  // Time base register (mftb)
  BFVIEW_M(hex, u32, 11, 5, TBRU);  // Upper time base register
  BFVIEW_M(hex, u32, 16, 5, TBRL);  // Lower time base register
  BFVIEW_M(hex, u32, 16, 4, SR);    // Segment register

  // Immediate
  BFVIEW_M(hex, u16, 0, 16, UIMM);     // 16-bit unsigned immediate
  BFVIEW_M(hex, s16, 0, 16, SIMM_16);  // 16-bit signed immediate
  BFVIEW_M(hex, s16, 0, 12, SIMM_12);  // 12-bit signed immediate (Paired-Singles Load/Store)
  BFVIEW_M(hex, u32, 11, 5, NB);       // Number of bytes to use in lswi/stswi (0 means 32 bytes)

  // Shift / Rotate (rlwinmx)
  BFVIEW_M(hex, u32, 1, 5, ME);   // Mask end
  BFVIEW_M(hex, u32, 6, 5, MB);   // Mask begin
  BFVIEW_M(hex, u32, 11, 5, SH);  // Shift amount

  // Paired Single Quantized Load/Store
  BFVIEW_M(hex, u32, 12, 3, I);    // Graphics quantization register to use
  BFVIEW_M(hex, bool, 15, 1, W);   // 0: paired single, 1: scalar
  BFVIEW_M(hex, u32, 7, 3, Ix);    // Graphics quantization register to use (indexed)
  BFVIEW_M(hex, bool, 10, 1, Wx);  // 0: paired single, 1: scalar (indexed)

  // Other
  BFVIEW_M(hex, bool, 0, 1, Rc);   // Record bit
                                   // 1, if the condition register should be updated by this
                                   // instruction
  BFVIEW_M(hex, bool, 10, 1, OE);  // Overflow enable
  BFVIEW_M(hex, u32, 12, 8, CRM);  // Field mask, identifies the CR fields to be updated by mtcrf
  BFVIEW_M(hex, u32, 17, 8, FM);   // Field mask, identifies the FPSCR fields to be updated by mtfsf
  BFVIEW_M(hex, u32, 21, 5, TO);   // Conditions on which to trap
  BFVIEW_M(hex, bool, 21, 1, L);   // Use low 32 bits for comparison (invalid for 32-bit CPUs)
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
struct UGQR
{
  u32 Hex = 0;

  BFVIEW_M(Hex, EQuantizeType, 0, 3, st_type)
  BFVIEW_M(Hex, u32, 8, 6, st_scale)
  BFVIEW_M(Hex, EQuantizeType, 16, 3, ld_type)
  BFVIEW_M(Hex, u32, 24, 6, ld_scale)

  UGQR() = default;
  explicit UGQR(u32 hex_) : Hex{hex_} {}
};

#define XER_CA_SHIFT 29
#define XER_OV_SHIFT 30
#define XER_SO_SHIFT 31
#define XER_OV_MASK 1
#define XER_SO_MASK 2
// XER
struct UReg_XER
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 7, BYTE_COUNT)
  BFVIEW_M(Hex, u32, 7, 1, reserved_1)
  BFVIEW_M(Hex, u32, 8, 8, BYTE_CMP)
  BFVIEW_M(Hex, u32, 16, 13, reserved_2)
  BFVIEW_M(Hex, u32, 29, 1, CA)
  BFVIEW_M(Hex, u32, 30, 1, OV)
  BFVIEW_M(Hex, u32, 31, 1, SO)

  UReg_XER() = default;
  explicit UReg_XER(u32 hex_) : Hex{hex_} {}
};

// Machine State Register
struct UReg_MSR
{
  u32 Hex = 0;

  BFVIEW_M(Hex, bool, 0, 1, LE);
  BFVIEW_M(Hex, bool, 1, 1, RI);
  BFVIEW_M(Hex, bool, 2, 1, PM);
  BFVIEW_M(Hex, u32, 3, 1, reserved_1);
  BFVIEW_M(Hex, bool, 4, 1, DR);
  BFVIEW_M(Hex, bool, 5, 1, IR);
  BFVIEW_M(Hex, bool, 6, 1, IP);
  BFVIEW_M(Hex, u32, 7, 1, reserved_2);
  BFVIEW_M(Hex, bool, 8, 1, FE1);
  BFVIEW_M(Hex, bool, 9, 1, BE);
  BFVIEW_M(Hex, bool, 10, 1, SE);
  BFVIEW_M(Hex, bool, 11, 1, FE0);
  BFVIEW_M(Hex, bool, 12, 1, MCHECK);
  BFVIEW_M(Hex, bool, 13, 1, FP);
  BFVIEW_M(Hex, bool, 14, 1, PR);
  BFVIEW_M(Hex, bool, 15, 1, EE);
  BFVIEW_M(Hex, bool, 16, 1, ILE);
  BFVIEW_M(Hex, u32, 17, 1, reserved_3);
  BFVIEW_M(Hex, bool, 18, 1, POW);
  BFVIEW_M(Hex, u32, 19, 13, reserved_4);

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
struct UReg_FPSCR
{
  u32 Hex = 0;

  // Rounding mode (towards: nearest, zero, +inf, -inf)
  BFVIEW_M(Hex, FPURoundMode::RoundMode, 0, 2, RN);
  // Non-IEEE mode enable (aka flush-to-zero)
  BFVIEW_M(Hex, bool, 2, 1, NI);
  // Inexact exception enable
  BFVIEW_M(Hex, bool, 3, 1, XE);
  // IEEE division by zero exception enable
  BFVIEW_M(Hex, bool, 4, 1, ZE);
  // IEEE underflow exception enable
  BFVIEW_M(Hex, bool, 5, 1, UE);
  // IEEE overflow exception enable
  BFVIEW_M(Hex, bool, 6, 1, OE);
  // Invalid operation exception enable
  BFVIEW_M(Hex, bool, 7, 1, VE);
  // Invalid operation exception for integer conversion (sticky)
  BFVIEW_M(Hex, bool, 8, 1, VXCVI);
  // Invalid operation exception for square root (sticky)
  BFVIEW_M(Hex, bool, 9, 1, VXSQRT);
  // Invalid operation exception for software request (sticky)
  BFVIEW_M(Hex, bool, 10, 1, VXSOFT);
  // reserved
  BFVIEW_M(Hex, bool, 11, 1, reserved);
  // Floating point result flags (includes FPCC) (not sticky)
  // from more to less significand: class, <, >, =, ?
  BFVIEW_M(Hex, u32, 12, 5, FPRF);
  // Fraction inexact (not sticky)
  BFVIEW_M(Hex, bool, 17, 1, FI);
  // Fraction rounded (not sticky)
  BFVIEW_M(Hex, bool, 18, 1, FR);
  // Invalid operation exception for invalid comparison (sticky)
  BFVIEW_M(Hex, bool, 19, 1, VXVC);
  // Invalid operation exception for inf * 0 (sticky)
  BFVIEW_M(Hex, bool, 20, 1, VXIMZ);
  // Invalid operation exception for 0 / 0 (sticky)
  BFVIEW_M(Hex, bool, 21, 1, VXZDZ);
  // Invalid operation exception for inf / inf (sticky)
  BFVIEW_M(Hex, bool, 22, 1, VXIDI);
  // Invalid operation exception for inf - inf (sticky)
  BFVIEW_M(Hex, bool, 23, 1, VXISI);
  // Invalid operation exception for SNaN (sticky)
  BFVIEW_M(Hex, bool, 24, 1, VXSNAN);
  // Inexact exception (sticky)
  BFVIEW_M(Hex, bool, 25, 1, XX);
  // Division by zero exception (sticky)
  BFVIEW_M(Hex, bool, 26, 1, ZX);
  // Underflow exception (sticky)
  BFVIEW_M(Hex, bool, 27, 1, UX);
  // Overflow exception (sticky)
  BFVIEW_M(Hex, bool, 28, 1, OX);
  // Invalid operation exception summary (not sticky)
  BFVIEW_M(Hex, bool, 29, 1, VX);
  // Enabled exception summary (not sticky)
  BFVIEW_M(Hex, bool, 30, 1, FEX);
  // Exception summary (sticky)
  BFVIEW_M(Hex, bool, 31, 1, FX);

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
    FI() = 0;
    FR() = 0;
  }
};

// Hardware Implementation-Dependent Register 0
struct UReg_HID0
{
  u32 Hex = 0;

  BFVIEW_M(Hex, bool, 0, 1, NOOPTI);
  BFVIEW_M(Hex, u32, 1, 1, reserved_1);
  BFVIEW_M(Hex, bool, 2, 1, BHT);
  BFVIEW_M(Hex, bool, 3, 1, ABE);
  BFVIEW_M(Hex, u32, 4, 1, reserved_2);
  BFVIEW_M(Hex, bool, 5, 1, BTIC);
  BFVIEW_M(Hex, bool, 6, 1, DCFA);
  BFVIEW_M(Hex, bool, 7, 1, SGE);
  BFVIEW_M(Hex, bool, 8, 1, IFEM);
  BFVIEW_M(Hex, bool, 9, 1, SPD);
  BFVIEW_M(Hex, bool, 10, 1, DCFI);
  BFVIEW_M(Hex, bool, 11, 1, ICFI);
  BFVIEW_M(Hex, bool, 12, 1, DLOCK);
  BFVIEW_M(Hex, bool, 13, 1, ILOCK);
  BFVIEW_M(Hex, bool, 14, 1, DCE);
  BFVIEW_M(Hex, bool, 15, 1, ICE);
  BFVIEW_M(Hex, bool, 16, 1, NHR);
  BFVIEW_M(Hex, u32, 17, 3, reserved_3);
  BFVIEW_M(Hex, bool, 20, 1, DPM);
  BFVIEW_M(Hex, bool, 21, 1, SLEEP);
  BFVIEW_M(Hex, bool, 22, 1, NAP);
  BFVIEW_M(Hex, bool, 23, 1, DOZE);
  BFVIEW_M(Hex, bool, 24, 1, PAR);
  BFVIEW_M(Hex, bool, 25, 1, ECLK);
  BFVIEW_M(Hex, u32, 26, 1, reserved_4);
  BFVIEW_M(Hex, bool, 27, 1, BCLK);
  BFVIEW_M(Hex, bool, 28, 1, EBD);
  BFVIEW_M(Hex, bool, 29, 1, EBA);
  BFVIEW_M(Hex, bool, 30, 1, DBP);
  BFVIEW_M(Hex, bool, 31, 1, EMCP);
};

// Hardware Implementation-Dependent Register 2
struct UReg_HID2
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 16, reserved);
  BFVIEW_M(Hex, bool, 16, 1, DQOEE);
  BFVIEW_M(Hex, bool, 17, 1, DCMEE);
  BFVIEW_M(Hex, bool, 18, 1, DNCEE);
  BFVIEW_M(Hex, bool, 19, 1, DCHEE);
  BFVIEW_M(Hex, bool, 20, 1, DQOERR);
  BFVIEW_M(Hex, bool, 21, 1, DCMERR);
  BFVIEW_M(Hex, bool, 22, 1, DNCERR);
  BFVIEW_M(Hex, bool, 23, 1, DCHERR);
  BFVIEW_M(Hex, u32, 24, 4, DMAQL);
  BFVIEW_M(Hex, bool, 28, 1, LCE);
  BFVIEW_M(Hex, bool, 29, 1, PSE);
  BFVIEW_M(Hex, bool, 30, 1, WPE);
  BFVIEW_M(Hex, bool, 31, 1, LSQE);

  UReg_HID2() = default;
  explicit UReg_HID2(u32 hex_) : Hex{hex_} {}
};

// Hardware Implementation-Dependent Register 4
struct UReg_HID4
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 20, reserved_1);
  BFVIEW_M(Hex, bool, 20, 1, L2CFI);
  BFVIEW_M(Hex, bool, 21, 1, L2MUM);
  BFVIEW_M(Hex, bool, 22, 1, DBP);
  BFVIEW_M(Hex, bool, 23, 1, LPE);
  BFVIEW_M(Hex, bool, 24, 1, ST0);
  BFVIEW_M(Hex, bool, 25, 1, SBE);
  BFVIEW_M(Hex, u32, 26, 1, reserved_2);
  BFVIEW_M(Hex, u32, 27, 2, BPD);
  BFVIEW_M(Hex, u32, 29, 2, L2FM);
  BFVIEW_M(Hex, u32, 31, 1, reserved_3);

  UReg_HID4() = default;
  explicit UReg_HID4(u32 hex_) : Hex{hex_} {}
};

// SDR1 - Page Table format
struct UReg_SDR1
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 9, htabmask);
  BFVIEW_M(Hex, u32, 9, 7, reserved);
  BFVIEW_M(Hex, u32, 16, 16, htaborg);

  UReg_SDR1() = default;
  explicit UReg_SDR1(u32 hex_) : Hex{hex_} {}
};

// MMCR0 - Monitor Mode Control Register 0 format
struct UReg_MMCR0
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 6, PMC2SELECT);
  BFVIEW_M(Hex, u32, 6, 7, PMC1SELECT);
  BFVIEW_M(Hex, bool, 13, 1, PMCTRIGGER);
  BFVIEW_M(Hex, bool, 14, 1, PMCINTCONTROL);
  BFVIEW_M(Hex, bool, 15, 1, PMC1INTCONTROL);
  BFVIEW_M(Hex, u32, 16, 6, THRESHOLD);
  BFVIEW_M(Hex, bool, 22, 1, INTONBITTRANS);
  BFVIEW_M(Hex, u32, 23, 2, RTCSELECT);
  BFVIEW_M(Hex, bool, 25, 1, DISCOUNT);
  BFVIEW_M(Hex, bool, 26, 1, ENINT);
  BFVIEW_M(Hex, bool, 27, 1, DMR);
  BFVIEW_M(Hex, bool, 28, 1, DMS);
  BFVIEW_M(Hex, bool, 29, 1, DU);
  BFVIEW_M(Hex, bool, 30, 1, DP);
  BFVIEW_M(Hex, bool, 31, 1, DIS);
};

// MMCR1 - Monitor Mode Control Register 1 format
struct UReg_MMCR1
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 22, reserved);
  BFVIEW_M(Hex, u32, 22, 5, PMC4SELECT);
  BFVIEW_M(Hex, u32, 27, 5, PMC3SELECT);
};

// Write Pipe Address Register
struct UReg_WPAR
{
  u32 Hex = 0;

  BFVIEW_M(Hex, bool, 0, 1, BNE);
  BFVIEW_M(Hex, u32, 1, 4, reserved);
  BFVIEW_M(Hex, u32, 5, 27, GB_ADDR);

  UReg_WPAR() = default;
  explicit UReg_WPAR(u32 hex_) : Hex{hex_} {}
};

// Direct Memory Access Upper register
struct UReg_DMAU
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 5, DMA_LEN_U);
  BFVIEW_M(Hex, u32, 5, 27, MEM_ADDR);

  UReg_DMAU() = default;
  explicit UReg_DMAU(u32 hex_) : Hex{hex_} {}
};

// Direct Memory Access Lower (DMAL) register
struct UReg_DMAL
{
  u32 Hex = 0;

  BFVIEW_M(Hex, bool, 0, 1, DMA_F);
  BFVIEW_M(Hex, bool, 1, 1, DMA_T);
  BFVIEW_M(Hex, u32, 2, 2, DMA_LEN_L);
  BFVIEW_M(Hex, u32, 4, 1, DMA_LD);
  BFVIEW_M(Hex, u32, 5, 27, LC_ADDR);

  UReg_DMAL() = default;
  explicit UReg_DMAL(u32 hex_) : Hex{hex_} {}
};

struct UReg_BAT_Up
{
  u32 Hex = 0;

  BFVIEW_M(Hex, bool, 0, 1, VP);
  BFVIEW_M(Hex, bool, 1, 1, VS);
  BFVIEW_M(Hex, u32, 2, 11, BL);  // Block length (aka block size mask)
  BFVIEW_M(Hex, u32, 13, 4, reserved);
  BFVIEW_M(Hex, u32, 17, 15, BEPI);

  UReg_BAT_Up() = default;
  explicit UReg_BAT_Up(u32 hex_) : Hex{hex_} {}
};

struct UReg_BAT_Lo
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 2, PP);
  BFVIEW_M(Hex, u32, 2, 1, reserved_1);
  BFVIEW_M(Hex, u32, 3, 4, WIMG);
  BFVIEW_M(Hex, u32, 7, 10, reserved_2);
  BFVIEW_M(Hex, u32, 17, 15, BRPN);  // Physical Block Number

  UReg_BAT_Lo() = default;
  explicit UReg_BAT_Lo(u32 hex_) : Hex{hex_} {}
};

// Segment register
struct UReg_SR
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 24, VSID);      // Virtual segment ID
  BFVIEW_M(Hex, u32, 24, 4, reserved);  // Reserved
  BFVIEW_M(Hex, bool, 28, 1, N);        // No-execute protection
  BFVIEW_M(Hex, bool, 29, 1, Kp);       // User-state protection
  BFVIEW_M(Hex, bool, 30, 1, Ks);       // Supervisor-state protection
  BFVIEW_M(Hex, bool, 31, 1, T);        // Segment register format selector

  // These override other fields if T = 1

  BFVIEW_M(Hex, u32, 0, 20, CNTLR_SPEC);  // Device-specific data for I/O controller
  BFVIEW_M(Hex, u32, 20, 9, BUID);        // Bus unit ID

  UReg_SR() = default;
  explicit UReg_SR(u32 hex_) : Hex{hex_} {}
};

struct UReg_THRM12
{
  u32 Hex = 0;

  BFVIEW_M(Hex, bool, 0, 1, V);    // Valid
  BFVIEW_M(Hex, bool, 1, 1, TIE);  // Thermal Interrupt Enable
  BFVIEW_M(Hex, bool, 2, 1, TID);  // Thermal Interrupt Direction
  BFVIEW_M(Hex, u32, 3, 20, reserved);
  BFVIEW_M(Hex, u32, 23, 7, THRESHOLD);  // Temperature Threshold, 0-127°C
  BFVIEW_M(Hex, bool, 30, 1, TIV);       // Thermal Interrupt Valid
  BFVIEW_M(Hex, bool, 31, 1, TIN);       // Thermal Interrupt

  UReg_THRM12() = default;
  explicit UReg_THRM12(u32 hex_) : Hex{hex_} {}
};

struct UReg_THRM3
{
  u32 Hex = 0;

  BFVIEW_M(Hex, bool, 0, 1, E);     // Enable
  BFVIEW_M(Hex, u32, 1, 13, SITV);  // Sample Interval Timer Value
  BFVIEW_M(Hex, u32, 14, 18, reserved);

  UReg_THRM3() = default;
  explicit UReg_THRM3(u32 hex_) : Hex{hex_} {}
};

struct UPTE_Lo
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 6, API);
  BFVIEW_M(Hex, bool, 6, 1, H);
  BFVIEW_M(Hex, u32, 7, 24, VSID);
  BFVIEW_M(Hex, bool, 31, 1, V);

  UPTE_Lo() = default;
  explicit UPTE_Lo(u32 hex_) : Hex{hex_} {}
};

struct UPTE_Hi
{
  u32 Hex = 0;

  BFVIEW_M(Hex, u32, 0, 2, PP);
  BFVIEW_M(Hex, u32, 2, 1, reserved_1);
  BFVIEW_M(Hex, u32, 3, 4, WIMG);
  BFVIEW_M(Hex, bool, 7, 1, C);
  BFVIEW_M(Hex, bool, 8, 1, R);
  BFVIEW_M(Hex, u32, 9, 3, reserved_2);
  BFVIEW_M(Hex, u32, 12, 20, RPN);

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

constexpr s32 SignExt16(s16 x)
{
  return (s32)x;
}
constexpr s32 SignExt26(u32 x)
{
  return x & 0x2000000 ? (s32)(x | 0xFC000000) : (s32)(x);
}
