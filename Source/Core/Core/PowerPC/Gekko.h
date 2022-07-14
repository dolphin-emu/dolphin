// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Gekko related unions, structs, ...

#pragma once

#include "Common/BitFieldView.h"
#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"

// --- Gekko Instruction ---

struct GeckoInstruction
{
  u32 hex = 0;

  GeckoInstruction() = default;
  GeckoInstruction(u32 hex_) : hex(hex_) {}

  // Opcode
  BFVIEW(u32, 6, 26, OPCD)     // Primary opcode
  BFVIEW(u32, 5, 1, SUBOP5)    // A-Form Extended Opcode
  BFVIEW(u32, 10, 1, SUBOP10)  // X, XL, XFX, XFL-Form Extended Opcode
  BFVIEW(u32, 6, 1, SUBOP6)    // Variation of X-Form Extended Opcode (paired singles indexed)

  // Branch
  BFVIEW(bool, 1, 0, LK)  // Link bit
                          // 1, if branch instructions should put the address of the next
                          // instruction into the link register
  BFVIEW(bool, 1, 1, AA)  // Absolute address bit
                          // 1, if the immediate field represents an absolute address
  BFVIEW(u32, 24, 2, LI)  // 24-bit signed immediate, right-extended by 0b00 (branch displacement)
  BFVIEW(u32, 14, 2, BD)  // 14-bit signed immediate, right-extended by 0b00 (branch displacement)
  BFVIEW(u32, 5, 16, BI)  // Branch condition
  BFVIEW(u32, 5, 21, BO)  // Conditional branch control

  // General-Purpose Register
  BFVIEW(u32, 5, 11, RB)  // Source GPR
  BFVIEW(u32, 5, 16, RA)  // Source GPR
  BFVIEW(u32, 5, 21, RS)  // Source GPR
  BFVIEW(u32, 5, 21, RD)  // Destination GPR

  // Floating-Point Register
  BFVIEW(u32, 5, 6, FC)   // Source FPR
  BFVIEW(u32, 5, 11, FB)  // Source FPR
  BFVIEW(u32, 5, 16, FA)  // Source FPR
  BFVIEW(u32, 5, 21, FS)  // Source FPR
  BFVIEW(u32, 5, 21, FD)  // Destination FPR

  // Compare Register Bit (crxor)
  BFVIEW(u32, 5, 11, CRBB)  // Source bit in the CR
  BFVIEW(u32, 5, 16, CRBA)  // Source bit in the CR
  BFVIEW(u32, 5, 21, CRBD)  // Destination bit in the CR

  // Compare Register Field
  BFVIEW(u32, 3, 18, CRFS)  // Source field in the CR or FPSCR
  BFVIEW(u32, 3, 23, CRFD)  // Destination field in CR or FPSCR

  // Other Register Types
  BFVIEW(u32, 10, 11, SPR)  // Special-purpose register
  BFVIEW(u32, 5, 11, SPRU)  // Upper special-purpose register
  BFVIEW(u32, 5, 16, SPRL)  // Lower special-purpose register
  BFVIEW(u32, 10, 11, TBR)  // Time base register (mftb)
  BFVIEW(u32, 5, 11, TBRU)  // Upper time base register
  BFVIEW(u32, 5, 16, TBRL)  // Lower time base register
  BFVIEW(u32, 4, 16, SR)    // Segment register

  // Immediate
  BFVIEW(u16, 16, 0, UIMM)     // 16-bit unsigned immediate
  BFVIEW(s16, 16, 0, SIMM_16)  // 16-bit signed immediate
  BFVIEW(s16, 12, 0, SIMM_12)  // 12-bit signed immediate (Paired-Singles Load/Store)
  BFVIEW(u32, 5, 11, NB)       // Number of bytes to use in lswi/stswi (0 means 32 bytes)

  // Shift / Rotate (rlwinmx)
  BFVIEW(u32, 5, 1, ME)   // Mask end
  BFVIEW(u32, 5, 6, MB)   // Mask begin
  BFVIEW(u32, 5, 11, SH)  // Shift amount

  // Paired Single Quantized Load/Store
  BFVIEW(u32, 3, 12, I)    // Graphics quantization register to use
  BFVIEW(bool, 1, 15, W)   // 0: paired single, 1: scalar
  BFVIEW(u32, 3, 7, Ix)    // Graphics quantization register to use (indexed)
  BFVIEW(bool, 1, 10, Wx)  // 0: paired single, 1: scalar (indexed)

