// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Assembler/AssemblerTables.h"

#include "Common/Assembler/AssemblerShared.h"
#include "Common/Assembler/CaseInsensitiveDict.h"
#include "Common/CommonTypes.h"

namespace Common::GekkoAssembler::detail
{
namespace
{
constexpr size_t PLAIN_MNEMONIC = 0x0;
constexpr size_t RECORD_BIT = 0x1;
constexpr size_t OVERFLOW_EXCEPTION = 0x2;
// Since RC/OE are mutually exclusive from LK/AA, they can occupy the same slot
constexpr size_t LINK_BIT = 0x1;
constexpr size_t ABSOLUTE_ADDRESS_BIT = 0x2;

// Compile-time helpers for mnemonic generation
// Generate inclusive mask [left, right] -- MSB=0 LSB=31
constexpr u32 Mask(u32 left, u32 right)
{
  return static_cast<u32>(((u64{1} << (32 - left)) - 1) & ~((u64{1} << (31 - right)) - 1));
}
constexpr u32 InsertVal(u32 val, u32 left, u32 right)
{
  return val << (31 - right) & Mask(left, right);
}
constexpr u32 InsertOpcode(u32 opcode)
{
  return InsertVal(opcode, 0, 5);
}
constexpr u32 SprBitswap(u32 spr)
{
  return ((spr & 0b0000011111) << 5) | ((spr & 0b1111100000) >> 5);
}

constexpr MnemonicDesc INVALID_MNEMONIC = {0, 0, {}};
constexpr ExtendedMnemonicDesc INVALID_EXT_MNEMONIC = {0, nullptr};

// All operands as referenced by the Gekko/Broadway user manual
// See section 12.1.2 under Chapter 12
constexpr OperandDesc _A = OperandDesc{Mask(11, 15), {16, false}};
constexpr OperandDesc _B = OperandDesc{Mask(16, 20), {11, false}};
constexpr OperandDesc _BD = OperandDesc{Mask(16, 29), {0, true}};
constexpr OperandDesc _BI = OperandDesc{Mask(11, 15), {16, false}};
constexpr OperandDesc _BO = OperandDesc{Mask(6, 10), {21, false}};
constexpr OperandDesc _C = OperandDesc{Mask(21, 25), {6, false}};
constexpr OperandDesc _Crba = OperandDesc{Mask(11, 15), {16, false}};
constexpr OperandDesc _Crbb = OperandDesc{Mask(16, 20), {11, false}};
constexpr OperandDesc _Crbd = OperandDesc{Mask(6, 10), {21, false}};
constexpr OperandDesc _Crfd = OperandDesc{Mask(6, 8), {23, false}};
constexpr OperandDesc _Crfs = OperandDesc{Mask(11, 13), {18, false}};
constexpr OperandDesc _CRM = OperandDesc{Mask(12, 19), {12, false}};
constexpr OperandDesc _D = OperandDesc{Mask(6, 10), {21, false}};
constexpr OperandDesc _FM = OperandDesc{Mask(7, 14), {17, false}};
constexpr OperandDesc _W1 = OperandDesc{Mask(16, 16), {15, false}};
constexpr OperandDesc _W2 = OperandDesc{Mask(21, 21), {10, false}};
constexpr OperandDesc _IMM = OperandDesc{Mask(16, 19), {12, false}};
constexpr OperandDesc _L = OperandDesc{Mask(10, 10), {21, false}};
constexpr OperandDesc _LI = OperandDesc{Mask(6, 29), {0, true}};
constexpr OperandDesc _MB = OperandDesc{Mask(21, 25), {6, false}};
constexpr OperandDesc _ME = OperandDesc{Mask(26, 30), {1, false}};
constexpr OperandDesc _NB = OperandDesc{Mask(16, 20), {11, false}};
constexpr OperandDesc _Offd = OperandDesc{Mask(16, 31), {0, true}};
constexpr OperandDesc _OffdPs = OperandDesc{Mask(20, 31), {0, true}};
constexpr OperandDesc _S = OperandDesc{Mask(6, 10), {21, false}};
constexpr OperandDesc _SH = OperandDesc{Mask(16, 20), {11, false}};
constexpr OperandDesc _SIMM = OperandDesc{Mask(16, 31), {0, true}};
constexpr OperandDesc _SPR = OperandDesc{Mask(11, 20), {11, false}};
constexpr OperandDesc _SR = OperandDesc{Mask(12, 15), {16, false}};
constexpr OperandDesc _TO = OperandDesc{Mask(6, 10), {21, false}};
constexpr OperandDesc _TPR = OperandDesc{Mask(11, 20), {11, false}};
constexpr OperandDesc _UIMM = OperandDesc{Mask(16, 31), {0, false}};
constexpr OperandDesc _I1 = OperandDesc{Mask(17, 19), {12, false}};
constexpr OperandDesc _I2 = OperandDesc{Mask(22, 24), {7, false}};
}  // namespace

void OperandList::Insert(size_t before, u32 val)
{
  overfill = count == MAX_OPERANDS;
  for (size_t i = before + 1; i <= count && i < MAX_OPERANDS; i++)
  {
    std::swap(list[i], list[before]);
  }

  list[before] = Tagged<Interval, u32>({0, 0}, val);
  if (!overfill)
  {
    count++;
  }
}

// OperandDesc holds the shift position for an operand, as well as the mask
// Whether the user provided a valid input for an operand can be determined by the mask
u32 OperandDesc::MaxVal() const
{
  const u32 mask_sh = mask >> shift;
  if (is_signed)
  {
    const u32 mask_hibit = (mask_sh & (mask_sh ^ (mask_sh >> 1)));
    return mask_hibit - 1;
  }
  return mask_sh;
}

u32 OperandDesc::MinVal() const
{
  if (is_signed)
  {
    return ~MaxVal();
  }
  return 0;
}

u32 OperandDesc::TruncBits() const
{
  const u32 mask_sh = mask >> shift;
  const u32 mask_lobit = mask_sh & (mask_sh ^ (mask_sh << 1));
  return mask_lobit - 1;
}

bool OperandDesc::Fits(u32 val) const
{
  const u32 mask_sh = mask >> shift;
  if (is_signed)
  {
    // Get high bit and low bit from a range mask
    const u32 mask_hibit = mask_sh & (mask_sh ^ (mask_sh >> 1));
    const u32 mask_lobit = mask_sh & (mask_sh ^ (mask_sh << 1));
    // Positive max is (signbit - 1)
    // Negative min is ~(Positive Max)
    const u32 positive_max = mask_hibit - 1;
    const u32 negative_max = ~positive_max;
    // Truncated bits are any bits right of the mask that are 0 after shifting
    const u32 truncate_bits = mask_lobit - 1;
    return (val <= positive_max || val >= negative_max) && !(val & truncate_bits);
  }
  return (mask_sh & val) == val;
}

u32 OperandDesc::Fit(u32 val) const
{
  return (val << shift) & mask;
}

///////////////////
// PARSER TABLES //
///////////////////

extern const CaseInsensitiveDict<u32, '_'> sprg_map = {
    {"xer", 1},      {"lr", 8},       {"ctr", 9},      {"dsisr", 18},   {"dar", 19},
    {"dec", 22},     {"sdr1", 25},    {"srr0", 26},    {"srr1", 27},    {"sprg0", 272},
    {"sprg1", 273},  {"sprg2", 274},  {"sprg3", 275},  {"ear", 282},    {"tbl", 284},
    {"tbu", 285},    {"ibat0u", 528}, {"ibat0l", 529}, {"ibat1u", 530}, {"ibat1l", 531},
    {"ibat2u", 532}, {"ibat2l", 533}, {"ibat3u", 534}, {"ibat3l", 535}, {"dbat0u", 536},
    {"dbat0l", 537}, {"dbat1u", 538}, {"dbat1l", 539}, {"dbat2u", 540}, {"dbat2l", 541},
    {"dbat3u", 542}, {"dbat3l", 543}, {"gqr0", 912},   {"gqr1", 913},   {"gqr2", 914},
    {"gqr3", 915},   {"gqr4", 916},   {"gqr5", 917},   {"gqr6", 918},   {"gqr7", 919},
    {"hid2", 920},   {"wpar", 921},   {"dma_u", 922},  {"dma_l", 923},  {"ummcr0", 936},
    {"upmc1", 937},  {"upmc2", 938},  {"usia", 939},   {"ummcr1", 940}, {"upmc3", 941},
    {"upmc4", 942},  {"usda", 943},   {"mmcr0", 952},  {"pmc1", 953},   {"pmc2", 954},
    {"sia", 955},    {"mmcr1", 956},  {"pmc3", 957},   {"pmc4", 958},   {"sda", 959},
    {"hid0", 1008},  {"hid1", 1009},  {"iabr", 1010},  {"dabr", 1013},  {"l2cr", 1017},
    {"ictc", 1019},  {"thrm1", 1020}, {"thrm2", 1021}, {"thrm3", 1022}};

extern const CaseInsensitiveDict<GekkoDirective> directives_map = {
    {"byte", GekkoDirective::Byte},     {"2byte", GekkoDirective::_2byte},
    {"4byte", GekkoDirective::_4byte},  {"8byte", GekkoDirective::_8byte},
    {"float", GekkoDirective::Float},   {"double", GekkoDirective::Double},
    {"locate", GekkoDirective::Locate}, {"padalign", GekkoDirective::PadAlign},
    {"align", GekkoDirective::Align},   {"zeros", GekkoDirective::Zeros},
    {"skip", GekkoDirective::Skip},     {"defvar", GekkoDirective::DefVar},
    {"ascii", GekkoDirective::Ascii},   {"asciz", GekkoDirective::Asciz},
};

#define MNEMONIC(mnemonic_str, mnemonic_enum, variant_bits, alg)                                   \
  {                                                                                                \
    mnemonic_str,                                                                                  \
    {                                                                                              \
      static_cast<size_t>(mnemonic_enum) * VARIANT_PERMUTATIONS + (variant_bits), alg              \
    }                                                                                              \
  }
#define PLAIN_MNEMONIC(mnemonic_str, mnemonic_enum, alg)                                           \
  MNEMONIC(mnemonic_str, mnemonic_enum, PLAIN_MNEMONIC, alg)
#define RC_MNEMONIC(mnemonic_str, mnemonic_enum, alg)                                              \
  MNEMONIC(mnemonic_str, mnemonic_enum, PLAIN_MNEMONIC, alg),                                      \
      MNEMONIC(mnemonic_str ".", mnemonic_enum, RECORD_BIT, alg)
#define OERC_MNEMONIC(mnemonic_str, mnemonic_enum, alg)                                            \
  MNEMONIC(mnemonic_str, mnemonic_enum, PLAIN_MNEMONIC, alg),                                      \
      MNEMONIC(mnemonic_str ".", mnemonic_enum, RECORD_BIT, alg),                                  \
      MNEMONIC(mnemonic_str "o", mnemonic_enum, OVERFLOW_EXCEPTION, alg),                          \
      MNEMONIC(mnemonic_str "o.", mnemonic_enum, (RECORD_BIT | OVERFLOW_EXCEPTION), alg)
#define LK_MNEMONIC(mnemonic_str, mnemonic_enum, alg)                                              \
  MNEMONIC(mnemonic_str, mnemonic_enum, PLAIN_MNEMONIC, alg),                                      \
      MNEMONIC(mnemonic_str "l", mnemonic_enum, LINK_BIT, alg)
#define AALK_MNEMONIC(mnemonic_str, mnemonic_enum, alg)                                            \
  MNEMONIC(mnemonic_str, mnemonic_enum, PLAIN_MNEMONIC, alg),                                      \
      MNEMONIC(mnemonic_str "l", mnemonic_enum, LINK_BIT, alg),                                    \
      MNEMONIC(mnemonic_str "a", mnemonic_enum, ABSOLUTE_ADDRESS_BIT, alg),                        \
      MNEMONIC(mnemonic_str "la", mnemonic_enum, (LINK_BIT | ABSOLUTE_ADDRESS_BIT), alg)

extern const CaseInsensitiveDict<ParseInfo, '.', '_'> mnemonic_tokens = {
    OERC_MNEMONIC("add", GekkoMnemonic::Add, ParseAlg::Op3),
    OERC_MNEMONIC("addc", GekkoMnemonic::Addc, ParseAlg::Op3),
    OERC_MNEMONIC("adde", GekkoMnemonic::Adde, ParseAlg::Op3),
    PLAIN_MNEMONIC("addi", GekkoMnemonic::Addi, ParseAlg::Op3),
    PLAIN_MNEMONIC("addic", GekkoMnemonic::Addic, ParseAlg::Op3),
    PLAIN_MNEMONIC("addic.", GekkoMnemonic::AddicDot, ParseAlg::Op3),
    PLAIN_MNEMONIC("addis", GekkoMnemonic::Addis, ParseAlg::Op3),
    OERC_MNEMONIC("addme", GekkoMnemonic::Addme, ParseAlg::Op2),
    OERC_MNEMONIC("addze", GekkoMnemonic::Addze, ParseAlg::Op2),
    RC_MNEMONIC("and", GekkoMnemonic::And, ParseAlg::Op3),
    RC_MNEMONIC("andc", GekkoMnemonic::Andc, ParseAlg::Op3),
    PLAIN_MNEMONIC("andi.", GekkoMnemonic::AndiDot, ParseAlg::Op3),
    PLAIN_MNEMONIC("andis.", GekkoMnemonic::AndisDot, ParseAlg::Op3),
    AALK_MNEMONIC("b", GekkoMnemonic::B, ParseAlg::Op1),
    AALK_MNEMONIC("bc", GekkoMnemonic::Bc, ParseAlg::Op3),
    LK_MNEMONIC("bcctr", GekkoMnemonic::Bcctr, ParseAlg::Op2),
    LK_MNEMONIC("bclr", GekkoMnemonic::Bclr, ParseAlg::Op2),
    PLAIN_MNEMONIC("cmp", GekkoMnemonic::Cmp, ParseAlg::Op4),
    PLAIN_MNEMONIC("cmpi", GekkoMnemonic::Cmpi, ParseAlg::Op4),
    PLAIN_MNEMONIC("cmpl", GekkoMnemonic::Cmpl, ParseAlg::Op4),
    PLAIN_MNEMONIC("cmpli", GekkoMnemonic::Cmpli, ParseAlg::Op4),
    RC_MNEMONIC("cntlzw", GekkoMnemonic::Cntlzw, ParseAlg::Op2),
    PLAIN_MNEMONIC("crand", GekkoMnemonic::Crand, ParseAlg::Op3),
    PLAIN_MNEMONIC("crandc", GekkoMnemonic::Crandc, ParseAlg::Op3),
    PLAIN_MNEMONIC("creqv", GekkoMnemonic::Creqv, ParseAlg::Op3),
    PLAIN_MNEMONIC("crnand", GekkoMnemonic::Crnand, ParseAlg::Op3),
    PLAIN_MNEMONIC("crnor", GekkoMnemonic::Crnor, ParseAlg::Op3),
    PLAIN_MNEMONIC("cror", GekkoMnemonic::Cror, ParseAlg::Op3),
    PLAIN_MNEMONIC("crorc", GekkoMnemonic::Crorc, ParseAlg::Op3),
    PLAIN_MNEMONIC("crxor", GekkoMnemonic::Crxor, ParseAlg::Op3),
    PLAIN_MNEMONIC("dcbf", GekkoMnemonic::Dcbf, ParseAlg::Op2),
    PLAIN_MNEMONIC("dcbi", GekkoMnemonic::Dcbi, ParseAlg::Op2),
    PLAIN_MNEMONIC("dcbst", GekkoMnemonic::Dcbst, ParseAlg::Op2),
    PLAIN_MNEMONIC("dcbt", GekkoMnemonic::Dcbt, ParseAlg::Op2),
    PLAIN_MNEMONIC("dcbtst", GekkoMnemonic::Dcbtst, ParseAlg::Op2),
    PLAIN_MNEMONIC("dcbz", GekkoMnemonic::Dcbz, ParseAlg::Op2),
    PLAIN_MNEMONIC("dcbz_l", GekkoMnemonic::Dcbz_l, ParseAlg::Op2),
    OERC_MNEMONIC("divw", GekkoMnemonic::Divw, ParseAlg::Op3),
    OERC_MNEMONIC("divwu", GekkoMnemonic::Divwu, ParseAlg::Op3),
    PLAIN_MNEMONIC("eciwx", GekkoMnemonic::Eciwx, ParseAlg::Op3),
    PLAIN_MNEMONIC("ecowx", GekkoMnemonic::Ecowx, ParseAlg::Op3),
    PLAIN_MNEMONIC("eieio", GekkoMnemonic::Eieio, ParseAlg::None),
    RC_MNEMONIC("eqv", GekkoMnemonic::Eqv, ParseAlg::Op3),
    RC_MNEMONIC("extsb", GekkoMnemonic::Extsb, ParseAlg::Op2),
    RC_MNEMONIC("extsh", GekkoMnemonic::Extsh, ParseAlg::Op2),
    RC_MNEMONIC("fabs", GekkoMnemonic::Fabs, ParseAlg::Op2),
    RC_MNEMONIC("fadd", GekkoMnemonic::Fadd, ParseAlg::Op3),
    RC_MNEMONIC("fadds", GekkoMnemonic::Fadds, ParseAlg::Op3),
    PLAIN_MNEMONIC("fcmpo", GekkoMnemonic::Fcmpo, ParseAlg::Op3),
    PLAIN_MNEMONIC("fcmpu", GekkoMnemonic::Fcmpu, ParseAlg::Op3),
    RC_MNEMONIC("fctiw", GekkoMnemonic::Fctiw, ParseAlg::Op2),
    RC_MNEMONIC("fctiwz", GekkoMnemonic::Fctiwz, ParseAlg::Op2),
    RC_MNEMONIC("fdiv", GekkoMnemonic::Fdiv, ParseAlg::Op3),
    RC_MNEMONIC("fdivs", GekkoMnemonic::Fdivs, ParseAlg::Op3),
    RC_MNEMONIC("fmadd", GekkoMnemonic::Fmadd, ParseAlg::Op4),
    RC_MNEMONIC("fmadds", GekkoMnemonic::Fmadds, ParseAlg::Op4),
    RC_MNEMONIC("fmr", GekkoMnemonic::Fmr, ParseAlg::Op2),
    RC_MNEMONIC("fmsub", GekkoMnemonic::Fmsub, ParseAlg::Op4),
    RC_MNEMONIC("fmsubs", GekkoMnemonic::Fmsubs, ParseAlg::Op4),
    RC_MNEMONIC("fmul", GekkoMnemonic::Fmul, ParseAlg::Op3),
    RC_MNEMONIC("fmuls", GekkoMnemonic::Fmuls, ParseAlg::Op3),
    RC_MNEMONIC("fnabs", GekkoMnemonic::Fnabs, ParseAlg::Op2),
    RC_MNEMONIC("fneg", GekkoMnemonic::Fneg, ParseAlg::Op2),
    RC_MNEMONIC("fnmadd", GekkoMnemonic::Fnmadd, ParseAlg::Op4),
    RC_MNEMONIC("fnmadds", GekkoMnemonic::Fnmadds, ParseAlg::Op4),
    RC_MNEMONIC("fnmsub", GekkoMnemonic::Fnmsub, ParseAlg::Op4),
    RC_MNEMONIC("fnmsubs", GekkoMnemonic::Fnmsubs, ParseAlg::Op4),
    RC_MNEMONIC("fres", GekkoMnemonic::Fres, ParseAlg::Op2),
    RC_MNEMONIC("frsp", GekkoMnemonic::Frsp, ParseAlg::Op2),
    RC_MNEMONIC("frsqrte", GekkoMnemonic::Frsqrte, ParseAlg::Op2),
    RC_MNEMONIC("fsel", GekkoMnemonic::Fsel, ParseAlg::Op4),
    RC_MNEMONIC("fsub", GekkoMnemonic::Fsub, ParseAlg::Op3),
    RC_MNEMONIC("fsubs", GekkoMnemonic::Fsubs, ParseAlg::Op3),
    PLAIN_MNEMONIC("icbi", GekkoMnemonic::Icbi, ParseAlg::Op2),
    PLAIN_MNEMONIC("isync", GekkoMnemonic::Isync, ParseAlg::None),
    PLAIN_MNEMONIC("lbz", GekkoMnemonic::Lbz, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lbzu", GekkoMnemonic::Lbzu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lbzux", GekkoMnemonic::Lbzux, ParseAlg::Op3),
    PLAIN_MNEMONIC("lbzx", GekkoMnemonic::Lbzx, ParseAlg::Op3),
    PLAIN_MNEMONIC("lfd", GekkoMnemonic::Lfd, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lfdu", GekkoMnemonic::Lfdu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lfdux", GekkoMnemonic::Lfdux, ParseAlg::Op3),
    PLAIN_MNEMONIC("lfdx", GekkoMnemonic::Lfdx, ParseAlg::Op3),
    PLAIN_MNEMONIC("lfs", GekkoMnemonic::Lfs, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lfsu", GekkoMnemonic::Lfsu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lfsux", GekkoMnemonic::Lfsux, ParseAlg::Op3),
    PLAIN_MNEMONIC("lfsx", GekkoMnemonic::Lfsx, ParseAlg::Op3),
    PLAIN_MNEMONIC("lha", GekkoMnemonic::Lha, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lhau", GekkoMnemonic::Lhau, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lhaux", GekkoMnemonic::Lhaux, ParseAlg::Op3),
    PLAIN_MNEMONIC("lhax", GekkoMnemonic::Lhax, ParseAlg::Op3),
    PLAIN_MNEMONIC("lhbrx", GekkoMnemonic::Lhbrx, ParseAlg::Op3),
    PLAIN_MNEMONIC("lhz", GekkoMnemonic::Lhz, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lhzu", GekkoMnemonic::Lhzu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lhzux", GekkoMnemonic::Lhzux, ParseAlg::Op3),
    PLAIN_MNEMONIC("lhzx", GekkoMnemonic::Lhzx, ParseAlg::Op3),
    PLAIN_MNEMONIC("lmw", GekkoMnemonic::Lmw, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lswi", GekkoMnemonic::Lswi, ParseAlg::Op3),
    PLAIN_MNEMONIC("lswx", GekkoMnemonic::Lswx, ParseAlg::Op3),
    PLAIN_MNEMONIC("lwarx", GekkoMnemonic::Lwarx, ParseAlg::Op3),
    PLAIN_MNEMONIC("lwbrx", GekkoMnemonic::Lwbrx, ParseAlg::Op3),
    PLAIN_MNEMONIC("lwz", GekkoMnemonic::Lwz, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lwzu", GekkoMnemonic::Lwzu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("lwzux", GekkoMnemonic::Lwzux, ParseAlg::Op3),
    PLAIN_MNEMONIC("lwzx", GekkoMnemonic::Lwzx, ParseAlg::Op3),
    PLAIN_MNEMONIC("mcrf", GekkoMnemonic::Mcrf, ParseAlg::Op2),
    PLAIN_MNEMONIC("mcrfs", GekkoMnemonic::Mcrfs, ParseAlg::Op2),
    PLAIN_MNEMONIC("mcrxr", GekkoMnemonic::Mcrxr, ParseAlg::Op1),
    PLAIN_MNEMONIC("mfcr", GekkoMnemonic::Mfcr, ParseAlg::Op1),
    RC_MNEMONIC("mffs", GekkoMnemonic::Mffs, ParseAlg::Op1),
    PLAIN_MNEMONIC("mfmsr", GekkoMnemonic::Mfmsr, ParseAlg::Op1),
    PLAIN_MNEMONIC("mfspr_nobitswap", GekkoMnemonic::Mfspr_nobitswap, ParseAlg::Op2),
    PLAIN_MNEMONIC("mfsr", GekkoMnemonic::Mfsr, ParseAlg::Op2),
    PLAIN_MNEMONIC("mfsrin", GekkoMnemonic::Mfsrin, ParseAlg::Op2),
    PLAIN_MNEMONIC("mftb_nobitswap", GekkoMnemonic::Mftb_nobitswap, ParseAlg::Op2),
    PLAIN_MNEMONIC("mtcrf", GekkoMnemonic::Mtcrf, ParseAlg::Op2),
    RC_MNEMONIC("mtfsb0", GekkoMnemonic::Mtfsb0, ParseAlg::Op1),
    RC_MNEMONIC("mtfsb1", GekkoMnemonic::Mtfsb1, ParseAlg::Op1),
    RC_MNEMONIC("mtfsf", GekkoMnemonic::Mtfsf, ParseAlg::Op2),
    RC_MNEMONIC("mtfsfi", GekkoMnemonic::Mtfsfi, ParseAlg::Op2),
    PLAIN_MNEMONIC("mtmsr", GekkoMnemonic::Mtmsr, ParseAlg::Op1),
    PLAIN_MNEMONIC("mtspr_nobitswap", GekkoMnemonic::Mtspr_nobitswap, ParseAlg::Op2),
    PLAIN_MNEMONIC("mtsr", GekkoMnemonic::Mtsr, ParseAlg::Op2),
    PLAIN_MNEMONIC("mtsrin", GekkoMnemonic::Mtsrin, ParseAlg::Op2),
    RC_MNEMONIC("mulhw", GekkoMnemonic::Mulhw, ParseAlg::Op3),
    RC_MNEMONIC("mulhwu", GekkoMnemonic::Mulhwu, ParseAlg::Op3),
    PLAIN_MNEMONIC("mulli", GekkoMnemonic::Mulli, ParseAlg::Op3),
    OERC_MNEMONIC("mullw", GekkoMnemonic::Mullw, ParseAlg::Op3),
    RC_MNEMONIC("nand", GekkoMnemonic::Nand, ParseAlg::Op3),
    OERC_MNEMONIC("neg", GekkoMnemonic::Neg, ParseAlg::Op2),
    RC_MNEMONIC("nor", GekkoMnemonic::Nor, ParseAlg::Op3),
    RC_MNEMONIC("or", GekkoMnemonic::Or, ParseAlg::Op3),
    RC_MNEMONIC("orc", GekkoMnemonic::Orc, ParseAlg::Op3),
    PLAIN_MNEMONIC("ori", GekkoMnemonic::Ori, ParseAlg::Op3),
    PLAIN_MNEMONIC("oris", GekkoMnemonic::Oris, ParseAlg::Op3),
    PLAIN_MNEMONIC("psq_l", GekkoMnemonic::Psq_l, ParseAlg::Op1Off1Op2),
    PLAIN_MNEMONIC("psq_lu", GekkoMnemonic::Psq_lu, ParseAlg::Op1Off1Op2),
    PLAIN_MNEMONIC("psq_lux", GekkoMnemonic::Psq_lux, ParseAlg::Op5),
    PLAIN_MNEMONIC("psq_lx", GekkoMnemonic::Psq_lx, ParseAlg::Op5),
    PLAIN_MNEMONIC("psq_st", GekkoMnemonic::Psq_st, ParseAlg::Op1Off1Op2),
    PLAIN_MNEMONIC("psq_stu", GekkoMnemonic::Psq_stu, ParseAlg::Op1Off1Op2),
    PLAIN_MNEMONIC("psq_stux", GekkoMnemonic::Psq_stux, ParseAlg::Op5),
    PLAIN_MNEMONIC("psq_stx", GekkoMnemonic::Psq_stx, ParseAlg::Op5),
    RC_MNEMONIC("ps_abs", GekkoMnemonic::Ps_abs, ParseAlg::Op2),
    RC_MNEMONIC("ps_add", GekkoMnemonic::Ps_add, ParseAlg::Op3),
    PLAIN_MNEMONIC("ps_cmpo0", GekkoMnemonic::Ps_cmpo0, ParseAlg::Op3),
    PLAIN_MNEMONIC("ps_cmpo1", GekkoMnemonic::Ps_cmpo1, ParseAlg::Op3),
    PLAIN_MNEMONIC("ps_cmpu0", GekkoMnemonic::Ps_cmpu0, ParseAlg::Op3),
    PLAIN_MNEMONIC("ps_cmpu1", GekkoMnemonic::Ps_cmpu1, ParseAlg::Op3),
    RC_MNEMONIC("ps_div", GekkoMnemonic::Ps_div, ParseAlg::Op3),
    RC_MNEMONIC("ps_madd", GekkoMnemonic::Ps_madd, ParseAlg::Op4),
    RC_MNEMONIC("ps_madds0", GekkoMnemonic::Ps_madds0, ParseAlg::Op4),
    RC_MNEMONIC("ps_madds1", GekkoMnemonic::Ps_madds1, ParseAlg::Op4),
    RC_MNEMONIC("ps_merge00", GekkoMnemonic::Ps_merge00, ParseAlg::Op3),
    RC_MNEMONIC("ps_merge01", GekkoMnemonic::Ps_merge01, ParseAlg::Op3),
    RC_MNEMONIC("ps_merge10", GekkoMnemonic::Ps_merge10, ParseAlg::Op3),
    RC_MNEMONIC("ps_merge11", GekkoMnemonic::Ps_merge11, ParseAlg::Op3),
    RC_MNEMONIC("ps_mr", GekkoMnemonic::Ps_mr, ParseAlg::Op2),
    RC_MNEMONIC("ps_msub", GekkoMnemonic::Ps_msub, ParseAlg::Op4),
    RC_MNEMONIC("ps_mul", GekkoMnemonic::Ps_mul, ParseAlg::Op3),
    RC_MNEMONIC("ps_muls0", GekkoMnemonic::Ps_muls0, ParseAlg::Op3),
    RC_MNEMONIC("ps_muls1", GekkoMnemonic::Ps_muls1, ParseAlg::Op3),
    RC_MNEMONIC("ps_nabs", GekkoMnemonic::Ps_nabs, ParseAlg::Op2),
    RC_MNEMONIC("ps_neg", GekkoMnemonic::Ps_neg, ParseAlg::Op2),
    RC_MNEMONIC("ps_nmadd", GekkoMnemonic::Ps_nmadd, ParseAlg::Op4),
    RC_MNEMONIC("ps_nmsub", GekkoMnemonic::Ps_nmsub, ParseAlg::Op4),
    RC_MNEMONIC("ps_res", GekkoMnemonic::Ps_res, ParseAlg::Op2),
    RC_MNEMONIC("ps_rsqrte", GekkoMnemonic::Ps_rsqrte, ParseAlg::Op2),
    RC_MNEMONIC("ps_sel", GekkoMnemonic::Ps_sel, ParseAlg::Op4),
    RC_MNEMONIC("ps_sub", GekkoMnemonic::Ps_sub, ParseAlg::Op3),
    RC_MNEMONIC("ps_sum0", GekkoMnemonic::Ps_sum0, ParseAlg::Op4),
    RC_MNEMONIC("ps_sum1", GekkoMnemonic::Ps_sum1, ParseAlg::Op4),
    PLAIN_MNEMONIC("rfi", GekkoMnemonic::Rfi, ParseAlg::None),
    RC_MNEMONIC("rlwimi", GekkoMnemonic::Rlwimi, ParseAlg::Op5),
    RC_MNEMONIC("rlwinm", GekkoMnemonic::Rlwinm, ParseAlg::Op5),
    RC_MNEMONIC("rlwnm", GekkoMnemonic::Rlwnm, ParseAlg::Op5),
    PLAIN_MNEMONIC("sc", GekkoMnemonic::Sc, ParseAlg::None),
    RC_MNEMONIC("slw", GekkoMnemonic::Slw, ParseAlg::Op3),
    RC_MNEMONIC("sraw", GekkoMnemonic::Sraw, ParseAlg::Op3),
    RC_MNEMONIC("srawi", GekkoMnemonic::Srawi, ParseAlg::Op3),
    RC_MNEMONIC("srw", GekkoMnemonic::Srw, ParseAlg::Op3),
    PLAIN_MNEMONIC("stb", GekkoMnemonic::Stb, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("stbu", GekkoMnemonic::Stbu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("stbux", GekkoMnemonic::Stbux, ParseAlg::Op3),
    PLAIN_MNEMONIC("stbx", GekkoMnemonic::Stbx, ParseAlg::Op3),
    PLAIN_MNEMONIC("stfd", GekkoMnemonic::Stfd, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("stfdu", GekkoMnemonic::Stfdu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("stfdux", GekkoMnemonic::Stfdux, ParseAlg::Op3),
    PLAIN_MNEMONIC("stfdx", GekkoMnemonic::Stfdx, ParseAlg::Op3),
    PLAIN_MNEMONIC("stfiwx", GekkoMnemonic::Stfiwx, ParseAlg::Op3),
    PLAIN_MNEMONIC("stfs", GekkoMnemonic::Stfs, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("stfsu", GekkoMnemonic::Stfsu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("stfsux", GekkoMnemonic::Stfsux, ParseAlg::Op3),
    PLAIN_MNEMONIC("stfsx", GekkoMnemonic::Stfsx, ParseAlg::Op3),
    PLAIN_MNEMONIC("sth", GekkoMnemonic::Sth, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("sthbrx", GekkoMnemonic::Sthbrx, ParseAlg::Op3),
    PLAIN_MNEMONIC("sthu", GekkoMnemonic::Sthu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("sthux", GekkoMnemonic::Sthux, ParseAlg::Op3),
    PLAIN_MNEMONIC("sthx", GekkoMnemonic::Sthx, ParseAlg::Op3),
    PLAIN_MNEMONIC("stmw", GekkoMnemonic::Stmw, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("stswi", GekkoMnemonic::Stswi, ParseAlg::Op3),
    PLAIN_MNEMONIC("stswx", GekkoMnemonic::Stswx, ParseAlg::Op3),
    PLAIN_MNEMONIC("stw", GekkoMnemonic::Stw, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("stwbrx", GekkoMnemonic::Stwbrx, ParseAlg::Op3),
    PLAIN_MNEMONIC("stwcx.", GekkoMnemonic::StwcxDot, ParseAlg::Op3),
    PLAIN_MNEMONIC("stwu", GekkoMnemonic::Stwu, ParseAlg::Op1Off1),
    PLAIN_MNEMONIC("stwux", GekkoMnemonic::Stwux, ParseAlg::Op3),
    PLAIN_MNEMONIC("stwx", GekkoMnemonic::Stwx, ParseAlg::Op3),
    OERC_MNEMONIC("subf", GekkoMnemonic::Subf, ParseAlg::Op3),
    OERC_MNEMONIC("subfc", GekkoMnemonic::Subfc, ParseAlg::Op3),
    OERC_MNEMONIC("subfe", GekkoMnemonic::Subfe, ParseAlg::Op3),
    PLAIN_MNEMONIC("subfic", GekkoMnemonic::Subfic, ParseAlg::Op3),
    OERC_MNEMONIC("subfme", GekkoMnemonic::Subfme, ParseAlg::Op2),
    OERC_MNEMONIC("subfze", GekkoMnemonic::Subfze, ParseAlg::Op2),
    PLAIN_MNEMONIC("sync", GekkoMnemonic::Sync, ParseAlg::None),
    PLAIN_MNEMONIC("tlbie", GekkoMnemonic::Tlbie, ParseAlg::Op1),
    PLAIN_MNEMONIC("tlbsync", GekkoMnemonic::Tlbsync, ParseAlg::None),
    PLAIN_MNEMONIC("tw", GekkoMnemonic::Tw, ParseAlg::Op3),
    PLAIN_MNEMONIC("twi", GekkoMnemonic::Twi, ParseAlg::Op3),
    RC_MNEMONIC("xor", GekkoMnemonic::Xor, ParseAlg::Op3),
    PLAIN_MNEMONIC("xori", GekkoMnemonic::Xori, ParseAlg::Op3),
    PLAIN_MNEMONIC("xoris", GekkoMnemonic::Xoris, ParseAlg::Op3),
};

#define PSEUDO(mnemonic, base, variant_bits, alg)                                                  \
  {                                                                                                \
    mnemonic, { static_cast<size_t>(base) * VARIANT_PERMUTATIONS + (variant_bits), alg }           \
  }
#define PLAIN_PSEUDO(mnemonic, base, alg) PSEUDO(mnemonic, base, PLAIN_MNEMONIC, alg)
#define RC_PSEUDO(mnemonic, base, alg)                                                             \
  PSEUDO(mnemonic, base, PLAIN_MNEMONIC, alg), PSEUDO(mnemonic ".", base, RECORD_BIT, alg)
#define OERC_PSEUDO(mnemonic, base, alg)                                                           \
  PSEUDO(mnemonic, base, PLAIN_MNEMONIC, alg), PSEUDO(mnemonic ".", base, RECORD_BIT, alg),        \
      PSEUDO(mnemonic "o", base, OVERFLOW_EXCEPTION, alg),                                         \
      PSEUDO(mnemonic "o.", base, (RECORD_BIT | OVERFLOW_EXCEPTION), alg)
#define LK_PSEUDO(mnemonic, base, alg)                                                             \
  PSEUDO(mnemonic, base, PLAIN_MNEMONIC, alg), PSEUDO(mnemonic "l", base, LINK_BIT, alg)
#define LKAA_PSEUDO(mnemonic, base, alg)                                                           \
  PSEUDO(mnemonic, base, PLAIN_MNEMONIC, alg), PSEUDO(mnemonic "l", base, LINK_BIT, alg),          \
      PSEUDO(mnemonic "a", base, ABSOLUTE_ADDRESS_BIT, alg),                                       \
      PSEUDO(mnemonic "la", base, (LINK_BIT | ABSOLUTE_ADDRESS_BIT), alg)
#define LKPRED_PSEUDO(mnemonic, base, alg)                                                         \
  PSEUDO(mnemonic, base, PLAIN_MNEMONIC, alg), PSEUDO(mnemonic "l", base, LINK_BIT, alg),          \
      PSEUDO(mnemonic "-", base, PLAIN_MNEMONIC, alg), PSEUDO(mnemonic "l-", base, LINK_BIT, alg), \
      PSEUDO(mnemonic "+", base##Predict, PLAIN_MNEMONIC, alg),                                    \
      PSEUDO(mnemonic "l+", base##Predict, LINK_BIT, alg)
#define LKAAPRED_PSEUDO(mnemonic, base, alg)                                                       \
  PSEUDO(mnemonic, base, PLAIN_MNEMONIC, alg), PSEUDO(mnemonic "l", base, LINK_BIT, alg),          \
      PSEUDO(mnemonic "a", base, ABSOLUTE_ADDRESS_BIT, alg),                                       \
      PSEUDO(mnemonic "la", base, (LINK_BIT | ABSOLUTE_ADDRESS_BIT), alg),                         \
      PSEUDO(mnemonic "-", base, PLAIN_MNEMONIC, alg), PSEUDO(mnemonic "l-", base, LINK_BIT, alg), \
      PSEUDO(mnemonic "a-", base, ABSOLUTE_ADDRESS_BIT, alg),                                      \
      PSEUDO(mnemonic "la-", base, (LINK_BIT | ABSOLUTE_ADDRESS_BIT), alg),                        \
      PSEUDO(mnemonic "+", base##Predict, PLAIN_MNEMONIC, alg),                                    \
      PSEUDO(mnemonic "l+", base##Predict, LINK_BIT, alg),                                         \
      PSEUDO(mnemonic "a+", base##Predict, ABSOLUTE_ADDRESS_BIT, alg),                             \
      PSEUDO(mnemonic "la+", base##Predict, (LINK_BIT | ABSOLUTE_ADDRESS_BIT), alg)

extern const CaseInsensitiveDict<ParseInfo, '.', '_', '+', '-'> extended_mnemonic_tokens = {
    PLAIN_PSEUDO("subi", ExtendedGekkoMnemonic::Subi, ParseAlg::Op3),
    PLAIN_PSEUDO("subis", ExtendedGekkoMnemonic::Subis, ParseAlg::Op3),
    PLAIN_PSEUDO("subic", ExtendedGekkoMnemonic::Subic, ParseAlg::Op3),
    PLAIN_PSEUDO("subic.", ExtendedGekkoMnemonic::SubicDot, ParseAlg::Op3),
    OERC_PSEUDO("sub", ExtendedGekkoMnemonic::Sub, ParseAlg::Op3),
    OERC_PSEUDO("subc", ExtendedGekkoMnemonic::Subc, ParseAlg::Op3),
    PLAIN_PSEUDO("cmpwi", ExtendedGekkoMnemonic::Cmpwi, ParseAlg::Op2Or3),
    PLAIN_PSEUDO("cmpw", ExtendedGekkoMnemonic::Cmpw, ParseAlg::Op2Or3),
    PLAIN_PSEUDO("cmplwi", ExtendedGekkoMnemonic::Cmplwi, ParseAlg::Op2Or3),
    PLAIN_PSEUDO("cmplw", ExtendedGekkoMnemonic::Cmplw, ParseAlg::Op2Or3),
    RC_PSEUDO("extlwi", ExtendedGekkoMnemonic::Extlwi, ParseAlg::Op4),
    RC_PSEUDO("extrwi", ExtendedGekkoMnemonic::Extrwi, ParseAlg::Op4),
    RC_PSEUDO("inslwi", ExtendedGekkoMnemonic::Inslwi, ParseAlg::Op4),
    RC_PSEUDO("insrwi", ExtendedGekkoMnemonic::Insrwi, ParseAlg::Op4),
    RC_PSEUDO("rotlwi", ExtendedGekkoMnemonic::Rotlwi, ParseAlg::Op3),
    RC_PSEUDO("rotrwi", ExtendedGekkoMnemonic::Rotrwi, ParseAlg::Op3),
    RC_PSEUDO("rotlw", ExtendedGekkoMnemonic::Rotlw, ParseAlg::Op3),
    RC_PSEUDO("slwi", ExtendedGekkoMnemonic::Slwi, ParseAlg::Op3),
    RC_PSEUDO("srwi", ExtendedGekkoMnemonic::Srwi, ParseAlg::Op3),
    RC_PSEUDO("clrlwi", ExtendedGekkoMnemonic::Clrlwi, ParseAlg::Op3),
    RC_PSEUDO("clrrwi", ExtendedGekkoMnemonic::Clrrwi, ParseAlg::Op3),
    RC_PSEUDO("clrlslwi", ExtendedGekkoMnemonic::Clrlslwi, ParseAlg::Op4),
    LKAAPRED_PSEUDO("bt", ExtendedGekkoMnemonic::Bt, ParseAlg::Op2),
    LKAAPRED_PSEUDO("bf", ExtendedGekkoMnemonic::Bf, ParseAlg::Op2),
    LKAAPRED_PSEUDO("bdnz", ExtendedGekkoMnemonic::Bdnz, ParseAlg::Op1),
    LKAAPRED_PSEUDO("bdnzt", ExtendedGekkoMnemonic::Bdnzt, ParseAlg::Op2),
    LKAAPRED_PSEUDO("bdnzf", ExtendedGekkoMnemonic::Bdnzf, ParseAlg::Op2),
    LKAAPRED_PSEUDO("bdz", ExtendedGekkoMnemonic::Bdz, ParseAlg::Op1),
    LKAAPRED_PSEUDO("bdzt", ExtendedGekkoMnemonic::Bdzt, ParseAlg::Op2),
    LKAAPRED_PSEUDO("bdzf", ExtendedGekkoMnemonic::Bdzf, ParseAlg::Op2),
    LK_PSEUDO("blr", ExtendedGekkoMnemonic::Blr, ParseAlg::None),
    LK_PSEUDO("bctr", ExtendedGekkoMnemonic::Bctr, ParseAlg::None),
    LKPRED_PSEUDO("btlr", ExtendedGekkoMnemonic::Btlr, ParseAlg::Op1),
    LKPRED_PSEUDO("btctr", ExtendedGekkoMnemonic::Btctr, ParseAlg::Op1),
    LKPRED_PSEUDO("bflr", ExtendedGekkoMnemonic::Bflr, ParseAlg::Op1),
    LKPRED_PSEUDO("bfctr", ExtendedGekkoMnemonic::Bfctr, ParseAlg::Op1),
    LKPRED_PSEUDO("bdnzlr", ExtendedGekkoMnemonic::Bdnzlr, ParseAlg::None),
    LKPRED_PSEUDO("bdnztlr", ExtendedGekkoMnemonic::Bdnztlr, ParseAlg::Op1),
    LKPRED_PSEUDO("bdnzflr", ExtendedGekkoMnemonic::Bdnzflr, ParseAlg::Op1),
    LKPRED_PSEUDO("bdzlr", ExtendedGekkoMnemonic::Bdzlr, ParseAlg::None),
    LKPRED_PSEUDO("bdztlr", ExtendedGekkoMnemonic::Bdztlr, ParseAlg::Op1),
    LKPRED_PSEUDO("bdzflr", ExtendedGekkoMnemonic::Bdzflr, ParseAlg::Op1),
    LKAAPRED_PSEUDO("blt", ExtendedGekkoMnemonic::Blt, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("ble", ExtendedGekkoMnemonic::Ble, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("beq", ExtendedGekkoMnemonic::Beq, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("bge", ExtendedGekkoMnemonic::Bge, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("bgt", ExtendedGekkoMnemonic::Bgt, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("bnl", ExtendedGekkoMnemonic::Bnl, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("bne", ExtendedGekkoMnemonic::Bne, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("bng", ExtendedGekkoMnemonic::Bng, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("bso", ExtendedGekkoMnemonic::Bso, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("bns", ExtendedGekkoMnemonic::Bns, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("bun", ExtendedGekkoMnemonic::Bun, ParseAlg::Op1Or2),
    LKAAPRED_PSEUDO("bnu", ExtendedGekkoMnemonic::Bnu, ParseAlg::Op1Or2),
    LKPRED_PSEUDO("bltlr", ExtendedGekkoMnemonic::Bltlr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bltctr", ExtendedGekkoMnemonic::Bltctr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("blelr", ExtendedGekkoMnemonic::Blelr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("blectr", ExtendedGekkoMnemonic::Blectr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("beqlr", ExtendedGekkoMnemonic::Beqlr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("beqctr", ExtendedGekkoMnemonic::Beqctr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bgelr", ExtendedGekkoMnemonic::Bgelr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bgectr", ExtendedGekkoMnemonic::Bgectr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bgtlr", ExtendedGekkoMnemonic::Bgtlr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bgtctr", ExtendedGekkoMnemonic::Bgtctr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bnllr", ExtendedGekkoMnemonic::Bnllr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bnlctr", ExtendedGekkoMnemonic::Bnlctr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bnelr", ExtendedGekkoMnemonic::Bnelr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bnectr", ExtendedGekkoMnemonic::Bnectr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bnglr", ExtendedGekkoMnemonic::Bnglr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bngctr", ExtendedGekkoMnemonic::Bngctr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bsolr", ExtendedGekkoMnemonic::Bsolr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bsoctr", ExtendedGekkoMnemonic::Bsoctr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bnslr", ExtendedGekkoMnemonic::Bnslr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bnsctr", ExtendedGekkoMnemonic::Bnsctr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bunlr", ExtendedGekkoMnemonic::Bunlr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bunctr", ExtendedGekkoMnemonic::Bunctr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bnulr", ExtendedGekkoMnemonic::Bnulr, ParseAlg::NoneOrOp1),
    LKPRED_PSEUDO("bnuctr", ExtendedGekkoMnemonic::Bnuctr, ParseAlg::NoneOrOp1),
    PLAIN_PSEUDO("crset", ExtendedGekkoMnemonic::Crset, ParseAlg::Op1),
    PLAIN_PSEUDO("crclr", ExtendedGekkoMnemonic::Crclr, ParseAlg::Op1),
    PLAIN_PSEUDO("crmove", ExtendedGekkoMnemonic::Crmove, ParseAlg::Op2),
    PLAIN_PSEUDO("crnot", ExtendedGekkoMnemonic::Crnot, ParseAlg::Op2),
    PLAIN_PSEUDO("twlt", ExtendedGekkoMnemonic::Twlt, ParseAlg::Op2),
    PLAIN_PSEUDO("twlti", ExtendedGekkoMnemonic::Twlti, ParseAlg::Op2),
    PLAIN_PSEUDO("twle", ExtendedGekkoMnemonic::Twle, ParseAlg::Op2),
    PLAIN_PSEUDO("twlei", ExtendedGekkoMnemonic::Twlei, ParseAlg::Op2),
    PLAIN_PSEUDO("tweq", ExtendedGekkoMnemonic::Tweq, ParseAlg::Op2),
    PLAIN_PSEUDO("tweqi", ExtendedGekkoMnemonic::Tweqi, ParseAlg::Op2),
    PLAIN_PSEUDO("twge", ExtendedGekkoMnemonic::Twge, ParseAlg::Op2),
    PLAIN_PSEUDO("twgei", ExtendedGekkoMnemonic::Twgei, ParseAlg::Op2),
    PLAIN_PSEUDO("twgt", ExtendedGekkoMnemonic::Twgt, ParseAlg::Op2),
    PLAIN_PSEUDO("twgti", ExtendedGekkoMnemonic::Twgti, ParseAlg::Op2),
    PLAIN_PSEUDO("twnl", ExtendedGekkoMnemonic::Twnl, ParseAlg::Op2),
    PLAIN_PSEUDO("twnli", ExtendedGekkoMnemonic::Twnli, ParseAlg::Op2),
    PLAIN_PSEUDO("twne", ExtendedGekkoMnemonic::Twne, ParseAlg::Op2),
    PLAIN_PSEUDO("twnei", ExtendedGekkoMnemonic::Twnei, ParseAlg::Op2),
    PLAIN_PSEUDO("twng", ExtendedGekkoMnemonic::Twng, ParseAlg::Op2),
    PLAIN_PSEUDO("twngi", ExtendedGekkoMnemonic::Twngi, ParseAlg::Op2),
    PLAIN_PSEUDO("twllt", ExtendedGekkoMnemonic::Twllt, ParseAlg::Op2),
    PLAIN_PSEUDO("twllti", ExtendedGekkoMnemonic::Twllti, ParseAlg::Op2),
    PLAIN_PSEUDO("twlle", ExtendedGekkoMnemonic::Twlle, ParseAlg::Op2),
    PLAIN_PSEUDO("twllei", ExtendedGekkoMnemonic::Twllei, ParseAlg::Op2),
    PLAIN_PSEUDO("twlge", ExtendedGekkoMnemonic::Twlge, ParseAlg::Op2),
    PLAIN_PSEUDO("twlgei", ExtendedGekkoMnemonic::Twlgei, ParseAlg::Op2),
    PLAIN_PSEUDO("twlgt", ExtendedGekkoMnemonic::Twlgt, ParseAlg::Op2),
    PLAIN_PSEUDO("twlgti", ExtendedGekkoMnemonic::Twlgti, ParseAlg::Op2),
    PLAIN_PSEUDO("twlnl", ExtendedGekkoMnemonic::Twlnl, ParseAlg::Op2),
    PLAIN_PSEUDO("twlnli", ExtendedGekkoMnemonic::Twlnli, ParseAlg::Op2),
    PLAIN_PSEUDO("twlng", ExtendedGekkoMnemonic::Twlng, ParseAlg::Op2),
    PLAIN_PSEUDO("twlngi", ExtendedGekkoMnemonic::Twlngi, ParseAlg::Op2),
    PLAIN_PSEUDO("trap", ExtendedGekkoMnemonic::Trap, ParseAlg::None),
    PLAIN_PSEUDO("mtxer", ExtendedGekkoMnemonic::Mtxer, ParseAlg::Op1),
    PLAIN_PSEUDO("mfxer", ExtendedGekkoMnemonic::Mfxer, ParseAlg::Op1),
    PLAIN_PSEUDO("mtlr", ExtendedGekkoMnemonic::Mtlr, ParseAlg::Op1),
    PLAIN_PSEUDO("mflr", ExtendedGekkoMnemonic::Mflr, ParseAlg::Op1),
    PLAIN_PSEUDO("mtctr", ExtendedGekkoMnemonic::Mtctr, ParseAlg::Op1),
    PLAIN_PSEUDO("mfctr", ExtendedGekkoMnemonic::Mfctr, ParseAlg::Op1),
    PLAIN_PSEUDO("mtdsisr", ExtendedGekkoMnemonic::Mtdsisr, ParseAlg::Op1),
    PLAIN_PSEUDO("mfdsisr", ExtendedGekkoMnemonic::Mfdsisr, ParseAlg::Op1),
    PLAIN_PSEUDO("mtdar", ExtendedGekkoMnemonic::Mtdar, ParseAlg::Op1),
    PLAIN_PSEUDO("mfdar", ExtendedGekkoMnemonic::Mfdar, ParseAlg::Op1),
    PLAIN_PSEUDO("mtdec", ExtendedGekkoMnemonic::Mtdec, ParseAlg::Op1),
    PLAIN_PSEUDO("mfdec", ExtendedGekkoMnemonic::Mfdec, ParseAlg::Op1),
    PLAIN_PSEUDO("mtsdr1", ExtendedGekkoMnemonic::Mtsdr1, ParseAlg::Op1),
    PLAIN_PSEUDO("mfsdr1", ExtendedGekkoMnemonic::Mfsdr1, ParseAlg::Op1),
    PLAIN_PSEUDO("mtsrr0", ExtendedGekkoMnemonic::Mtsrr0, ParseAlg::Op1),
    PLAIN_PSEUDO("mfsrr0", ExtendedGekkoMnemonic::Mfsrr0, ParseAlg::Op1),
    PLAIN_PSEUDO("mtsrr1", ExtendedGekkoMnemonic::Mtsrr1, ParseAlg::Op1),
    PLAIN_PSEUDO("mfsrr1", ExtendedGekkoMnemonic::Mfsrr1, ParseAlg::Op1),
    PLAIN_PSEUDO("mtasr", ExtendedGekkoMnemonic::Mtasr, ParseAlg::Op1),
    PLAIN_PSEUDO("mfasr", ExtendedGekkoMnemonic::Mfasr, ParseAlg::Op1),
    PLAIN_PSEUDO("mtear", ExtendedGekkoMnemonic::Mtear, ParseAlg::Op1),
    PLAIN_PSEUDO("mfear", ExtendedGekkoMnemonic::Mfear, ParseAlg::Op1),
    PLAIN_PSEUDO("mttbl", ExtendedGekkoMnemonic::Mttbl, ParseAlg::Op1),
    PLAIN_PSEUDO("mftbl", ExtendedGekkoMnemonic::Mftbl, ParseAlg::Op1),
    PLAIN_PSEUDO("mttbu", ExtendedGekkoMnemonic::Mttbu, ParseAlg::Op1),
    PLAIN_PSEUDO("mftbu", ExtendedGekkoMnemonic::Mftbu, ParseAlg::Op1),
    PLAIN_PSEUDO("mtsprg", ExtendedGekkoMnemonic::Mtsprg, ParseAlg::Op2),
    PLAIN_PSEUDO("mfsprg", ExtendedGekkoMnemonic::Mfsprg, ParseAlg::Op2),
    PLAIN_PSEUDO("mtibatu", ExtendedGekkoMnemonic::Mtibatu, ParseAlg::Op2),
    PLAIN_PSEUDO("mfibatu", ExtendedGekkoMnemonic::Mfibatu, ParseAlg::Op2),
    PLAIN_PSEUDO("mtibatl", ExtendedGekkoMnemonic::Mtibatl, ParseAlg::Op2),
    PLAIN_PSEUDO("mfibatl", ExtendedGekkoMnemonic::Mfibatl, ParseAlg::Op2),
    PLAIN_PSEUDO("mtdbatu", ExtendedGekkoMnemonic::Mtdbatu, ParseAlg::Op2),
    PLAIN_PSEUDO("mfdbatu", ExtendedGekkoMnemonic::Mfdbatu, ParseAlg::Op2),
    PLAIN_PSEUDO("mtdbatl", ExtendedGekkoMnemonic::Mtdbatl, ParseAlg::Op2),
    PLAIN_PSEUDO("mfdbatl", ExtendedGekkoMnemonic::Mfdbatl, ParseAlg::Op2),
    PLAIN_PSEUDO("nop", ExtendedGekkoMnemonic::Nop, ParseAlg::None),
    PLAIN_PSEUDO("li", ExtendedGekkoMnemonic::Li, ParseAlg::Op2),
    PLAIN_PSEUDO("lis", ExtendedGekkoMnemonic::Lis, ParseAlg::Op2),
    PLAIN_PSEUDO("la", ExtendedGekkoMnemonic::La, ParseAlg::Op1Off1),
    RC_PSEUDO("mr", ExtendedGekkoMnemonic::Mr, ParseAlg::Op2),
    RC_PSEUDO("not", ExtendedGekkoMnemonic::Not, ParseAlg::Op2),
    PLAIN_PSEUDO("mtcr", ExtendedGekkoMnemonic::Mtcr, ParseAlg::Op1),
    PLAIN_PSEUDO("mfspr", ExtendedGekkoMnemonic::Mfspr, ParseAlg::Op2),
    PLAIN_PSEUDO("mftb", ExtendedGekkoMnemonic::Mftb, ParseAlg::Op2),
    PLAIN_PSEUDO("mtspr", ExtendedGekkoMnemonic::Mtspr, ParseAlg::Op2),
};

#undef MNEMONIC
#undef PLAIN_MNEMONIC
#undef RC_MNEMONIC
#undef OERC_MNEMONIC
#undef LK_MNEMONIC
#undef AALK_MNEMONIC
#undef PSEUDO
#undef PLAIN_PSEUDO
#undef RC_PSEUDO
#undef OERC_PSEUDO
#undef LK_PSEUDO
#undef LKAA_PSEUDO
#undef LKPRED_PSEUDO
#undef LKAAPRED_PSEUDO

//////////////////////
// ASSEMBLER TABLES //
//////////////////////
#define EMIT_MNEMONIC_ENTRY(opcode_val, extra_bits, ...)                                           \
  MnemonicDesc                                                                                     \
  {                                                                                                \
    InsertOpcode(opcode_val) | (extra_bits),                                                       \
        static_cast<u32>(std::initializer_list<OperandDesc>{__VA_ARGS__}.size()),                  \
    {                                                                                              \
      __VA_ARGS__                                                                                  \
    }                                                                                              \
  }
#define MNEMONIC(opcode_val, extra_bits, ...)                                                      \
  EMIT_MNEMONIC_ENTRY(opcode_val, extra_bits, __VA_ARGS__), INVALID_MNEMONIC, INVALID_MNEMONIC,    \
      INVALID_MNEMONIC
#define BASIC_MNEMONIC(opcode_val, ...) MNEMONIC(opcode_val, 0, __VA_ARGS__)
#define RC_MNEMONIC(opcode_val, extra_bits, ...)                                                   \
  EMIT_MNEMONIC_ENTRY(opcode_val, extra_bits, __VA_ARGS__),                                        \
      EMIT_MNEMONIC_ENTRY(opcode_val, ((extra_bits) | InsertVal(1, 31, 31)), __VA_ARGS__),         \
      INVALID_MNEMONIC, INVALID_MNEMONIC
#define OERC_MNEMONIC(opcode_val, extra_bits, ...)                                                 \
  EMIT_MNEMONIC_ENTRY(opcode_val, extra_bits, __VA_ARGS__),                                        \
      EMIT_MNEMONIC_ENTRY(opcode_val, ((extra_bits) | InsertVal(1, 31, 31)), __VA_ARGS__),         \
      EMIT_MNEMONIC_ENTRY(opcode_val, ((extra_bits) | InsertVal(1, 21, 21)), __VA_ARGS__),         \
      EMIT_MNEMONIC_ENTRY(                                                                         \
          opcode_val, ((extra_bits) | InsertVal(1, 31, 31) | InsertVal(1, 21, 21)), __VA_ARGS__)
#define LK_MNEMONIC(opcode_val, extra_bits, ...)                                                   \
  EMIT_MNEMONIC_ENTRY(opcode_val, extra_bits, __VA_ARGS__),                                        \
      EMIT_MNEMONIC_ENTRY(opcode_val, ((extra_bits) | InsertVal(1, 31, 31)), __VA_ARGS__),         \
      INVALID_MNEMONIC, INVALID_MNEMONIC
#define AALK_MNEMONIC(opcode_val, extra_bits, ...)                                                 \
  EMIT_MNEMONIC_ENTRY(opcode_val, extra_bits, __VA_ARGS__),                                        \
      EMIT_MNEMONIC_ENTRY(opcode_val, ((extra_bits) | InsertVal(0b01, 30, 31)), __VA_ARGS__),      \
      EMIT_MNEMONIC_ENTRY(opcode_val, ((extra_bits) | InsertVal(0b10, 30, 31)), __VA_ARGS__),      \
      EMIT_MNEMONIC_ENTRY(opcode_val, ((extra_bits) | InsertVal(0b11, 30, 31)), __VA_ARGS__)

// Defines all basic mnemonics that Broadway/Gekko supports
extern const std::array<MnemonicDesc, NUM_MNEMONICS* VARIANT_PERMUTATIONS> mnemonics = {
    // A-2
    OERC_MNEMONIC(31, InsertVal(266, 22, 30), _D, _A, _B),  // add
    OERC_MNEMONIC(31, InsertVal(10, 22, 30), _D, _A, _B),   // addc
    OERC_MNEMONIC(31, InsertVal(138, 22, 30), _D, _A, _B),  // adde
    BASIC_MNEMONIC(14, _D, _A, _SIMM),                      // addi
    BASIC_MNEMONIC(12, _D, _A, _SIMM),                      // addic
    BASIC_MNEMONIC(13, _D, _A, _SIMM),                      // addic.
    BASIC_MNEMONIC(15, _D, _A, _SIMM),                      // addis
    OERC_MNEMONIC(31, InsertVal(234, 22, 30), _D, _A),      // addme
    OERC_MNEMONIC(31, InsertVal(202, 22, 30), _D, _A),      // addze
    OERC_MNEMONIC(31, InsertVal(491, 22, 30), _D, _A, _B),  // divw
    OERC_MNEMONIC(31, InsertVal(459, 22, 30), _D, _A, _B),  // divwu
    RC_MNEMONIC(31, InsertVal(75, 22, 30), _D, _A, _B),     // mulhw
    RC_MNEMONIC(31, InsertVal(11, 22, 30), _D, _A, _B),     // mulhwu
    BASIC_MNEMONIC(7, _D, _A, _SIMM),                       // mulli
    OERC_MNEMONIC(31, InsertVal(235, 22, 30), _D, _A, _B),  // mullw
    OERC_MNEMONIC(31, InsertVal(104, 22, 30), _D, _A),      // neg
    OERC_MNEMONIC(31, InsertVal(40, 22, 30), _D, _A, _B),   // subf
    OERC_MNEMONIC(31, InsertVal(8, 22, 30), _D, _A, _B),    // subfc
    OERC_MNEMONIC(31, InsertVal(136, 22, 30), _D, _A, _B),  // subfe
    BASIC_MNEMONIC(8, _D, _A, _SIMM),                       // subfic
    OERC_MNEMONIC(31, InsertVal(232, 22, 30), _D, _A),      // subfme
    OERC_MNEMONIC(31, InsertVal(200, 22, 30), _D, _A),      // subfze

    // A-3
    MNEMONIC(31, InsertVal(0, 21, 30), _Crfd, _L, _A, _B),   // cmp
    BASIC_MNEMONIC(11, _Crfd, _L, _A, _SIMM),                // cmpi
    MNEMONIC(31, InsertVal(32, 21, 30), _Crfd, _L, _A, _B),  // cmpl
    BASIC_MNEMONIC(10, _Crfd, _L, _A, _UIMM),                // cmpli

    // A-4
    RC_MNEMONIC(31, InsertVal(28, 21, 30), _A, _S, _B),   // and
    RC_MNEMONIC(31, InsertVal(60, 21, 30), _A, _S, _B),   // andc
    BASIC_MNEMONIC(28, _A, _S, _UIMM),                    // andi.
    BASIC_MNEMONIC(29, _A, _S, _UIMM),                    // andis.
    RC_MNEMONIC(31, InsertVal(26, 21, 30), _A, _S),       // cntlzw
    RC_MNEMONIC(31, InsertVal(284, 21, 30), _A, _S, _B),  // eqv
    RC_MNEMONIC(31, InsertVal(954, 21, 30), _A, _S),      // extsb
    RC_MNEMONIC(31, InsertVal(922, 21, 30), _A, _S),      // extsh
    RC_MNEMONIC(31, InsertVal(476, 21, 30), _A, _S, _B),  // nand
    RC_MNEMONIC(31, InsertVal(124, 21, 30), _A, _S, _B),  // nor
    RC_MNEMONIC(31, InsertVal(444, 21, 30), _A, _S, _B),  // or
    RC_MNEMONIC(31, InsertVal(412, 21, 30), _A, _S, _B),  // orc
    BASIC_MNEMONIC(24, _A, _S, _UIMM),                    // ori
    BASIC_MNEMONIC(25, _A, _S, _UIMM),                    // oris
    RC_MNEMONIC(31, InsertVal(316, 21, 30), _A, _S, _B),  // xor
    BASIC_MNEMONIC(26, _A, _S, _UIMM),                    // xori
    BASIC_MNEMONIC(27, _A, _S, _UIMM),                    // xoris

    // A-5
    RC_MNEMONIC(20, 0, _A, _S, _SH, _MB, _ME),  // rlwimi
    RC_MNEMONIC(21, 0, _A, _S, _SH, _MB, _ME),  // rlwinm
    RC_MNEMONIC(23, 0, _A, _S, _B, _MB, _ME),   // rlwnm

    // A-6
    RC_MNEMONIC(31, InsertVal(24, 21, 30), _A, _S, _B),    // slw
    RC_MNEMONIC(31, InsertVal(792, 21, 30), _A, _S, _B),   // sraw
    RC_MNEMONIC(31, InsertVal(824, 21, 30), _A, _S, _SH),  // srawi
    RC_MNEMONIC(31, InsertVal(536, 21, 30), _A, _S, _B),   // srw

    // A-7
    RC_MNEMONIC(63, InsertVal(21, 26, 30), _D, _A, _B),      // fadd
    RC_MNEMONIC(59, InsertVal(21, 26, 30), _D, _A, _B),      // fadds
    RC_MNEMONIC(63, InsertVal(18, 26, 30), _D, _A, _B),      // fdiv
    RC_MNEMONIC(59, InsertVal(18, 26, 30), _D, _A, _B),      // fdivs
    RC_MNEMONIC(63, InsertVal(25, 26, 30), _D, _A, _C),      // fmul
    RC_MNEMONIC(59, InsertVal(25, 26, 30), _D, _A, _C),      // fmuls
    RC_MNEMONIC(59, InsertVal(24, 26, 30), _D, _B),          // fres
    RC_MNEMONIC(63, InsertVal(26, 26, 30), _D, _B),          // frsqrte
    RC_MNEMONIC(63, InsertVal(20, 26, 30), _D, _A, _B),      // fsub
    RC_MNEMONIC(59, InsertVal(20, 26, 30), _D, _A, _B),      // fsubs
    RC_MNEMONIC(63, InsertVal(23, 26, 30), _D, _A, _C, _B),  // fsel

    // A-8
    RC_MNEMONIC(63, InsertVal(29, 26, 30), _D, _A, _C, _B),  // fmadd
    RC_MNEMONIC(59, InsertVal(29, 26, 30), _D, _A, _C, _B),  // fmadds
    RC_MNEMONIC(63, InsertVal(28, 26, 30), _D, _A, _C, _B),  // fmsub
    RC_MNEMONIC(59, InsertVal(28, 26, 30), _D, _A, _C, _B),  // fmsubs
    RC_MNEMONIC(63, InsertVal(31, 26, 30), _D, _A, _C, _B),  // fnmadd
    RC_MNEMONIC(59, InsertVal(31, 26, 30), _D, _A, _C, _B),  // fnmadds
    RC_MNEMONIC(63, InsertVal(30, 26, 30), _D, _A, _C, _B),  // fnmsub
    RC_MNEMONIC(59, InsertVal(30, 26, 30), _D, _A, _C, _B),  // fnmsubs

    // A-9
    RC_MNEMONIC(63, InsertVal(14, 21, 30), _D, _B),  // fctiw
    RC_MNEMONIC(63, InsertVal(15, 21, 30), _D, _B),  // fctiwz
    RC_MNEMONIC(63, InsertVal(12, 21, 30), _D, _B),  // frsp

    // A-10
    MNEMONIC(63, InsertVal(32, 21, 30), _Crfd, _A, _B),  // fcmpo
    MNEMONIC(63, InsertVal(0, 21, 30), _Crfd, _A, _B),   // fcmpu

    // A-11
    MNEMONIC(63, InsertVal(64, 21, 30), _Crfd, _Crfs),     // mcrfs
    RC_MNEMONIC(63, InsertVal(583, 21, 30), _D),           // mffs
    RC_MNEMONIC(63, InsertVal(70, 21, 30), _Crbd),         // mtfsb0
    RC_MNEMONIC(63, InsertVal(38, 21, 30), _Crbd),         // mtfsb1
    RC_MNEMONIC(63, InsertVal(711, 21, 30), _FM, _B),      // mtfsf
    RC_MNEMONIC(63, InsertVal(134, 21, 30), _Crfd, _IMM),  // mtfsfi

    // A-12
    BASIC_MNEMONIC(34, _D, _Offd, _A),                 // lbz
    BASIC_MNEMONIC(35, _D, _Offd, _A),                 // lbzu
    MNEMONIC(31, InsertVal(119, 21, 30), _D, _A, _B),  // lbzux
    MNEMONIC(31, InsertVal(87, 21, 30), _D, _A, _B),   // lbzx
    BASIC_MNEMONIC(42, _D, _Offd, _A),                 // lha
    BASIC_MNEMONIC(43, _D, _Offd, _A),                 // lhau
    MNEMONIC(31, InsertVal(375, 21, 30), _D, _A, _B),  // lhaux
    MNEMONIC(31, InsertVal(343, 21, 30), _D, _A, _B),  // lhax
    BASIC_MNEMONIC(40, _D, _Offd, _A),                 // lhz
    BASIC_MNEMONIC(41, _D, _Offd, _A),                 // lhzu
    MNEMONIC(31, InsertVal(311, 21, 30), _D, _A, _B),  // lhzux
    MNEMONIC(31, InsertVal(279, 21, 30), _D, _A, _B),  // lhzx
    BASIC_MNEMONIC(32, _D, _Offd, _A),                 // lwz
    BASIC_MNEMONIC(33, _D, _Offd, _A),                 // lwzu
    MNEMONIC(31, InsertVal(55, 21, 30), _D, _A, _B),   // lwzux
    MNEMONIC(31, InsertVal(23, 21, 30), _D, _A, _B),   // lwzx

    // A-13
    BASIC_MNEMONIC(38, _S, _Offd, _A),                 // stb
    BASIC_MNEMONIC(39, _S, _Offd, _A),                 // stbu
    MNEMONIC(31, InsertVal(247, 21, 30), _S, _A, _B),  // stbux
    MNEMONIC(31, InsertVal(215, 21, 30), _S, _A, _B),  // stbx
    BASIC_MNEMONIC(44, _S, _Offd, _A),                 // sth
    BASIC_MNEMONIC(45, _S, _Offd, _A),                 // sthu
    MNEMONIC(31, InsertVal(439, 21, 30), _S, _A, _B),  // sthux
    MNEMONIC(31, InsertVal(407, 21, 30), _S, _A, _B),  // sthx
    BASIC_MNEMONIC(36, _S, _Offd, _A),                 // stw
    BASIC_MNEMONIC(37, _S, _Offd, _A),                 // stwu
    MNEMONIC(31, InsertVal(183, 21, 30), _S, _A, _B),  // stwux
    MNEMONIC(31, InsertVal(151, 21, 30), _S, _A, _B),  // stwx

    // A-14
    MNEMONIC(31, InsertVal(790, 21, 30), _D, _A, _B),  // lhbrx
    MNEMONIC(31, InsertVal(534, 21, 30), _D, _A, _B),  // lwbrx
    MNEMONIC(31, InsertVal(918, 21, 30), _S, _A, _B),  // sthbrx
    MNEMONIC(31, InsertVal(662, 21, 30), _S, _A, _B),  // stwbrx

    // A-15
    BASIC_MNEMONIC(46, _D, _Offd, _A),  // lmw
    BASIC_MNEMONIC(47, _S, _Offd, _A),  // stmw

    // A-16
    MNEMONIC(31, InsertVal(597, 21, 30), _D, _A, _NB),  // lswi
    MNEMONIC(31, InsertVal(533, 21, 30), _D, _A, _B),   // lswx
    MNEMONIC(31, InsertVal(725, 21, 30), _S, _A, _NB),  // stswi
    MNEMONIC(31, InsertVal(661, 21, 30), _S, _A, _B),   // stswx

    // A-17
    MNEMONIC(31, InsertVal(854, 21, 30)),                                     // eieio
    MNEMONIC(19, InsertVal(150, 21, 30)),                                     // isync
    MNEMONIC(31, InsertVal(20, 21, 30), _D, _A, _B),                          // lwarx
    MNEMONIC(31, InsertVal(150, 21, 30) | InsertVal(1, 31, 31), _S, _A, _B),  // stwcx.
    MNEMONIC(31, InsertVal(598, 21, 30)),                                     // sync

    // A-18
    BASIC_MNEMONIC(50, _D, _Offd, _A),                 // lfd
    BASIC_MNEMONIC(51, _D, _Offd, _A),                 // lfdu
    MNEMONIC(31, InsertVal(631, 21, 30), _D, _A, _B),  // lfdux
    MNEMONIC(31, InsertVal(599, 21, 30), _D, _A, _B),  // lfdx
    BASIC_MNEMONIC(48, _D, _Offd, _A),                 // lfs
    BASIC_MNEMONIC(49, _D, _Offd, _A),                 // lfsu
    MNEMONIC(31, InsertVal(567, 21, 30), _D, _A, _B),  // lfsux
    MNEMONIC(31, InsertVal(535, 21, 30), _D, _A, _B),  // lfsx

    // A-19
    BASIC_MNEMONIC(54, _S, _Offd, _A),                 // stfd
    BASIC_MNEMONIC(55, _S, _Offd, _A),                 // stfdu
    MNEMONIC(31, InsertVal(759, 21, 30), _S, _A, _B),  // stfdux
    MNEMONIC(31, InsertVal(727, 21, 30), _S, _A, _B),  // stfdx
    MNEMONIC(31, InsertVal(983, 21, 30), _S, _A, _B),  // stfiwx
    BASIC_MNEMONIC(52, _S, _Offd, _A),                 // stfs
    BASIC_MNEMONIC(53, _S, _Offd, _A),                 // stfsu
    MNEMONIC(31, InsertVal(695, 21, 30), _S, _A, _B),  // stfsux
    MNEMONIC(31, InsertVal(663, 21, 30), _S, _A, _B),  // stfsx

    // A-20
    RC_MNEMONIC(63, InsertVal(264, 21, 30), _D, _B),  // fabs
    RC_MNEMONIC(63, InsertVal(72, 21, 30), _D, _B),   // fmr
    RC_MNEMONIC(63, InsertVal(136, 21, 30), _D, _B),  // fnabs
    RC_MNEMONIC(63, InsertVal(40, 21, 30), _D, _B),   // fneg

    // A-21
    AALK_MNEMONIC(18, 0, _LI),                          // b
    AALK_MNEMONIC(16, 0, _BO, _BI, _BD),                // bc
    LK_MNEMONIC(19, InsertVal(528, 21, 30), _BO, _BI),  // bcctr
    LK_MNEMONIC(19, InsertVal(16, 21, 30), _BO, _BI),   // bclr

    // A-22
    MNEMONIC(19, InsertVal(257, 21, 30), _Crbd, _Crba, _Crbb),  // crand
    MNEMONIC(19, InsertVal(129, 21, 30), _Crbd, _Crba, _Crbb),  // crandc
    MNEMONIC(19, InsertVal(289, 21, 30), _Crbd, _Crba, _Crbb),  // creqv
    MNEMONIC(19, InsertVal(225, 21, 30), _Crbd, _Crba, _Crbb),  // crnand
    MNEMONIC(19, InsertVal(33, 21, 30), _Crbd, _Crba, _Crbb),   // crnor
    MNEMONIC(19, InsertVal(449, 21, 30), _Crbd, _Crba, _Crbb),  // cror
    MNEMONIC(19, InsertVal(417, 21, 30), _Crbd, _Crba, _Crbb),  // crorc
    MNEMONIC(19, InsertVal(193, 21, 30), _Crbd, _Crba, _Crbb),  // crxor
    MNEMONIC(19, InsertVal(0, 21, 30), _Crfd, _Crfs),           // mcrf

    // A-23
    MNEMONIC(19, InsertVal(50, 21, 30)),  // rfi
    MNEMONIC(17, InsertVal(1, 30, 30)),   // sc

    // A-24
    MNEMONIC(31, InsertVal(4, 21, 30), _TO, _A, _B),  // tw
    BASIC_MNEMONIC(3, _TO, _A, _SIMM),                // twi

    // A-25
    MNEMONIC(31, InsertVal(512, 21, 30), _Crfd),     // mcrxr
    MNEMONIC(31, InsertVal(19, 21, 30), _D),         // mfcr
    MNEMONIC(31, InsertVal(83, 21, 30), _D),         // mfmsr
    MNEMONIC(31, InsertVal(339, 21, 30), _D, _SPR),  // mfspr
    MNEMONIC(31, InsertVal(371, 21, 30), _D, _TPR),  // mftb
    MNEMONIC(31, InsertVal(144, 21, 30), _CRM, _S),  // mtcrf
    MNEMONIC(31, InsertVal(146, 21, 30), _S),        // mtmsr
    MNEMONIC(31, InsertVal(467, 21, 30), _SPR, _D),  // mtspr

    // A-26
    MNEMONIC(31, InsertVal(86, 21, 30), _A, _B),    // dcbf
    MNEMONIC(31, InsertVal(470, 21, 30), _A, _B),   // dcbi
    MNEMONIC(31, InsertVal(54, 21, 30), _A, _B),    // dcbst
    MNEMONIC(31, InsertVal(278, 21, 30), _A, _B),   // dcbt
    MNEMONIC(31, InsertVal(246, 21, 30), _A, _B),   // dcbtst
    MNEMONIC(31, InsertVal(1014, 21, 30), _A, _B),  // dcbz
    MNEMONIC(31, InsertVal(982, 21, 30), _A, _B),   // icbi

    // A-27
    MNEMONIC(31, InsertVal(595, 21, 30), _D, _SR),  // mfsr
    MNEMONIC(31, InsertVal(659, 21, 30), _D, _B),   // mfsrin
    MNEMONIC(31, InsertVal(210, 21, 30), _SR, _S),  // mtsr
    MNEMONIC(31, InsertVal(242, 21, 30), _S, _B),   // mtsrin

    // A-28
    MNEMONIC(31, InsertVal(306, 21, 30), _B),  // tlbie
    MNEMONIC(31, InsertVal(566, 21, 30)),      // tlbsync

    // A-29
    MNEMONIC(31, InsertVal(310, 21, 30), _D, _A, _B),  // eciwx
    MNEMONIC(31, InsertVal(438, 21, 30), _S, _A, _B),  // ecowx

    // A-30
    MNEMONIC(4, InsertVal(6, 25, 30), _D, _A, _B, _W2, _I2),   // psq_lx
    MNEMONIC(4, InsertVal(7, 25, 30), _S, _A, _B, _W2, _I2),   // psq_stx
    MNEMONIC(4, InsertVal(38, 25, 30), _D, _A, _B, _W2, _I2),  // psq_lux
    MNEMONIC(4, InsertVal(39, 25, 30), _S, _A, _B, _W2, _I2),  // psq_stux
    BASIC_MNEMONIC(56, _D, _OffdPs, _A, _W1, _I1),             // psq_l
    BASIC_MNEMONIC(57, _D, _OffdPs, _A, _W1, _I1),             // psq_lu
    BASIC_MNEMONIC(60, _S, _OffdPs, _A, _W1, _I1),             // psq_st
    BASIC_MNEMONIC(61, _S, _OffdPs, _A, _W1, _I1),             // psq_stu

    // A-31
    RC_MNEMONIC(4, InsertVal(18, 26, 30), _D, _A, _B),      // ps_div
    RC_MNEMONIC(4, InsertVal(20, 26, 30), _D, _A, _B),      // ps_sub
    RC_MNEMONIC(4, InsertVal(21, 26, 30), _D, _A, _B),      // ps_add
    RC_MNEMONIC(4, InsertVal(23, 26, 30), _D, _A, _C, _B),  // ps_sel
    RC_MNEMONIC(4, InsertVal(24, 26, 30), _D, _B),          // ps_res
    RC_MNEMONIC(4, InsertVal(25, 26, 30), _D, _A, _C),      // ps_mul
    RC_MNEMONIC(4, InsertVal(26, 26, 30), _D, _B),          // ps_rsqrte
    RC_MNEMONIC(4, InsertVal(28, 26, 30), _D, _A, _C, _B),  // ps_msub
    RC_MNEMONIC(4, InsertVal(29, 26, 30), _D, _A, _C, _B),  // ps_madd
    RC_MNEMONIC(4, InsertVal(30, 26, 30), _D, _A, _C, _B),  // ps_nmsub
    RC_MNEMONIC(4, InsertVal(31, 26, 30), _D, _A, _C, _B),  // ps_nmadd
    RC_MNEMONIC(4, InsertVal(40, 21, 30), _D, _B),          // ps_neg
    RC_MNEMONIC(4, InsertVal(72, 21, 30), _D, _B),          // ps_mr
    RC_MNEMONIC(4, InsertVal(136, 21, 30), _D, _B),         // ps_nabs
    RC_MNEMONIC(4, InsertVal(264, 21, 30), _D, _B),         // ps_abs

    // A-32
    RC_MNEMONIC(4, InsertVal(10, 26, 30), _D, _A, _C, _B),  // ps_sum0
    RC_MNEMONIC(4, InsertVal(11, 26, 30), _D, _A, _C, _B),  // ps_sum1
    RC_MNEMONIC(4, InsertVal(12, 26, 30), _D, _A, _C),      // ps_muls0
    RC_MNEMONIC(4, InsertVal(13, 26, 30), _D, _A, _C),      // ps_muls1
    RC_MNEMONIC(4, InsertVal(14, 26, 30), _D, _A, _C, _B),  // ps_madds0
    RC_MNEMONIC(4, InsertVal(15, 26, 30), _D, _A, _C, _B),  // ps_madds1
    MNEMONIC(4, InsertVal(0, 21, 30), _Crfd, _A, _B),       // ps_cmpu0
    MNEMONIC(4, InsertVal(32, 21, 30), _Crfd, _A, _B),      // ps_cmpo0
    MNEMONIC(4, InsertVal(64, 21, 30), _Crfd, _A, _B),      // ps_cmpu1
    MNEMONIC(4, InsertVal(96, 21, 30), _Crfd, _A, _B),      // ps_cmpo1
    RC_MNEMONIC(4, InsertVal(528, 21, 30), _D, _A, _B),     // ps_merge00
    RC_MNEMONIC(4, InsertVal(560, 21, 30), _D, _A, _B),     // ps_merge01
    RC_MNEMONIC(4, InsertVal(592, 21, 30), _D, _A, _B),     // ps_merge10
    RC_MNEMONIC(4, InsertVal(624, 21, 30), _D, _A, _B),     // ps_merge11
    MNEMONIC(4, InsertVal(1014, 21, 30), _A, _B),           // dcbz_l
};

namespace
{
// Reused operand translators for extended mnemonics
void NegateSIMM(OperandList& operands)
{
  operands[2] = static_cast<u32>(-static_cast<s32>(operands[2]));
}

void SwapOps1And2(OperandList& operands)
{
  std::swap(operands[1], operands[2]);
}

void SetCompareWordMode(OperandList& operands)
{
  if (operands.count == 2)
  {
    operands.Insert(0, 0);
  }
  operands.Insert(1, 0);
}

template <u32 BO, u32 BI>
void FillBOBI(OperandList& operands)
{
  operands.Insert(0, BO);
  operands.Insert(1, BI);
}

template <size_t Idx>
void BitswapIdx(OperandList& operands)
{
  operands[Idx] = SprBitswap(operands[Idx]);
}

template <u32 BO, u32 Cond, u32 ParamCount>
void FillBOBICond(OperandList& operands)
{
  if (operands.count < ParamCount)
  {
    operands.Insert(0, 0);
  }
  operands[0] = (operands[0] << 2) | Cond;
  operands.Insert(0, BO);
}

template <u32 BO>
void FillBO(OperandList& operands)
{
  operands.Insert(0, BO);
}

template <u32 TO>
void TrapSetTO(OperandList& operands)
{
  operands.Insert(0, TO);
}

template <u32 SPRG>
void FillMtspr(OperandList& operands)
{
  operands.Insert(0, SPRG);
}

template <u32 SPRG>
void FillMfspr(OperandList& operands)
{
  operands.Insert(1, SPRG);
}

template <u32 SPRG>
void FillMtsprBatAndBitswap(OperandList& operands)
{
  operands[0] = SprBitswap(2 * operands[0] + SPRG);
}

template <u32 SPRG>
void FillMfsprBatAndBitswap(OperandList& operands)
{
  operands[1] = SprBitswap(2 * operands[1] + SPRG);
}
}  // namespace

#define PSEUDO(base, variant_bits, cb)                                                             \
  ExtendedMnemonicDesc { static_cast<size_t>(base) * VARIANT_PERMUTATIONS + variant_bits, cb }
#define PLAIN_PSEUDO(base, cb)                                                                     \
  PSEUDO(base, PLAIN_MNEMONIC, cb), INVALID_EXT_MNEMONIC, INVALID_EXT_MNEMONIC, INVALID_EXT_MNEMONIC
#define RC_PSEUDO(base, cb)                                                                        \
  PSEUDO(base, PLAIN_MNEMONIC, cb), PSEUDO(base, RECORD_BIT, cb), INVALID_EXT_MNEMONIC,            \
      INVALID_EXT_MNEMONIC
#define OERC_PSEUDO(base, cb)                                                                      \
  PSEUDO(base, PLAIN_MNEMONIC, cb), PSEUDO(base, RECORD_BIT, cb),                                  \
      PSEUDO(base, OVERFLOW_EXCEPTION, cb), PSEUDO(base, (RECORD_BIT | OVERFLOW_EXCEPTION), cb)
#define LK_PSEUDO(base, cb)                                                                        \
  PSEUDO(base, PLAIN_MNEMONIC, cb), PSEUDO(base, LINK_BIT, cb), INVALID_EXT_MNEMONIC,              \
      INVALID_EXT_MNEMONIC
#define LKAA_PSEUDO(base, cb)                                                                      \
  PSEUDO(base, PLAIN_MNEMONIC, cb), PSEUDO(base, LINK_BIT, cb),                                    \
      PSEUDO(base, ABSOLUTE_ADDRESS_BIT, cb), PSEUDO(base, (LINK_BIT | ABSOLUTE_ADDRESS_BIT), cb)

extern const std::array<ExtendedMnemonicDesc, NUM_EXT_MNEMONICS* VARIANT_PERMUTATIONS>
    extended_mnemonics = {
        // E.2.1
        PLAIN_PSEUDO(GekkoMnemonic::Addi, NegateSIMM),      // subi
        PLAIN_PSEUDO(GekkoMnemonic::Addis, NegateSIMM),     // subis
        PLAIN_PSEUDO(GekkoMnemonic::Addic, NegateSIMM),     // subic
        PLAIN_PSEUDO(GekkoMnemonic::AddicDot, NegateSIMM),  // subic.

        // E.2.2
        OERC_PSEUDO(GekkoMnemonic::Subf, SwapOps1And2),   // sub
        OERC_PSEUDO(GekkoMnemonic::Subfc, SwapOps1And2),  // subc

        // E.3.2
        PLAIN_PSEUDO(GekkoMnemonic::Cmpi, SetCompareWordMode),   // cmpwi
        PLAIN_PSEUDO(GekkoMnemonic::Cmp, SetCompareWordMode),    // cmpw
        PLAIN_PSEUDO(GekkoMnemonic::Cmpli, SetCompareWordMode),  // cmplwi
        PLAIN_PSEUDO(GekkoMnemonic::Cmpl, SetCompareWordMode),   // cmplw

        // E.4.2
        RC_PSEUDO(GekkoMnemonic::Rlwinm, ([](OperandList& operands) {
                    const u32 n = operands[2], b = operands[3];
                    operands[2] = b;
                    operands[3] = 0;
                    operands.Insert(4, n - 1);
                  })),  // extlwi
        RC_PSEUDO(GekkoMnemonic::Rlwinm, ([](OperandList& operands) {
                    const u32 n = operands[2], b = operands[3];
                    operands[2] = b + n;
                    operands[3] = 32 - n;
                    operands.Insert(4, 31);
                  })),  // extrwi
        RC_PSEUDO(GekkoMnemonic::Rlwimi, ([](OperandList& operands) {
                    const u32 n = operands[2], b = operands[3];
                    operands[2] = 32 - b;
                    operands[3] = b;
                    operands.Insert(4, b + n - 1);
                  })),  // inslwi
        RC_PSEUDO(GekkoMnemonic::Rlwimi, ([](OperandList& operands) {
                    const u32 n = operands[2], b = operands[3];
                    operands[2] = 32 - (b + n);
                    operands[3] = b;
                    operands.Insert(4, b + n - 1);
                  })),  // insrwi
        RC_PSEUDO(GekkoMnemonic::Rlwinm, ([](OperandList& operands) {
                    operands.Insert(3, 0);
                    operands.Insert(4, 31);
                  })),  // rotlwi
        RC_PSEUDO(GekkoMnemonic::Rlwinm, ([](OperandList& operands) {
                    const u32 n = operands[2];
                    operands[2] = 32 - n;
                    operands.Insert(3, 0);
                    operands.Insert(4, 31);
                  })),  // rotrwi
        RC_PSEUDO(GekkoMnemonic::Rlwnm, ([](OperandList& operands) {
                    operands.Insert(3, 0);
                    operands.Insert(4, 31);
                  })),  // rotlw
        RC_PSEUDO(GekkoMnemonic::Rlwinm, ([](OperandList& operands) {
                    const u32 n = operands[2];
                    operands.Insert(3, 0);
                    operands.Insert(4, 31 - n);
                  })),  // slwi
        RC_PSEUDO(GekkoMnemonic::Rlwinm, ([](OperandList& operands) {
                    const u32 n = operands[2];
                    operands[2] = 32 - n;
                    operands.Insert(3, n);
                    operands.Insert(4, 31);
                  })),  // srwi
        RC_PSEUDO(GekkoMnemonic::Rlwinm, ([](OperandList& operands) {
                    const u32 n = operands[2];
                    operands[2] = 0;
                    operands.Insert(3, n);
                    operands.Insert(4, 31);
                  })),  // clrlwi
        RC_PSEUDO(GekkoMnemonic::Rlwinm, ([](OperandList& operands) {
                    const u32 n = operands[2];
                    operands[2] = 0;
                    operands.Insert(3, 0);
                    operands.Insert(4, 31 - n);
                  })),  // clrrwi
        RC_PSEUDO(GekkoMnemonic::Rlwinm, ([](OperandList& operands) {
                    const u32 b = operands[2], n = operands[3];
                    operands[2] = n;
                    operands[3] = b - n;
                    operands.Insert(4, 31 - n);
                  })),  // clrlslwi

        // E.5.2
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<12>)),       // bt
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<4>)),        // bf
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBI<16, 0>)),  // bdnz
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<8>)),        // bdnzt
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<0>)),        // bdnzf
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBI<18, 0>)),  // bdz
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<10>)),       // bdzt
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<2>)),        // bdzf
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<13>)),       // bt+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<5>)),        // bf+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBI<17, 0>)),  // bdnz+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<9>)),        // bdnzt+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<1>)),        // bdnzf+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBI<19, 0>)),  // bdz+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<11>)),       // bdzt+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBO<3>)),        // bdzf+

        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBI<20, 0>)),  // blr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<12>)),       // btlr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<4>)),        // bflr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBI<16, 0>)),  // bdnzlr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<8>)),        // bdnztlr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<0>)),        // bdnzflr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBI<18, 0>)),  // bdzlr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<10>)),       // bdztlr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<2>)),        // bdzflr

        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<13>)),       // btlr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<5>)),        // bflr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBI<17, 0>)),  // bdnzlr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<9>)),        // bdnztlr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<1>)),        // bdnzflr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBI<19, 0>)),  // bdzlr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<11>)),       // bdztlr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBO<3>)),        // bdzflr+

        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBI<20, 0>)),  // bctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBO<12>)),       // btctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBO<4>)),        // bfctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBO<13>)),       // btctr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBO<5>)),        // bfctr+

        // E.5.3
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<12, 0, 2>)),  // blt
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<4, 1, 2>)),   // ble
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<12, 2, 2>)),  // beq
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<4, 0, 2>)),   // bge
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<12, 1, 2>)),  // bgt
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<4, 0, 2>)),   // bnl
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<4, 2, 2>)),   // bne
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<4, 1, 2>)),   // bng
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<12, 3, 2>)),  // bso
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<4, 3, 2>)),   // bns
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<12, 3, 2>)),  // bun
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<4, 3, 2>)),   // bnu

        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<13, 0, 2>)),  // blt+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<5, 1, 2>)),   // ble+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<13, 2, 2>)),  // beq+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<5, 0, 2>)),   // bge+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<13, 1, 2>)),  // bgt+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<5, 0, 2>)),   // bnl+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<5, 2, 2>)),   // bne+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<5, 1, 2>)),   // bng+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<13, 3, 2>)),  // bso+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<5, 3, 2>)),   // bns+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<13, 3, 2>)),  // bun+
        LKAA_PSEUDO(GekkoMnemonic::Bc, (FillBOBICond<5, 3, 2>)),   // bnu+

        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<12, 0, 1>)),  // bltlr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<4, 1, 1>)),   // blelr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<12, 2, 1>)),  // beqlr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<4, 0, 1>)),   // bgelr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<12, 1, 1>)),  // bgtlr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<4, 0, 1>)),   // bnllr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<4, 2, 1>)),   // bnelr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<4, 1, 1>)),   // bnglr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<12, 3, 1>)),  // bsolr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<4, 3, 1>)),   // bnslr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<12, 3, 1>)),  // bunlr
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<4, 3, 1>)),   // bnulr

        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<13, 0, 1>)),  // bltlr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<5, 1, 1>)),   // blelr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<13, 2, 1>)),  // beqlr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<5, 0, 1>)),   // bgelr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<13, 1, 1>)),  // bgtlr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<5, 0, 1>)),   // bnllr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<5, 2, 1>)),   // bnelr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<5, 1, 1>)),   // bnglr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<13, 3, 1>)),  // bsolr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<5, 3, 1>)),   // bnslr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<13, 3, 1>)),  // bunlr+
        LK_PSEUDO(GekkoMnemonic::Bclr, (FillBOBICond<5, 3, 1>)),   // bnulr+

        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<12, 0, 1>)),  // bltctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<4, 1, 1>)),   // blectr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<12, 2, 1>)),  // beqctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<4, 0, 1>)),   // bgectr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<12, 1, 1>)),  // bgtctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<4, 0, 1>)),   // bnlctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<4, 2, 1>)),   // bnectr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<4, 1, 1>)),   // bngctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<12, 3, 1>)),  // bsoctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<4, 3, 1>)),   // bnsctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<12, 3, 1>)),  // bunctr
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<4, 3, 1>)),   // bnuctr

        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<13, 0, 1>)),  // bltctr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<5, 1, 1>)),   // blectr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<13, 2, 1>)),  // beqctr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<5, 0, 1>)),   // bgectr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<13, 1, 1>)),  // bgtctr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<5, 0, 1>)),   // bnlctr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<5, 2, 1>)),   // bnectr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<5, 1, 1>)),   // bngctr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<13, 3, 1>)),  // bsoctr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<5, 3, 1>)),   // bnsctr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<13, 3, 1>)),  // bunctr+
        LK_PSEUDO(GekkoMnemonic::Bcctr, (FillBOBICond<5, 3, 1>)),   // bnuctr+

        // E.6
        PLAIN_PSEUDO(GekkoMnemonic::Creqv,
                     [](OperandList& operands) {
                       operands.Insert(1, operands[0]);
                       operands.Insert(2, operands[0]);
                     }),  // crset
        PLAIN_PSEUDO(GekkoMnemonic::Crxor,
                     [](OperandList& operands) {
                       operands.Insert(1, operands[0]);
                       operands.Insert(2, operands[0]);
                     }),  // crclr
        PLAIN_PSEUDO(GekkoMnemonic::Cror,
                     [](OperandList& operands) { operands.Insert(2, operands[1]); }),  // crmove
        PLAIN_PSEUDO(GekkoMnemonic::Crnor,
                     [](OperandList& operands) { operands.Insert(2, operands[1]); }),  // crnot

        // E.7
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<16>),   // twlt
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<16>),  // twlti
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<20>),   // twle
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<20>),  // twlei
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<4>),    // tweq
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<4>),   // tweqi
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<12>),   // twge
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<12>),  // twgei
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<8>),    // twgt
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<8>),   // twgti
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<12>),   // twnl
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<12>),  // twnli
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<24>),   // twne
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<24>),  // twnei
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<20>),   // twng
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<20>),  // twngi
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<2>),    // twllt
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<2>),   // twllti
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<6>),    // twlle
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<6>),   // twllei
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<5>),    // twlge
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<5>),   // twlgei
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<1>),    // twlgt
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<1>),   // twlgti
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<5>),    // twlnl
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<5>),   // twlnli
        PLAIN_PSEUDO(GekkoMnemonic::Tw, TrapSetTO<6>),    // twlng
        PLAIN_PSEUDO(GekkoMnemonic::Twi, TrapSetTO<6>),   // twlngi
        PLAIN_PSEUDO(GekkoMnemonic::Tw,
                     [](OperandList& operands) {
                       operands.Insert(0, 31);
                       operands.Insert(1, 0);
                       operands.Insert(2, 0);
                     }),  // trap

        // E.8
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(1)>),    // mtxer
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(1)>),    // mfxer
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(8)>),    // mtlr
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(8)>),    // mflr
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(9)>),    // mtctr
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(9)>),    // mfctr
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(18)>),   // mtdsisr
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(18)>),   // mfdsisr
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(19)>),   // mtdar
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(19)>),   // mfdar
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(22)>),   // mtdec
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(22)>),   // mfdec
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(25)>),   // mtsdr1
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(25)>),   // mfsdr1
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(26)>),   // mtsrr0
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(26)>),   // mfsrr0
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(27)>),   // mtsrr1
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(27)>),   // mfsrr1
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(280)>),  // mtasr
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(280)>),  // mfasr
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(282)>),  // mtear
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfspr<SprBitswap(282)>),  // mfear
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(284)>),  // mttbl
        PLAIN_PSEUDO(GekkoMnemonic::Mftb_nobitswap, FillMfspr<SprBitswap(268)>),   // mftbl
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtspr<SprBitswap(285)>),  // mttbu
        PLAIN_PSEUDO(GekkoMnemonic::Mftb_nobitswap, FillMfspr<SprBitswap(269)>),   // mftbu
        PLAIN_PSEUDO(
            GekkoMnemonic::Mtspr_nobitswap,
            [](OperandList& operands) { operands[0] = SprBitswap(operands[0] + 272); }),  // mtsprg
        PLAIN_PSEUDO(
            GekkoMnemonic::Mfspr_nobitswap,
            [](OperandList& operands) { operands[1] = SprBitswap(operands[1] + 272); }),  // mfsprg
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtsprBatAndBitswap<528>),        // mtibatu
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfsprBatAndBitswap<528>),        // mfibatu
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtsprBatAndBitswap<529>),        // mtibatl
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfsprBatAndBitswap<529>),        // mfibatl
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtsprBatAndBitswap<536>),        // mtdbatu
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfsprBatAndBitswap<536>),        // mfdbatu
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, FillMtsprBatAndBitswap<537>),        // mtdbatl
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, FillMfsprBatAndBitswap<537>),        // mfdbatl

        // E.9
        PLAIN_PSEUDO(GekkoMnemonic::Ori,
                     [](OperandList& operands) {
                       operands.Insert(0, 0);
                       operands.Insert(1, 0);
                       operands.Insert(2, 0);
                     }),  // nop
        PLAIN_PSEUDO(GekkoMnemonic::Addi,
                     [](OperandList& operands) { operands.Insert(1, 0); }),  // li
        PLAIN_PSEUDO(GekkoMnemonic::Addis,
                     [](OperandList& operands) { operands.Insert(1, 0); }),  // lis
        PLAIN_PSEUDO(GekkoMnemonic::Addi, SwapOps1And2),                     // la
        RC_PSEUDO(GekkoMnemonic::Or,
                  ([](OperandList& operands) { operands.Insert(2, operands[1]); })),  // mr
        RC_PSEUDO(GekkoMnemonic::Nor,
                  ([](OperandList& operands) { operands.Insert(2, operands[1]); })),  // not
        PLAIN_PSEUDO(GekkoMnemonic::Mtcrf,
                     [](OperandList& operands) { operands.Insert(0, 0xff); }),  // mtcr

        // Additional mnemonics
        PLAIN_PSEUDO(GekkoMnemonic::Mfspr_nobitswap, BitswapIdx<1>),  // mfspr
        PLAIN_PSEUDO(GekkoMnemonic::Mftb_nobitswap, BitswapIdx<1>),   // mfspr
        PLAIN_PSEUDO(GekkoMnemonic::Mtspr_nobitswap, BitswapIdx<0>),  // mtspr
};

#undef EMIT_MNEMONIC_ENTRY
#undef MNEMONIC
#undef BASIC_MNEMONIC
#undef RC_MNEMONIC
#undef OERC_MNEMONIC
#undef LK_MNEMONIC
#undef AALK_MNEMONIC
#undef PSEUDO
#undef PLAIN_PSEUDO
#undef RC_PSEUDO
#undef OERC_PSEUDO
#undef LK_PSEUDO
#undef LKAA_PSEUDO

//////////////////
// LEXER TABLES //
//////////////////

namespace
{
constexpr TransitionF HasPlusOrMinus = [](char c) { return c == '+' || c == '-'; };
constexpr TransitionF HasDigit = [](char c) -> bool { return std::isdigit(c); };
constexpr TransitionF HasE = [](char c) { return c == 'e'; };
constexpr TransitionF HasDot = [](char c) { return c == '.'; };

// Normal string characters
constexpr TransitionF HasNormal = [](char c) { return c != '\n' && c != '"' && c != '\\'; };
// Invalid characters in string
constexpr TransitionF HasInvalid = [](char c) { return c == '\n'; };
// Octal digits
constexpr TransitionF HasOctal = [](char c) { return c >= '0' && c <= '7'; };
// Hex digits
constexpr TransitionF HasHex = [](char c) -> bool { return std::isxdigit(c); };
// Normal - octal
constexpr TransitionF HasNormalMinusOctal = [](char c) { return HasNormal(c) && !HasOctal(c); };
// Normal - hex
constexpr TransitionF HasNormalMinusHex = [](char c) { return HasNormal(c) && !HasHex(c); };
// Escape start
constexpr TransitionF HasEscape = [](char c) { return c == '\\'; };
// All single-character escapes
constexpr TransitionF HasSCE = [](char c) { return !HasOctal(c) && c != 'x' && c != '\n'; };
// Hex escape
constexpr TransitionF HasHexStart = [](char c) { return c == 'x'; };
constexpr TransitionF HasQuote = [](char c) { return c == '"'; };
}  // namespace