  // Other
  BFVIEW(bool, 1, 0, Rc)   // Record bit
                           // 1, if the condition register should be updated by this instruction
  BFVIEW(bool, 1, 10, OE)  // Overflow enable
  BFVIEW(u32, 8, 12, CRM)  // Field mask, identifies the CR fields to be updated by mtcrf
  BFVIEW(u32, 8, 17, FM)   // Field mask, identifies the FPSCR fields to be updated by mtfsf
  BFVIEW(u32, 5, 21, TO)   // Conditions on which to trap
  BFVIEW(bool, 1, 21, L)   // Use low 32 bits for comparison (invalid for 32-bit CPUs)
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
struct Reg_GQR
{
  BFVIEW(EQuantizeType, 3, 0, st_type)
  BFVIEW(u32, 6, 8, st_scale)
  BFVIEW(EQuantizeType, 3, 16, ld_type)
  BFVIEW(u32, 6, 24, ld_scale)

  u32 Hex = 0;

  Reg_GQR() = default;
  explicit Reg_GQR(u32 hex_) : Hex{hex_} {}
};

#define XER_CA_SHIFT 29
#define XER_OV_SHIFT 30
#define XER_SO_SHIFT 31
#define XER_OV_MASK 1
#define XER_SO_MASK 2
// XER
struct Reg_XER
{
  BFVIEW(u32, 7, 0, BYTE_COUNT)
  BFVIEW(u32, 1, 7, reserved_1)
  BFVIEW(u32, 8, 8, BYTE_CMP)
  BFVIEW(u32, 13, 16, reserved_2)
  BFVIEW(bool, 1, 29, CA)
  BFVIEW(bool, 1, 30, OV)
  BFVIEW(bool, 1, 31, SO)

  u32 Hex = 0;

  Reg_XER() = default;
  explicit Reg_XER(u32 hex_) : Hex{hex_} {}
};

// Machine State Register
struct Reg_MSR
{
  BFVIEW(bool, 1, 0, LE)
  BFVIEW(bool, 1, 1, RI)
  BFVIEW(bool, 1, 2, PM)
  BFVIEW(u32, 1, 3, reserved_1)
  BFVIEW(bool, 1, 4, DR)
  BFVIEW(bool, 1, 5, IR)
  BFVIEW(bool, 1, 6, IP)
  BFVIEW(u32, 1, 7, reserved_2)
  BFVIEW(bool, 1, 8, FE1)
  BFVIEW(bool, 1, 9, BE)
  BFVIEW(bool, 1, 10, SE)
  BFVIEW(bool, 1, 11, FE0)
  BFVIEW(bool, 1, 12, MCHECK)
  BFVIEW(bool, 1, 13, FP)
  BFVIEW(bool, 1, 14, PR)
  BFVIEW(bool, 1, 15, EE)
  BFVIEW(bool, 1, 16, ILE)
  BFVIEW(u32, 1, 17, reserved_3)
  BFVIEW(bool, 1, 18, POW)
  BFVIEW(u32, 13, 19, reserved_4)

  u32 Hex = 0;