extern const std::vector<DfaNode> float_dfa = {
    {{DfaEdge(HasPlusOrMinus, 1), DfaEdge(HasDigit, 2), DfaEdge(HasDot, 5)},
     "Invalid float: No numeric value"},

    {{DfaEdge(HasDigit, 2), DfaEdge(HasDot, 5)}, "Invalid float: No numeric value"},

    {{DfaEdge(HasDigit, 2), DfaEdge(HasDot, 3), DfaEdge(HasE, 7)}, std::nullopt},
    {{DfaEdge(HasDigit, 4)}, "Invalid float: No numeric value after decimal point"},
    {{DfaEdge(HasDigit, 4), DfaEdge(HasE, 7)}, std::nullopt},

    {{DfaEdge(HasDigit, 6)}, "Invalid float: No numeric value after decimal point"},
    {{DfaEdge(HasDigit, 6), DfaEdge(HasE, 7)}, std::nullopt},

    {{DfaEdge(HasDigit, 9), DfaEdge(HasPlusOrMinus, 8)},
     "Invalid float: No numeric value following exponent signifier"},
    {{DfaEdge(HasDigit, 9)}, "Invalid float: No numeric value following exponent signifier"},
    {{DfaEdge(HasDigit, 9)}, std::nullopt},
};

extern const std::vector<DfaNode> string_dfa = {
    // Base character check
    {{DfaEdge(HasNormal, 0), DfaEdge(HasInvalid, 1), DfaEdge(HasQuote, 2), DfaEdge(HasEscape, 3)},
     "Invalid string: No terminating \""},

    // Invalid (unescaped newline)
    {{}, "Invalid string: No terminating \""},
    // String end
    {{}, std::nullopt},

    // Escape character breakout
    {{DfaEdge(HasSCE, 0), DfaEdge(HasInvalid, 1), DfaEdge(HasOctal, 4), DfaEdge(HasHexStart, 6)},
     "Invalid string: No terminating \""},

    // Octal characters, at most 3
    {{DfaEdge(HasNormalMinusOctal, 0), DfaEdge(HasInvalid, 1), DfaEdge(HasQuote, 2),
      DfaEdge(HasEscape, 3), DfaEdge(HasOctal, 5)},
     "Invalid string: No terminating \""},
    {{DfaEdge(HasNormal, 0), DfaEdge(HasInvalid, 1), DfaEdge(HasQuote, 2), DfaEdge(HasEscape, 3)},
     "Invalid string: No terminating \""},

    // Hex characters, 1 or more
    {{DfaEdge(HasHex, 7)}, "Invalid string: bad hex escape"},
    {{DfaEdge(HasNormalMinusHex, 0), DfaEdge(HasInvalid, 1), DfaEdge(HasQuote, 2),
      DfaEdge(HasEscape, 3), DfaEdge(HasHex, 7)},
     "Invalid string: No terminating \""},
};
}  // namespace Common::GekkoAssembler::detail