  Reg_MSR() = default;
  explicit Reg_MSR(u32 hex_) : Hex{hex_} {}
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
struct Reg_FPSCR
{
  // Rounding mode (towards: nearest, zero, +inf, -inf)
  BFVIEW(FPURoundMode::RoundMode, 2, 0, RN)
  // Non-IEEE mode enable (aka flush-to-zero)
  BFVIEW(bool, 1, 2, NI)
  // Inexact exception enable
  BFVIEW(bool, 1, 3, XE)
  // IEEE division by zero exception enable
  BFVIEW(bool, 1, 4, ZE)
  // IEEE underflow exception enable
  BFVIEW(bool, 1, 5, UE)
  // IEEE overflow exception enable
  BFVIEW(bool, 1, 6, OE)
  // Invalid operation exception enable
  BFVIEW(bool, 1, 7, VE)
  // Invalid operation exception for integer conversion (sticky)
  BFVIEW(bool, 1, 8, VXCVI)
  // Invalid operation exception for square root (sticky)
  BFVIEW(bool, 1, 9, VXSQRT)
  // Invalid operation exception for software request (sticky)
  BFVIEW(bool, 1, 10, VXSOFT)
  // reserved
  BFVIEW(u32, 1, 11, reserved)
  // Floating point result flags (includes FPCC) (not sticky)
  // from more to less significand: class, <, >, =, ?
  BFVIEW(u32, 5, 12, FPRF)
  // Fraction inexact (not sticky)
  BFVIEW(bool, 1, 17, FI)
  // Fraction rounded (not sticky)
  BFVIEW(bool, 1, 18, FR)
  // Invalid operation exception for invalid comparison (sticky)
  BFVIEW(bool, 1, 19, VXVC)
  // Invalid operation exception for inf * 0 (sticky)
  BFVIEW(bool, 1, 20, VXIMZ)
  // Invalid operation exception for 0 / 0 (sticky)
  BFVIEW(bool, 1, 21, VXZDZ)
  // Invalid operation exception for inf / inf (sticky)
  BFVIEW(bool, 1, 22, VXIDI)
  // Invalid operation exception for inf - inf (sticky)
  BFVIEW(bool, 1, 23, VXISI)
  // Invalid operation exception for SNaN (sticky)
  BFVIEW(bool, 1, 24, VXSNAN)
  // Inexact exception (sticky)
  BFVIEW(bool, 1, 25, XX)
  // Division by zero exception (sticky)
  BFVIEW(bool, 1, 26, ZX)
  // Underflow exception (sticky)
  BFVIEW(bool, 1, 27, UX)
  // Overflow exception (sticky)
  BFVIEW(bool, 1, 28, OX)
  // Invalid operation exception summary (not sticky)
  BFVIEW(bool, 1, 29, VX)
  // Enabled exception summary (not sticky)
  BFVIEW(bool, 1, 30, FEX)
  // Exception summary (sticky)
  BFVIEW(bool, 1, 31, FX)

  u32 Hex = 0;

  // The FPSCR's 20th bit (11th from a little endian perspective)
  // is defined as reserved and set to zero. Attempts to modify it
  // are ignored by hardware, so we do the same.
  static constexpr u32 mask = 0xFFFFF7FF;

  Reg_FPSCR() = default;
  explicit Reg_FPSCR(u32 hex_) : Hex{hex_ & mask} {}

  Reg_FPSCR& operator=(u32 value)
  {
    Hex = value & mask;
    return *this;
  }

  Reg_FPSCR& operator|=(u32 value)
  {
    Hex |= value & mask;
    return *this;
  }

  Reg_FPSCR& operator&=(u32 value)
  {
    Hex &= value;
    return *this;
  }

  Reg_FPSCR& operator^=(u32 value)
  {
    Hex ^= value & mask;
    return *this;
  }

  void ClearFIFR()
  {
    FI() = false;
    FR() = false;
  }
};

// Hardware Implementation-Dependent Register 0
struct Reg_HID0
{
  BFVIEW(bool, 1, 0, NOOPTI)
  BFVIEW(u32, 1, 1, reserved_1)
  BFVIEW(bool, 1, 2, BHT)
  BFVIEW(bool, 1, 3, ABE)
  BFVIEW(u32, 1, 4, reserved_2)
  BFVIEW(bool, 1, 5, BTIC)
  BFVIEW(bool, 1, 6, DCFA)
  BFVIEW(bool, 1, 7, SGE)
  BFVIEW(bool, 1, 8, IFEM)
  BFVIEW(bool, 1, 9, SPD)
  BFVIEW(bool, 1, 10, DCFI)
  BFVIEW(bool, 1, 11, ICFI)
  BFVIEW(bool, 1, 12, DLOCK)
  BFVIEW(bool, 1, 13, ILOCK)
  BFVIEW(bool, 1, 14, DCE)
  BFVIEW(bool, 1, 15, ICE)
  BFVIEW(bool, 1, 16, NHR)
  BFVIEW(u32, 3, 17, reserved_3)
  BFVIEW(bool, 1, 20, DPM)
  BFVIEW(bool, 1, 21, SLEEP)
  BFVIEW(bool, 1, 22, NAP)
  BFVIEW(bool, 1, 23, DOZE)
  BFVIEW(bool, 1, 24, PAR)
  BFVIEW(bool, 1, 25, ECLK)
  BFVIEW(u32, 1, 26, reserved_4)
  BFVIEW(bool, 1, 27, BCLK)
  BFVIEW(bool, 1, 28, EBD)
  BFVIEW(bool, 1, 29, EBA)
  BFVIEW(bool, 1, 30, DBP)
  BFVIEW(bool, 1, 31, EMCP)

  u32 Hex = 0;
};

// Hardware Implementation-Dependent Register 2
struct Reg_HID2
{
  BFVIEW(u32, 16, 0, reserved)
  BFVIEW(bool, 1, 16, DQOEE)
  BFVIEW(bool, 1, 17, DCMEE)
  BFVIEW(bool, 1, 18, DNCEE)
  BFVIEW(bool, 1, 19, DCHEE)
  BFVIEW(bool, 1, 20, DQOERR)
  BFVIEW(bool, 1, 21, DCMERR)
  BFVIEW(bool, 1, 22, DNCERR)
  BFVIEW(bool, 1, 23, DCHERR)
  BFVIEW(u32, 4, 24, DMAQL)
  BFVIEW(bool, 1, 28, LCE)
  BFVIEW(bool, 1, 29, PSE)
  BFVIEW(bool, 1, 30, WPE)
  BFVIEW(bool, 1, 31, LSQE)

  u32 Hex = 0;

  Reg_HID2() = default;
  explicit Reg_HID2(u32 hex_) : Hex{hex_} {}
};

// Hardware Implementation-Dependent Register 4
struct Reg_HID4
{
  BFVIEW(u32, 20, 0, reserved_1)
  BFVIEW(bool, 1, 20, L2CFI)
  BFVIEW(bool, 1, 21, L2MUM)
  BFVIEW(bool, 1, 22, DBP)
  BFVIEW(bool, 1, 23, LPE)
  BFVIEW(bool, 1, 24, ST0)
  BFVIEW(bool, 1, 25, SBE)
  BFVIEW(u32, 1, 26, reserved_2)
  BFVIEW(u32, 2, 27, BPD)
  BFVIEW(u32, 2, 29, L2FM)
  BFVIEW(u32, 1, 31, reserved_3)

  u32 Hex = 0;

  Reg_HID4() = default;
  explicit Reg_HID4(u32 hex_) : Hex{hex_} {}
};

// SDR1 - Page Table format
struct Reg_SDR1
{
  BFVIEW(u32, 9, 0, htabmask)
  BFVIEW(u32, 7, 9, reserved)
  BFVIEW(u32, 16, 16, htaborg)

  u32 Hex = 0;

  Reg_SDR1() = default;
  explicit Reg_SDR1(u32 hex_) : Hex{hex_} {}
};

// MMCR0 - Monitor Mode Control Register 0 format
struct Reg_MMCR0
{
  BFVIEW(u32, 6, 0, PMC2SELECT)
  BFVIEW(u32, 7, 6, PMC1SELECT)
  BFVIEW(bool, 1, 13, PMCTRIGGER)
  BFVIEW(bool, 1, 14, PMCINTCONTROL)
  BFVIEW(bool, 1, 15, PMC1INTCONTROL)
  BFVIEW(u32, 6, 16, THRESHOLD)
  BFVIEW(bool, 1, 22, INTONBITTRANS)
  BFVIEW(u32, 2, 23, RTCSELECT)
  BFVIEW(bool, 1, 25, DISCOUNT)
  BFVIEW(bool, 1, 26, ENINT)
  BFVIEW(bool, 1, 27, DMR)
  BFVIEW(bool, 1, 28, DMS)
  BFVIEW(bool, 1, 29, DU)
  BFVIEW(bool, 1, 30, DP)
  BFVIEW(bool, 1, 31, DIS)

  u32 Hex = 0;
};

// MMCR1 - Monitor Mode Control Register 1 format
struct Reg_MMCR1
{
  BFVIEW(u32, 22, 0, reserved)
  BFVIEW(u32, 5, 22, PMC4SELECT)
  BFVIEW(u32, 5, 27, PMC3SELECT)

  u32 Hex = 0;
};

// Write Pipe Address Register
struct Reg_WPAR
{
  u32 Hex = 0;

  BFVIEW(bool, 1, 0, BNE)
  BFVIEW(u32, 4, 1, reserved)
  BFVIEW(u32, 27, 5, GB_ADDR)

  Reg_WPAR() = default;
  explicit Reg_WPAR(u32 hex_) : Hex{hex_} {}
};

// Direct Memory Access Upper register
struct Reg_DMAU
{
  BFVIEW(u32, 5, 0, DMA_LEN_U)
  BFVIEW(u32, 27, 5, MEM_ADDR)

  u32 Hex = 0;

  Reg_DMAU() = default;
  explicit Reg_DMAU(u32 hex_) : Hex{hex_} {}
};

// Direct Memory Access Lower (DMAL) register
struct Reg_DMAL
{
  u32 Hex = 0;

  BFVIEW(bool, 1, 0, DMA_F)
  BFVIEW(bool, 1, 1, DMA_T)
  BFVIEW(u32, 2, 2, DMA_LEN_L)
  BFVIEW(u32, 1, 4, DMA_LD)
  BFVIEW(u32, 27, 5, LC_ADDR)

  Reg_DMAL() = default;
  explicit Reg_DMAL(u32 hex_) : Hex{hex_} {}
};

struct Reg_BAT_Up
{
  BFVIEW(bool, 1, 0, VP)
  BFVIEW(bool, 1, 1, VS)
  BFVIEW(u32, 11, 2, BL)  // Block length (aka block size mask)
  BFVIEW(u32, 4, 13, reserved)
  BFVIEW(u32, 15, 17, BEPI)

  u32 Hex = 0;

  Reg_BAT_Up() = default;
  explicit Reg_BAT_Up(u32 hex_) : Hex{hex_} {}
};

struct Reg_BAT_Lo
{
  BFVIEW(u32, 2, 0, PP)
  BFVIEW(u32, 1, 2, reserved_1)
  BFVIEW(u32, 4, 3, WIMG)
  BFVIEW(u32, 10, 7, reserved_2)
  BFVIEW(u32, 15, 17, BRPN)  // Physical Block Number

  u32 Hex = 0;

  Reg_BAT_Lo() = default;
  explicit Reg_BAT_Lo(u32 hex_) : Hex{hex_} {}
};

// Segment register
struct Reg_SR
{
  BFVIEW(u32, 24, 0, VSID)      // Virtual segment ID
  BFVIEW(u32, 4, 24, reserved)  // Reserved
  BFVIEW(bool, 1, 28, N)        // No-execute protection
  BFVIEW(bool, 1, 29, Kp)       // User-state protection
  BFVIEW(bool, 1, 30, Ks)       // Supervisor-state protection
  BFVIEW(bool, 1, 31, T)        // Segment register format selector

  // These override other fields if T = 1

  BFVIEW(u32, 20, 0, CNTLR_SPEC)  // Device-specific data for I/O controller
  BFVIEW(u32, 9, 20, BUID)        // Bus unit ID

  u32 Hex = 0;

  Reg_SR() = default;
  explicit Reg_SR(u32 hex_) : Hex{hex_} {}
};

struct Reg_THRM12
{
  BFVIEW(bool, 1, 0, V)    // Valid
  BFVIEW(bool, 1, 1, TIE)  // Thermal Interrupt Enable
  BFVIEW(bool, 1, 2, TID)  // Thermal Interrupt Direction
  BFVIEW(u32, 20, 3, reserved)
  BFVIEW(u32, 7, 23, THRESHOLD)  // Temperature Threshold, 0-127°C
  BFVIEW(bool, 1, 30, TIV)       // Thermal Interrupt Valid
  BFVIEW(bool, 1, 31, TIN)       // Thermal Interrupt

  u32 Hex = 0;

  Reg_THRM12() = default;
  explicit Reg_THRM12(u32 hex_) : Hex{hex_} {}
};

struct Reg_THRM3
{
  BFVIEW(bool, 1, 0, E)     // Enable
  BFVIEW(u32, 13, 1, SITV)  // Sample Interval Timer Value
  BFVIEW(u32, 18, 14, reserved)

  u32 Hex = 0;

  Reg_THRM3() = default;
  explicit Reg_THRM3(u32 hex_) : Hex{hex_} {}
};

struct PTE_Lo
{
  BFVIEW(u32, 6, 0, API)
  BFVIEW(bool, 1, 6, H)
  BFVIEW(u32, 24, 7, VSID)
  BFVIEW(bool, 1, 31, V)

  u32 Hex = 0;

  PTE_Lo() = default;
  explicit PTE_Lo(u32 hex_) : Hex{hex_} {}
};

struct PTE_Hi
{
  BFVIEW(u32, 2, 0, PP)
  BFVIEW(u32, 1, 2, reserved_1)
  BFVIEW(u32, 4, 3, WIMG)
  BFVIEW(bool, 1, 7, C)
  BFVIEW(bool, 1, 8, R)
  BFVIEW(u32, 3, 9, reserved_2)
  BFVIEW(u32, 20, 12, RPN)

  u32 Hex = 0;

  PTE_Hi() = default;
  explicit PTE_Hi(u32 hex_) : Hex{hex_} {}
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
