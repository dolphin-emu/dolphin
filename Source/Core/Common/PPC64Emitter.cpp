// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/PPC64Emitter.h"

#include <algorithm>
#include <cstring>

#ifdef __linux__
#include <sys/auxv.h>
#endif

#include "Common/Assert.h"
#include "Common/CommonTypes.h"

namespace PPC64Gen
{
namespace
{
constexpr u32 TRAP_INSTRUCTION = 0x7FE00008;  // tw 31, 0, 0

// Primary opcodes
constexpr u32 OP_MULLI = 7;
constexpr u32 OP_SUBFIC = 8;
constexpr u32 OP_CMPLI = 10;
constexpr u32 OP_CMPI = 11;
constexpr u32 OP_ADDIC = 12;
constexpr u32 OP_ADDIC_ = 13;
constexpr u32 OP_ADDI = 14;
constexpr u32 OP_ADDIS = 15;
constexpr u32 OP_BC = 16;
constexpr u32 OP_B = 18;
constexpr u32 OP_XL = 19;
constexpr u32 OP_RLWIMI = 20;
constexpr u32 OP_RLWINM = 21;
constexpr u32 OP_RLWNM = 23;
constexpr u32 OP_ORI = 24;
constexpr u32 OP_ORIS = 25;
constexpr u32 OP_XORI = 26;
constexpr u32 OP_XORIS = 27;
constexpr u32 OP_ANDI_ = 28;
constexpr u32 OP_ANDIS_ = 29;
constexpr u32 OP_MD = 30;
constexpr u32 OP_X = 31;
constexpr u32 OP_LWZ = 32;
constexpr u32 OP_LBZ = 34;
constexpr u32 OP_STW = 36;
constexpr u32 OP_STB = 38;
constexpr u32 OP_LHZ = 40;
constexpr u32 OP_LHA = 42;
constexpr u32 OP_STH = 44;
constexpr u32 OP_LFS = 48;
constexpr u32 OP_LFD = 50;
constexpr u32 OP_STFS = 52;
constexpr u32 OP_STFD = 54;
constexpr u32 OP_LXSD = 57;
constexpr u32 OP_DS_LOAD = 58;
constexpr u32 OP_FP_SINGLE = 59;
constexpr u32 OP_VSX = 60;
constexpr u32 OP_LXV = 61;
constexpr u32 OP_DS_STORE = 62;
constexpr u32 OP_FP_DOUBLE = 63;
constexpr u32 OP_VMX = 4;

// Branch BO fields
constexpr u32 BO_IF_FALSE = 4;
constexpr u32 BO_IF_TRUE = 12;
constexpr u32 BO_DNZ = 16;
constexpr u32 BO_ALWAYS = 20;

constexpr bool IsInRange(s64 offset, u32 bits)
{
  return offset >= -(s64(1) << (bits - 1)) && offset < (s64(1) << (bits - 1));
}

constexpr u32 BranchBO(CCFlags cond)
{
  return static_cast<u8>(cond) < 4 ? BO_IF_TRUE : BO_IF_FALSE;
}

// The 6-bit MB and ME fields of MD-form instructions are encoded with the high bit last.
constexpr u32 EncodeMB6(u32 mb)
{
  return ((mb & 0x1F) << 1) | (mb >> 5);
}
}  // namespace

void PPC64XEmitter::SetCodePtrUnsafe(u8* ptr, u8* end, bool write_failed)
{
  m_code = ptr;
  m_code_end = end;
  m_write_failed = write_failed;
}

void PPC64XEmitter::SetCodePtr(u8* ptr, u8* end, bool write_failed)
{
  SetCodePtrUnsafe(ptr, end, write_failed);
  m_last_cache_flush_end = ptr;
}

void PPC64XEmitter::ReserveCodeSpace(u32 bytes)
{
  for (u32 i = 0; i < bytes / 4; i++)
    TRAP();
}

u8* PPC64XEmitter::AlignCode16()
{
  const int c = int(reinterpret_cast<u64>(m_code) & 15);
  if (c)
    ReserveCodeSpace(16 - c);
  return m_code;
}

u8* PPC64XEmitter::AlignCodePage()
{
  const int c = int(reinterpret_cast<u64>(m_code) & 4095);
  if (c)
    ReserveCodeSpace(4096 - c);
  return m_code;
}

void PPC64XEmitter::Write32(u32 value)
{
  if (m_code + sizeof(u32) > m_code_end)
  {
    m_code = m_code_end;
    m_write_failed = true;
    return;
  }

  std::memcpy(m_code, &value, sizeof(u32));
  m_code += sizeof(u32);
}

void PPC64XEmitter::AlignForPrefixed()
{
  if ((reinterpret_cast<uintptr_t>(m_code) & 63) == 60)
    NOP();
}

void PPC64XEmitter::WritePrefixed(u32 prefix, u32 suffix)
{
  AlignForPrefixed();
  Write32(prefix);
  Write32(suffix);
}

bool PPC64XEmitter::IsInPrefixedPCRange(const void* target) const
{
  // Allowing for the alignment of the instruction.
  const s64 offset = static_cast<const u8*>(target) - m_code;
  return IsInPrefixedRange(offset) && IsInPrefixedRange(offset - 4);
}

void PPC64XEmitter::FlushIcache()
{
  FlushIcacheSection(m_last_cache_flush_end, m_code);
  m_last_cache_flush_end = m_code;
}

void PPC64XEmitter::FlushIcacheSection(u8* start, u8* end)
{
  if (start == end)
    return;

  // Write the data cache blocks back, then invalidate the instruction cache blocks, as in
  // Power ISA Book II, "Instruction Storage". A smaller block size than the real one only costs
  // redundant instructions, so fall back to the smallest one the architecture allows.
#ifdef __linux__
  static const size_t dcache_block_size = std::max<size_t>(getauxval(AT_DCACHEBSIZE), 32);
  static const size_t icache_block_size = std::max<size_t>(getauxval(AT_ICACHEBSIZE), 32);
#else
  static constexpr size_t dcache_block_size = 32;
  static constexpr size_t icache_block_size = 32;
#endif

  const u64 end_address = reinterpret_cast<u64>(end);
  for (u64 address = reinterpret_cast<u64>(start) & ~(dcache_block_size - 1); address < end_address;
       address += dcache_block_size)
  {
    __asm__ __volatile__("dcbst 0, %0" : : "r"(address) : "memory");
  }
  __asm__ __volatile__("sync" : : : "memory");
  for (u64 address = reinterpret_cast<u64>(start) & ~(icache_block_size - 1); address < end_address;
       address += icache_block_size)
  {
    __asm__ __volatile__("icbi 0, %0" : : "r"(address) : "memory");
  }
  __asm__ __volatile__("sync\n\tisync" : : : "memory");
}

// Encoding

void PPC64XEmitter::EncodeIForm(u32 op, s64 offset, bool aa, bool lk)
{
  ASSERT_MSG(DYNA_REC, (offset & 3) == 0, "Misaligned branch offset {}", offset);
  ASSERT_MSG(DYNA_REC, IsInRange(offset, 26), "Branch offset {} out of range", offset);
  Write32((op << 26) | (static_cast<u32>(offset) & 0x03FFFFFC) | (aa << 1) | lk);
}

void PPC64XEmitter::EncodeBForm(u32 op, u32 bo, u32 bi, s64 offset, bool aa, bool lk)
{
  ASSERT_MSG(DYNA_REC, (offset & 3) == 0, "Misaligned branch offset {}", offset);
  ASSERT_MSG(DYNA_REC, IsInRange(offset, 16), "Conditional branch offset {} out of range", offset);
  Write32((op << 26) | (bo << 21) | (bi << 16) | (static_cast<u32>(offset) & 0xFFFC) | (aa << 1) |
          lk);
}

void PPC64XEmitter::EncodeDForm(u32 op, u32 rt, u32 ra, u32 d)
{
  Write32((op << 26) | (rt << 21) | (ra << 16) | (d & 0xFFFF));
}

void PPC64XEmitter::EncodeDSForm(u32 op, u32 rt, u32 ra, s32 ds, u32 xo)
{
  ASSERT_MSG(DYNA_REC, (ds & 3) == 0, "DS-form offset {} is not a multiple of 4", ds);
  Write32((op << 26) | (rt << 21) | (ra << 16) | (static_cast<u32>(ds) & 0xFFFC) | xo);
}

void PPC64XEmitter::EncodeXForm(u32 op, u32 rt, u32 ra, u32 rb, u32 xo, bool rc)
{
  Write32((op << 26) | (rt << 21) | (ra << 16) | (rb << 11) | (xo << 1) | rc);
}

void PPC64XEmitter::EncodeXOForm(u32 rt, u32 ra, u32 rb, u32 xo, bool rc)
{
  // OE is always 0.
  EncodeXForm(OP_X, rt, ra, rb, xo, rc);
}

void PPC64XEmitter::EncodeXLForm(u32 bt, u32 ba, u32 bb, u32 xo, bool lk)
{
  EncodeXForm(OP_XL, bt, ba, bb, xo, lk);
}

void PPC64XEmitter::EncodeMForm(u32 op, u32 rs, u32 ra, u32 rb_sh, u32 mb, u32 me, bool rc)
{
  ASSERT(rb_sh < 32 && mb < 32 && me < 32);
  Write32((op << 26) | (rs << 21) | (ra << 16) | (rb_sh << 11) | (mb << 6) | (me << 1) | rc);
}

void PPC64XEmitter::EncodeMDForm(u32 rs, u32 ra, u32 sh, u32 mb, u32 xo, bool rc)
{
  ASSERT(sh < 64 && mb < 64);
  Write32((OP_MD << 26) | (rs << 21) | (ra << 16) | ((sh & 0x1F) << 11) | (EncodeMB6(mb) << 5) |
          (xo << 2) | ((sh >> 5) << 1) | rc);
}

void PPC64XEmitter::EncodeAForm(u32 op, u32 frt, u32 fra, u32 frb, u32 frc, u32 xo, bool rc)
{
  Write32((op << 26) | (frt << 21) | (fra << 16) | (frb << 11) | (frc << 6) | (xo << 1) | rc);
}

void PPC64XEmitter::EncodeXX1Form(u32 t, GPR ra, GPR rb, u32 xo)
{
  Write32((OP_X << 26) | ((t & 0x1F) << 21) | (DecodeReg(ra) << 16) | (DecodeReg(rb) << 11) |
          (xo << 1) | (t >> 5));
}

void PPC64XEmitter::EncodeXX2Form(u32 t, u32 b, u32 xo)
{
  Write32((OP_VSX << 26) | ((t & 0x1F) << 21) | ((b & 0x1F) << 11) | (xo << 2) | ((b >> 5) << 1) |
          (t >> 5));
}

void PPC64XEmitter::EncodeXX3Form(u32 t, u32 a, u32 b, u32 xo)
{
  Write32((OP_VSX << 26) | ((t & 0x1F) << 21) | ((a & 0x1F) << 16) | ((b & 0x1F) << 11) |
          (xo << 3) | ((a >> 5) << 2) | ((b >> 5) << 1) | (t >> 5));
}

void PPC64XEmitter::EncodeSPR(u32 rs, SPR spr, u32 xo)
{
  // The two halves of the SPR number are swapped in the instruction.
  const u32 n = static_cast<u32>(spr);
  EncodeXForm(OP_X, rs, n & 0x1F, (n >> 5) & 0x1F, xo);
}

// Branches

bool PPC64XEmitter::IsInBranchRange(const void* target) const
{
  return IsInRange(reinterpret_cast<const u8*>(target) - m_code, 26);
}

bool PPC64XEmitter::IsInConditionalBranchRange(const void* target) const
{
  return IsInRange(reinterpret_cast<const u8*>(target) - m_code, 16);
}

FixupBranch PPC64XEmitter::WriteFixupBranch(FixupBranch::Type type, u8 bo, u8 bi)
{
  FixupBranch branch{};
  branch.ptr = m_code;
  branch.type = type;
  branch.bo = bo;
  branch.bi = bi;
  TRAP();

  // If we couldn't write the full jump instruction, indicate that in the returned FixupBranch by
  // setting the branch's address to null. This will prevent a later SetJumpTarget() from writing to
  // invalid memory.
  if (HasWriteFailed())
    branch.ptr = nullptr;

  return branch;
}

void PPC64XEmitter::SetJumpTarget(const FixupBranch& branch)
{
  SetJumpTarget(branch, m_code);
}

void PPC64XEmitter::SetJumpTarget(const FixupBranch& branch, const u8* target)
{
  if (!branch.ptr)
    return;

  u8* const code = m_code;
  u8* const code_end = m_code_end;
  const bool write_failed = m_write_failed;

  // Encode the branch in place, as though emitting it at its own address.
  SetCodePtrUnsafe(branch.ptr, branch.ptr + sizeof(u32));
  const s64 offset = target - branch.ptr;
  switch (branch.type)
  {
  case FixupBranch::Type::B:
    EncodeIForm(OP_B, offset, false, false);
    break;
  case FixupBranch::Type::BL:
    EncodeIForm(OP_B, offset, false, true);
    break;
  case FixupBranch::Type::BC:
    EncodeBForm(OP_BC, branch.bo, branch.bi, offset, false, false);
    break;
  }
  SetCodePtrUnsafe(code, code_end, write_failed);
}

FixupBranch PPC64XEmitter::B()
{
  return WriteFixupBranch(FixupBranch::Type::B);
}

FixupBranch PPC64XEmitter::BL()
{
  return WriteFixupBranch(FixupBranch::Type::BL);
}

FixupBranch PPC64XEmitter::BC(CCFlags cond, CRF field)
{
  return WriteFixupBranch(FixupBranch::Type::BC, BranchBO(cond), CRBit(field, cond));
}

FixupBranch PPC64XEmitter::BDNZ()
{
  return WriteFixupBranch(FixupBranch::Type::BC, BO_DNZ, 0);
}

void PPC64XEmitter::B(const void* target)
{
  EncodeIForm(OP_B, reinterpret_cast<const u8*>(target) - m_code, false, false);
}

void PPC64XEmitter::BL(const void* target)
{
  EncodeIForm(OP_B, reinterpret_cast<const u8*>(target) - m_code, false, true);
}

void PPC64XEmitter::BC(CCFlags cond, CRF field, const void* target)
{
  EncodeBForm(OP_BC, BranchBO(cond), CRBit(field, cond),
              reinterpret_cast<const u8*>(target) - m_code, false, false);
}

void PPC64XEmitter::BCLR(CCFlags cond, CRF field)
{
  EncodeXLForm(BranchBO(cond), CRBit(field, cond), 0, 16);
}

void PPC64XEmitter::BLR()
{
  EncodeXLForm(BO_ALWAYS, 0, 0, 16);
}

void PPC64XEmitter::BCL_NEXT()
{
  EncodeBForm(OP_BC, BO_ALWAYS, 31, 4, false, true);
}

void PPC64XEmitter::BCTR()
{
  EncodeXLForm(BO_ALWAYS, 0, 0, 528);
}

void PPC64XEmitter::BCTRL()
{
  EncodeXLForm(BO_ALWAYS, 0, 0, 528, true);
}

// Integer arithmetic

void PPC64XEmitter::ADDI(GPR rt, GPR ra, s16 imm)
{
  EncodeDForm(OP_ADDI, DecodeReg(rt), DecodeReg(ra), static_cast<u16>(imm));
}

void PPC64XEmitter::ADDIS(GPR rt, GPR ra, s16 imm)
{
  EncodeDForm(OP_ADDIS, DecodeReg(rt), DecodeReg(ra), static_cast<u16>(imm));
}

void PPC64XEmitter::ADDIC(GPR rt, GPR ra, s16 imm, bool rc)
{
  EncodeDForm(rc ? OP_ADDIC_ : OP_ADDIC, DecodeReg(rt), DecodeReg(ra), static_cast<u16>(imm));
}

void PPC64XEmitter::SUBFIC(GPR rt, GPR ra, s16 imm)
{
  EncodeDForm(OP_SUBFIC, DecodeReg(rt), DecodeReg(ra), static_cast<u16>(imm));
}

void PPC64XEmitter::MULLI(GPR rt, GPR ra, s16 imm)
{
  EncodeDForm(OP_MULLI, DecodeReg(rt), DecodeReg(ra), static_cast<u16>(imm));
}

void PPC64XEmitter::ADD(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 266, rc);
}

void PPC64XEmitter::ADDC(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 10, rc);
}

void PPC64XEmitter::ADDE(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 138, rc);
}

void PPC64XEmitter::ADDZE(GPR rt, GPR ra, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), 0, 202, rc);
}

void PPC64XEmitter::ADDME(GPR rt, GPR ra, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), 0, 234, rc);
}

void PPC64XEmitter::SUBF(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 40, rc);
}

void PPC64XEmitter::SUBFC(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 8, rc);
}

void PPC64XEmitter::SUBFE(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 136, rc);
}

void PPC64XEmitter::SUBFZE(GPR rt, GPR ra, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), 0, 200, rc);
}

void PPC64XEmitter::SUBFME(GPR rt, GPR ra, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), 0, 232, rc);
}

void PPC64XEmitter::NEG(GPR rt, GPR ra, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), 0, 104, rc);
}

void PPC64XEmitter::MULLW(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 235, rc);
}

void PPC64XEmitter::MULLD(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 233, rc);
}

void PPC64XEmitter::MULHW(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 75, rc);
}

void PPC64XEmitter::MULHWU(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 11, rc);
}

void PPC64XEmitter::MULHD(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 73, rc);
}

void PPC64XEmitter::MULHDU(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 9, rc);
}

void PPC64XEmitter::DIVW(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 491, rc);
}

void PPC64XEmitter::DIVWU(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 459, rc);
}

void PPC64XEmitter::DIVD(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 489, rc);
}

void PPC64XEmitter::DIVDU(GPR rt, GPR ra, GPR rb, bool rc)
{
  EncodeXOForm(DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 457, rc);
}

// Integer comparison

void PPC64XEmitter::CMPWI(CRF bf, GPR ra, s16 imm)
{
  EncodeDForm(OP_CMPI, DecodeReg(bf) << 2, DecodeReg(ra), static_cast<u16>(imm));
}

void PPC64XEmitter::CMPDI(CRF bf, GPR ra, s16 imm)
{
  EncodeDForm(OP_CMPI, (DecodeReg(bf) << 2) | 1, DecodeReg(ra), static_cast<u16>(imm));
}

void PPC64XEmitter::CMPLWI(CRF bf, GPR ra, u16 imm)
{
  EncodeDForm(OP_CMPLI, DecodeReg(bf) << 2, DecodeReg(ra), imm);
}

void PPC64XEmitter::CMPLDI(CRF bf, GPR ra, u16 imm)
{
  EncodeDForm(OP_CMPLI, (DecodeReg(bf) << 2) | 1, DecodeReg(ra), imm);
}

void PPC64XEmitter::CMPW(CRF bf, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(bf) << 2, DecodeReg(ra), DecodeReg(rb), 0);
}

void PPC64XEmitter::CMPD(CRF bf, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, (DecodeReg(bf) << 2) | 1, DecodeReg(ra), DecodeReg(rb), 0);
}

void PPC64XEmitter::CMPLW(CRF bf, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(bf) << 2, DecodeReg(ra), DecodeReg(rb), 32);
}

void PPC64XEmitter::CMPLD(CRF bf, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, (DecodeReg(bf) << 2) | 1, DecodeReg(ra), DecodeReg(rb), 32);
}

void PPC64XEmitter::ISEL(GPR rt, GPR ra, GPR rb, u32 bc)
{
  ASSERT(bc < 32);
  Write32((OP_X << 26) | (DecodeReg(rt) << 21) | (DecodeReg(ra) << 16) | (DecodeReg(rb) << 11) |
          (bc << 6) | (15 << 1));
}

// Logical

void PPC64XEmitter::ANDI_(GPR ra, GPR rs, u16 imm)
{
  EncodeDForm(OP_ANDI_, DecodeReg(rs), DecodeReg(ra), imm);
}

void PPC64XEmitter::ANDIS_(GPR ra, GPR rs, u16 imm)
{
  EncodeDForm(OP_ANDIS_, DecodeReg(rs), DecodeReg(ra), imm);
}

void PPC64XEmitter::ORI(GPR ra, GPR rs, u16 imm)
{
  EncodeDForm(OP_ORI, DecodeReg(rs), DecodeReg(ra), imm);
}

void PPC64XEmitter::ORIS(GPR ra, GPR rs, u16 imm)
{
  EncodeDForm(OP_ORIS, DecodeReg(rs), DecodeReg(ra), imm);
}

void PPC64XEmitter::XORI(GPR ra, GPR rs, u16 imm)
{
  EncodeDForm(OP_XORI, DecodeReg(rs), DecodeReg(ra), imm);
}

void PPC64XEmitter::XORIS(GPR ra, GPR rs, u16 imm)
{
  EncodeDForm(OP_XORIS, DecodeReg(rs), DecodeReg(ra), imm);
}

void PPC64XEmitter::AND(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 28, rc);
}

void PPC64XEmitter::ANDC(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 60, rc);
}

void PPC64XEmitter::OR(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 444, rc);
}

void PPC64XEmitter::ORC(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 412, rc);
}

void PPC64XEmitter::XOR(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 316, rc);
}

void PPC64XEmitter::NAND(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 476, rc);
}

void PPC64XEmitter::NOR(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 124, rc);
}

void PPC64XEmitter::EQV(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 284, rc);
}

void PPC64XEmitter::EXTSB(GPR ra, GPR rs, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 954, rc);
}

void PPC64XEmitter::EXTSH(GPR ra, GPR rs, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 922, rc);
}

void PPC64XEmitter::EXTSW(GPR ra, GPR rs, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 986, rc);
}

void PPC64XEmitter::CNTLZW(GPR ra, GPR rs, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 26, rc);
}

void PPC64XEmitter::CNTLZD(GPR ra, GPR rs, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 58, rc);
}

void PPC64XEmitter::POPCNTW(GPR ra, GPR rs)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 378);
}

void PPC64XEmitter::POPCNTD(GPR ra, GPR rs)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 506);
}

// Shifts and rotates

void PPC64XEmitter::SLW(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 24, rc);
}

void PPC64XEmitter::SRW(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 536, rc);
}

void PPC64XEmitter::SRAW(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 792, rc);
}

void PPC64XEmitter::SRAWI(GPR ra, GPR rs, u32 sh, bool rc)
{
  ASSERT(sh < 32);
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), sh, 824, rc);
}

void PPC64XEmitter::SLD(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 27, rc);
}

void PPC64XEmitter::SRD(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 539, rc);
}

void PPC64XEmitter::SRAD(GPR ra, GPR rs, GPR rb, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 794, rc);
}

void PPC64XEmitter::SRADI(GPR ra, GPR rs, u32 sh, bool rc)
{
  // XS-form
  ASSERT(sh < 64);
  Write32((OP_X << 26) | (DecodeReg(rs) << 21) | (DecodeReg(ra) << 16) | ((sh & 0x1F) << 11) |
          (413 << 2) | ((sh >> 5) << 1) | rc);
}

void PPC64XEmitter::RLWINM(GPR ra, GPR rs, u32 sh, u32 mb, u32 me, bool rc)
{
  EncodeMForm(OP_RLWINM, DecodeReg(rs), DecodeReg(ra), sh, mb, me, rc);
}

void PPC64XEmitter::RLWNM(GPR ra, GPR rs, GPR rb, u32 mb, u32 me, bool rc)
{
  EncodeMForm(OP_RLWNM, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), mb, me, rc);
}

void PPC64XEmitter::RLWIMI(GPR ra, GPR rs, u32 sh, u32 mb, u32 me, bool rc)
{
  EncodeMForm(OP_RLWIMI, DecodeReg(rs), DecodeReg(ra), sh, mb, me, rc);
}

void PPC64XEmitter::RLDICL(GPR ra, GPR rs, u32 sh, u32 mb, bool rc)
{
  EncodeMDForm(DecodeReg(rs), DecodeReg(ra), sh, mb, 0, rc);
}

void PPC64XEmitter::RLDICR(GPR ra, GPR rs, u32 sh, u32 me, bool rc)
{
  EncodeMDForm(DecodeReg(rs), DecodeReg(ra), sh, me, 1, rc);
}

void PPC64XEmitter::RLDIC(GPR ra, GPR rs, u32 sh, u32 mb, bool rc)
{
  EncodeMDForm(DecodeReg(rs), DecodeReg(ra), sh, mb, 2, rc);
}

void PPC64XEmitter::RLDIMI(GPR ra, GPR rs, u32 sh, u32 mb, bool rc)
{
  EncodeMDForm(DecodeReg(rs), DecodeReg(ra), sh, mb, 3, rc);
}

void PPC64XEmitter::RLDCL(GPR ra, GPR rs, GPR rb, u32 mb, bool rc)
{
  // MDS-form
  ASSERT(mb < 64);
  Write32((OP_MD << 26) | (DecodeReg(rs) << 21) | (DecodeReg(ra) << 16) | (DecodeReg(rb) << 11) |
          (EncodeMB6(mb) << 5) | (8 << 1) | rc);
}

// Loads

void PPC64XEmitter::LBZ(GPR rt, GPR ra, s16 offset)
{
  EncodeDForm(OP_LBZ, DecodeReg(rt), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::LHZ(GPR rt, GPR ra, s16 offset)
{
  EncodeDForm(OP_LHZ, DecodeReg(rt), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::LHA(GPR rt, GPR ra, s16 offset)
{
  EncodeDForm(OP_LHA, DecodeReg(rt), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::LWZ(GPR rt, GPR ra, s16 offset)
{
  EncodeDForm(OP_LWZ, DecodeReg(rt), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::LWA(GPR rt, GPR ra, s16 offset)
{
  EncodeDSForm(OP_DS_LOAD, DecodeReg(rt), DecodeReg(ra), offset, 2);
}

void PPC64XEmitter::LD(GPR rt, GPR ra, s16 offset)
{
  EncodeDSForm(OP_DS_LOAD, DecodeReg(rt), DecodeReg(ra), offset, 0);
}

void PPC64XEmitter::LBZX(GPR rt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 87);
}

void PPC64XEmitter::LHZX(GPR rt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 279);
}

void PPC64XEmitter::LHAX(GPR rt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 343);
}

void PPC64XEmitter::LWZX(GPR rt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 23);
}

void PPC64XEmitter::LWAX(GPR rt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 341);
}

void PPC64XEmitter::LDX(GPR rt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 21);
}

void PPC64XEmitter::LHBRX(GPR rt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 790);
}

void PPC64XEmitter::LWBRX(GPR rt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 534);
}

void PPC64XEmitter::LDBRX(GPR rt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(ra), DecodeReg(rb), 532);
}

// Stores

void PPC64XEmitter::STB(GPR rs, GPR ra, s16 offset)
{
  EncodeDForm(OP_STB, DecodeReg(rs), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::STH(GPR rs, GPR ra, s16 offset)
{
  EncodeDForm(OP_STH, DecodeReg(rs), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::STW(GPR rs, GPR ra, s16 offset)
{
  EncodeDForm(OP_STW, DecodeReg(rs), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::STD(GPR rs, GPR ra, s16 offset)
{
  EncodeDSForm(OP_DS_STORE, DecodeReg(rs), DecodeReg(ra), offset, 0);
}

void PPC64XEmitter::STDU(GPR rs, GPR ra, s16 offset)
{
  EncodeDSForm(OP_DS_STORE, DecodeReg(rs), DecodeReg(ra), offset, 1);
}

void PPC64XEmitter::STBX(GPR rs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 215);
}

void PPC64XEmitter::STHX(GPR rs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 407);
}

void PPC64XEmitter::STWX(GPR rs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 151);
}

void PPC64XEmitter::STDX(GPR rs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 149);
}

void PPC64XEmitter::STHBRX(GPR rs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 918);
}

void PPC64XEmitter::STWBRX(GPR rs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 662);
}

void PPC64XEmitter::STDBRX(GPR rs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), DecodeReg(rb), 660);
}

// Condition register

void PPC64XEmitter::CRAND(u32 bt, u32 ba, u32 bb)
{
  EncodeXLForm(bt, ba, bb, 257);
}

void PPC64XEmitter::CROR(u32 bt, u32 ba, u32 bb)
{
  EncodeXLForm(bt, ba, bb, 449);
}

void PPC64XEmitter::CRXOR(u32 bt, u32 ba, u32 bb)
{
  EncodeXLForm(bt, ba, bb, 193);
}

void PPC64XEmitter::CRNAND(u32 bt, u32 ba, u32 bb)
{
  EncodeXLForm(bt, ba, bb, 225);
}

void PPC64XEmitter::CRNOR(u32 bt, u32 ba, u32 bb)
{
  EncodeXLForm(bt, ba, bb, 33);
}

void PPC64XEmitter::CREQV(u32 bt, u32 ba, u32 bb)
{
  EncodeXLForm(bt, ba, bb, 289);
}

void PPC64XEmitter::CRANDC(u32 bt, u32 ba, u32 bb)
{
  EncodeXLForm(bt, ba, bb, 129);
}

void PPC64XEmitter::CRORC(u32 bt, u32 ba, u32 bb)
{
  EncodeXLForm(bt, ba, bb, 417);
}

void PPC64XEmitter::MCRF(CRF bf, CRF bfa)
{
  EncodeXLForm(DecodeReg(bf) << 2, DecodeReg(bfa) << 2, 0, 0);
}

void PPC64XEmitter::MFCR(GPR rt)
{
  EncodeXForm(OP_X, DecodeReg(rt), 0, 0, 19);
}

void PPC64XEmitter::MFOCRF(GPR rt, CRF field)
{
  Write32((OP_X << 26) | (DecodeReg(rt) << 21) | (1 << 20) | ((0x80 >> DecodeReg(field)) << 12) |
          (19 << 1));
}

void PPC64XEmitter::MTCRF(u32 mask, GPR rs)
{
  ASSERT(mask < 0x100);
  Write32((OP_X << 26) | (DecodeReg(rs) << 21) | (mask << 12) | (144 << 1));
}

void PPC64XEmitter::MTOCRF(CRF field, GPR rs)
{
  Write32((OP_X << 26) | (DecodeReg(rs) << 21) | (1 << 20) | ((0x80 >> DecodeReg(field)) << 12) |
          (144 << 1));
}

// Special purpose registers

void PPC64XEmitter::MFSPR(GPR rt, SPR spr)
{
  EncodeSPR(DecodeReg(rt), spr, 339);
}

void PPC64XEmitter::MTSPR(SPR spr, GPR rs)
{
  EncodeSPR(DecodeReg(rs), spr, 467);
}

// Storage control and synchronisation

void PPC64XEmitter::SYNC()
{
  EncodeXForm(OP_X, 0, 0, 0, 598);
}

void PPC64XEmitter::LWSYNC()
{
  EncodeXForm(OP_X, 1, 0, 0, 598);
}

void PPC64XEmitter::ISYNC()
{
  EncodeXLForm(0, 0, 0, 150);
}

void PPC64XEmitter::ICBI(GPR ra, GPR rb)
{
  EncodeXForm(OP_X, 0, DecodeReg(ra), DecodeReg(rb), 982);
}

void PPC64XEmitter::DCBST(GPR ra, GPR rb)
{
  EncodeXForm(OP_X, 0, DecodeReg(ra), DecodeReg(rb), 54);
}

void PPC64XEmitter::TRAP()
{
  Write32(TRAP_INSTRUCTION);
}

void PPC64XEmitter::CNTTZW(GPR ra, GPR rs, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 538, rc);
}

void PPC64XEmitter::CNTTZD(GPR ra, GPR rs, bool rc)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 570, rc);
}

void PPC64XEmitter::BRH(GPR ra, GPR rs)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 219);
}

void PPC64XEmitter::BRW(GPR ra, GPR rs)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 155);
}

void PPC64XEmitter::BRD(GPR ra, GPR rs)
{
  EncodeXForm(OP_X, DecodeReg(rs), DecodeReg(ra), 0, 187);
}

void PPC64XEmitter::SETB(GPR rt, CRF bfa)
{
  EncodeXForm(OP_X, DecodeReg(rt), DecodeReg(bfa) << 2, 0, 128);
}

void PPC64XEmitter::ADDPCIS(GPR rt, s16 d)
{
  // DX-form: d0, d1 and d2 are the upper 10 bits, the next 5 and the lowest of d.
  const u32 bits = static_cast<u16>(d);
  Write32((19u << 26) | (DecodeReg(rt) << 21) | (((bits >> 1) & 0x1F) << 16) | ((bits >> 6) << 6) |
          (2u << 1) | (bits & 1));
}

void PPC64XEmitter::SETBC(GPR rt, u32 bi)
{
  EncodeXForm(OP_X, DecodeReg(rt), bi, 0, 384);
}

void PPC64XEmitter::SETBCR(GPR rt, u32 bi)
{
  EncodeXForm(OP_X, DecodeReg(rt), bi, 0, 416);
}

void PPC64XEmitter::SETNBC(GPR rt, u32 bi)
{
  EncodeXForm(OP_X, DecodeReg(rt), bi, 0, 448);
}

void PPC64XEmitter::SETNBCR(GPR rt, u32 bi)
{
  EncodeXForm(OP_X, DecodeReg(rt), bi, 0, 480);
}

void PPC64XEmitter::NOP()
{
  ORI(GPR::R0, GPR::R0, 0);
}

// Floating-point loads and stores

void PPC64XEmitter::LFS(FPR frt, GPR ra, s16 offset)
{
  EncodeDForm(OP_LFS, DecodeReg(frt), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::LFD(FPR frt, GPR ra, s16 offset)
{
  EncodeDForm(OP_LFD, DecodeReg(frt), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::LFSX(FPR frt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(frt), DecodeReg(ra), DecodeReg(rb), 535);
}

void PPC64XEmitter::LFDX(FPR frt, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(frt), DecodeReg(ra), DecodeReg(rb), 599);
}

void PPC64XEmitter::STFS(FPR frs, GPR ra, s16 offset)
{
  EncodeDForm(OP_STFS, DecodeReg(frs), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::STFD(FPR frs, GPR ra, s16 offset)
{
  EncodeDForm(OP_STFD, DecodeReg(frs), DecodeReg(ra), static_cast<u16>(offset));
}

void PPC64XEmitter::STFSX(FPR frs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(frs), DecodeReg(ra), DecodeReg(rb), 663);
}

void PPC64XEmitter::STFDX(FPR frs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(frs), DecodeReg(ra), DecodeReg(rb), 727);
}

void PPC64XEmitter::STFIWX(FPR frs, GPR ra, GPR rb)
{
  EncodeXForm(OP_X, DecodeReg(frs), DecodeReg(ra), DecodeReg(rb), 983);
}

// Floating-point arithmetic

void PPC64XEmitter::FMR(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 72, rc);
}

void PPC64XEmitter::FNEG(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 40, rc);
}

void PPC64XEmitter::FABS(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 264, rc);
}

void PPC64XEmitter::FNABS(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 136, rc);
}

void PPC64XEmitter::FCPSGN(FPR frt, FPR fra, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), 8, rc);
}

void PPC64XEmitter::FADD(FPR frt, FPR fra, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), 0, 21, rc);
}

void PPC64XEmitter::FADDS(FPR frt, FPR fra, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), 0, 21, rc);
}

void PPC64XEmitter::FSUB(FPR frt, FPR fra, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), 0, 20, rc);
}

void PPC64XEmitter::FSUBS(FPR frt, FPR fra, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), 0, 20, rc);
}

void PPC64XEmitter::FMUL(FPR frt, FPR fra, FPR frc, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), 0, DecodeReg(frc), 25, rc);
}

void PPC64XEmitter::FMULS(FPR frt, FPR fra, FPR frc, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), DecodeReg(fra), 0, DecodeReg(frc), 25, rc);
}

void PPC64XEmitter::FDIV(FPR frt, FPR fra, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), 0, 18, rc);
}

void PPC64XEmitter::FDIVS(FPR frt, FPR fra, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), 0, 18, rc);
}

void PPC64XEmitter::FMADD(FPR frt, FPR fra, FPR frc, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), DecodeReg(frc), 29, rc);
}

void PPC64XEmitter::FMADDS(FPR frt, FPR fra, FPR frc, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), DecodeReg(frc), 29, rc);
}

void PPC64XEmitter::FMSUB(FPR frt, FPR fra, FPR frc, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), DecodeReg(frc), 28, rc);
}

void PPC64XEmitter::FMSUBS(FPR frt, FPR fra, FPR frc, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), DecodeReg(frc), 28, rc);
}

void PPC64XEmitter::FNMADD(FPR frt, FPR fra, FPR frc, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), DecodeReg(frc), 31, rc);
}

void PPC64XEmitter::FNMADDS(FPR frt, FPR fra, FPR frc, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), DecodeReg(frc), 31, rc);
}

void PPC64XEmitter::FNMSUB(FPR frt, FPR fra, FPR frc, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), DecodeReg(frc), 30, rc);
}

void PPC64XEmitter::FNMSUBS(FPR frt, FPR fra, FPR frc, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), DecodeReg(frc), 30, rc);
}

void PPC64XEmitter::FSQRT(FPR frt, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 0, 22, rc);
}

void PPC64XEmitter::FSQRTS(FPR frt, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), 0, DecodeReg(frb), 0, 22, rc);
}

void PPC64XEmitter::FRES(FPR frt, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_SINGLE, DecodeReg(frt), 0, DecodeReg(frb), 0, 24, rc);
}

void PPC64XEmitter::FRSQRTE(FPR frt, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 0, 26, rc);
}

void PPC64XEmitter::FSEL(FPR frt, FPR fra, FPR frc, FPR frb, bool rc)
{
  EncodeAForm(OP_FP_DOUBLE, DecodeReg(frt), DecodeReg(fra), DecodeReg(frb), DecodeReg(frc), 23, rc);
}

void PPC64XEmitter::FRSP(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 12, rc);
}

void PPC64XEmitter::FCTIW(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 14, rc);
}

void PPC64XEmitter::FCTIWZ(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 15, rc);
}

void PPC64XEmitter::FCTID(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 814, rc);
}

void PPC64XEmitter::FCTIDZ(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 815, rc);
}

void PPC64XEmitter::FCFID(FPR frt, FPR frb, bool rc)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, DecodeReg(frb), 846, rc);
}

void PPC64XEmitter::FCMPU(CRF bf, FPR fra, FPR frb)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(bf) << 2, DecodeReg(fra), DecodeReg(frb), 0);
}

void PPC64XEmitter::FCMPO(CRF bf, FPR fra, FPR frb)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(bf) << 2, DecodeReg(fra), DecodeReg(frb), 32);
}

// Floating-point status and control register

void PPC64XEmitter::MFFS(FPR frt)
{
  EncodeXForm(OP_FP_DOUBLE, DecodeReg(frt), 0, 0, 583);
}

void PPC64XEmitter::MTFSF(u32 mask, FPR frb)
{
  ASSERT(mask < 0x100);
  Write32((OP_FP_DOUBLE << 26) | (mask << 17) | (DecodeReg(frb) << 11) | (711 << 1));
}

void PPC64XEmitter::MTFSFI(u32 field, u32 imm)
{
  ASSERT(field < 8 && imm < 16);
  Write32((OP_FP_DOUBLE << 26) | (field << 23) | (imm << 12) | (134 << 1));
}

void PPC64XEmitter::MTFSB0(u32 bt)
{
  ASSERT(bt < 32);
  EncodeXForm(OP_FP_DOUBLE, bt, 0, 0, 70);
}

void PPC64XEmitter::MTFSB1(u32 bt)
{
  ASSERT(bt < 32);
  EncodeXForm(OP_FP_DOUBLE, bt, 0, 0, 38);
}

// VSX loads and stores

void PPC64XEmitter::LXSDX(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 588);
}

void PPC64XEmitter::LXSSPX(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 524);
}

void PPC64XEmitter::LXSIWZX(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 12);
}

void PPC64XEmitter::LXSIWAX(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 76);
}

void PPC64XEmitter::LXVD2X(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 844);
}

void PPC64XEmitter::LXVW4X(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 780);
}

void PPC64XEmitter::LXVDSX(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 332);
}

void PPC64XEmitter::STXSDX(VSR xs, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xs), ra, rb, 716);
}

void PPC64XEmitter::STXSSPX(VSR xs, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xs), ra, rb, 652);
}

void PPC64XEmitter::STXSIWX(VSR xs, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xs), ra, rb, 140);
}

void PPC64XEmitter::STXVD2X(VSR xs, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xs), ra, rb, 972);
}

void PPC64XEmitter::STXVW4X(VSR xs, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xs), ra, rb, 908);
}

void PPC64XEmitter::LXV(VSR xt, GPR ra, s16 dq)
{
  ASSERT_MSG(DYNA_REC, (dq & 15) == 0, "DQ-form offset {} is not a multiple of 16", dq);
  const u32 t = DecodeReg(xt);
  Write32((OP_LXV << 26) | ((t & 0x1F) << 21) | (DecodeReg(ra) << 16) |
          (static_cast<u32>(dq) & 0xFFF0) | ((t >> 5) << 3) | 1);
}

void PPC64XEmitter::STXV(VSR xs, GPR ra, s16 dq)
{
  ASSERT_MSG(DYNA_REC, (dq & 15) == 0, "DQ-form offset {} is not a multiple of 16", dq);
  const u32 s = DecodeReg(xs);
  Write32((OP_LXV << 26) | ((s & 0x1F) << 21) | (DecodeReg(ra) << 16) |
          (static_cast<u32>(dq) & 0xFFF0) | ((s >> 5) << 3) | 5);
}

void PPC64XEmitter::LXVX(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 268);
}

void PPC64XEmitter::STXVX(VSR xs, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xs), ra, rb, 396);
}

void PPC64XEmitter::LXVWSX(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 364);
}

static u32 VRNumber(VSR reg)
{
  ASSERT_MSG(DYNA_REC, DecodeReg(reg) >= 32, "vs{} is not a VR", DecodeReg(reg));
  return DecodeReg(reg) - 32;
}

void PPC64XEmitter::LXSD(VSR vrt, GPR ra, s16 ds)
{
  EncodeDSForm(OP_LXSD, VRNumber(vrt), DecodeReg(ra), ds, 2);
}

void PPC64XEmitter::STXSD(VSR vrs, GPR ra, s16 ds)
{
  EncodeDSForm(OP_LXV, VRNumber(vrs), DecodeReg(ra), ds, 2);
}

void PPC64XEmitter::LXSSP(VSR vrt, GPR ra, s16 ds)
{
  EncodeDSForm(OP_LXSD, VRNumber(vrt), DecodeReg(ra), ds, 3);
}

void PPC64XEmitter::STXSSP(VSR vrs, GPR ra, s16 ds)
{
  EncodeDSForm(OP_LXV, VRNumber(vrs), DecodeReg(ra), ds, 3);
}

// Moves between GPRs and VSX registers

void PPC64XEmitter::MTVSRD(VSR xt, GPR ra)
{
  EncodeXX1Form(DecodeReg(xt), ra, GPR::R0, 179);
}

void PPC64XEmitter::MTVSRWZ(VSR xt, GPR ra)
{
  EncodeXX1Form(DecodeReg(xt), ra, GPR::R0, 243);
}

void PPC64XEmitter::MTVSRWA(VSR xt, GPR ra)
{
  EncodeXX1Form(DecodeReg(xt), ra, GPR::R0, 211);
}

void PPC64XEmitter::MFVSRD(GPR ra, VSR xs)
{
  EncodeXX1Form(DecodeReg(xs), ra, GPR::R0, 51);
}

void PPC64XEmitter::MFVSRWZ(GPR ra, VSR xs)
{
  EncodeXX1Form(DecodeReg(xs), ra, GPR::R0, 115);
}

void PPC64XEmitter::MTVSRWS(VSR xt, GPR ra)
{
  EncodeXX1Form(DecodeReg(xt), ra, GPR::R0, 403);
}

void PPC64XEmitter::MTVSRDD(VSR xt, GPR ra, GPR rb)
{
  EncodeXX1Form(DecodeReg(xt), ra, rb, 435);
}

void PPC64XEmitter::MFVSRLD(GPR ra, VSR xs)
{
  EncodeXX1Form(DecodeReg(xs), ra, GPR::R0, 307);
}

// VSX permutes and logical operations

void PPC64XEmitter::XXPERMDI(VSR xt, VSR xa, VSR xb, u32 dm)
{
  ASSERT(dm < 4);
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), (dm << 5) | 10);
}

void PPC64XEmitter::XXSLDWI(VSR xt, VSR xa, VSR xb, u32 shw)
{
  ASSERT(shw < 4);
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), (shw << 5) | 2);
}

void PPC64XEmitter::XXSPLTW(VSR xt, VSR xb, u32 uim)
{
  ASSERT(uim < 4);
  const u32 t = DecodeReg(xt);
  const u32 b = DecodeReg(xb);
  Write32((OP_VSX << 26) | ((t & 0x1F) << 21) | (uim << 16) | ((b & 0x1F) << 11) | (164 << 2) |
          ((b >> 5) << 1) | (t >> 5));
}

void PPC64XEmitter::XXSEL(VSR xt, VSR xa, VSR xb, VSR xc)
{
  // XX4-form
  const u32 t = DecodeReg(xt);
  const u32 a = DecodeReg(xa);
  const u32 b = DecodeReg(xb);
  const u32 c = DecodeReg(xc);
  Write32((OP_VSX << 26) | ((t & 0x1F) << 21) | ((a & 0x1F) << 16) | ((b & 0x1F) << 11) |
          ((c & 0x1F) << 6) | (3 << 4) | ((c >> 5) << 3) | ((a >> 5) << 2) | ((b >> 5) << 1) |
          (t >> 5));
}

void PPC64XEmitter::XXLAND(VSR xt, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), 130);
}

void PPC64XEmitter::XXLANDC(VSR xt, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), 138);
}

void PPC64XEmitter::XXLOR(VSR xt, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), 146);
}

void PPC64XEmitter::XXLORC(VSR xt, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), 170);
}

void PPC64XEmitter::XXLXOR(VSR xt, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), 154);
}

void PPC64XEmitter::XXLNOR(VSR xt, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), 162);
}

void PPC64XEmitter::XXLNAND(VSR xt, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), 178);
}

void PPC64XEmitter::XXLEQV(VSR xt, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), 186);
}

static u32 EncodeXXBR(u32 t, u32 b, u32 kind)
{
  return (60u << 26) | ((t & 0x1F) << 21) | (kind << 16) | ((b & 0x1F) << 11) | (475 << 2) |
         ((b >> 5) << 1) | (t >> 5);
}

void PPC64XEmitter::XXBRH(VSR xt, VSR xb)
{
  Write32(EncodeXXBR(DecodeReg(xt), DecodeReg(xb), 7));
}

void PPC64XEmitter::XXBRW(VSR xt, VSR xb)
{
  Write32(EncodeXXBR(DecodeReg(xt), DecodeReg(xb), 15));
}

void PPC64XEmitter::XXBRD(VSR xt, VSR xb)
{
  Write32(EncodeXXBR(DecodeReg(xt), DecodeReg(xb), 23));
}

void PPC64XEmitter::XXBRQ(VSR xt, VSR xb)
{
  Write32(EncodeXXBR(DecodeReg(xt), DecodeReg(xb), 31));
}

void PPC64XEmitter::XXSPLTIB(VSR xt, u8 imm)
{
  const u32 t = DecodeReg(xt);
  Write32((OP_VSX << 26) | ((t & 0x1F) << 21) | (u32{imm} << 11) | (360 << 1) | (t >> 5));
}

void PPC64XEmitter::XXSPLTIW(VSR xt, u32 imm)
{
  const u32 t = DecodeReg(xt);
  WritePrefixed(0x05000000 | (imm >> 16),
                0x80000000 | ((t & 0x1F) << 21) | (3 << 17) | ((t >> 5) << 16) | (imm & 0xFFFF));
}

void PPC64XEmitter::XXSPLTIDP(VSR xt, u32 single_bits)
{
  const u32 t = DecodeReg(xt);
  WritePrefixed(0x05000000 | (single_bits >> 16), 0x80000000 | ((t & 0x1F) << 21) | (2 << 17) |
                                                      ((t >> 5) << 16) | (single_bits & 0xFFFF));
}

// VSX scalar floating-point

#define XX3_OP(name, xo)                                                                           \
  void PPC64XEmitter::name(VSR xt, VSR xa, VSR xb)                                                 \
  {                                                                                                \
    EncodeXX3Form(DecodeReg(xt), DecodeReg(xa), DecodeReg(xb), xo);                                \
  }
#define XX2_OP(name, xo)                                                                           \
  void PPC64XEmitter::name(VSR xt, VSR xb)                                                         \
  {                                                                                                \
    EncodeXX2Form(DecodeReg(xt), DecodeReg(xb), xo);                                               \
  }

XX3_OP(XSADDDP, 32)
XX3_OP(XSSUBDP, 40)
XX3_OP(XSMULDP, 48)
XX3_OP(XSDIVDP, 56)
XX3_OP(XSADDSP, 0)
XX3_OP(XSSUBSP, 8)
XX3_OP(XSMULSP, 16)
XX3_OP(XSDIVSP, 24)
XX3_OP(XSMADDADP, 33)
XX3_OP(XSMADDMDP, 41)
XX3_OP(XSMSUBADP, 49)
XX3_OP(XSMSUBMDP, 57)
XX3_OP(XSNMADDADP, 161)
XX3_OP(XSNMADDMDP, 169)
XX3_OP(XSNMSUBADP, 177)
XX3_OP(XSNMSUBMDP, 185)
XX3_OP(XSMADDASP, 1)
XX3_OP(XSMADDMSP, 9)
XX3_OP(XSMSUBASP, 17)
XX3_OP(XSMSUBMSP, 25)
XX3_OP(XSNMADDASP, 129)
XX3_OP(XSNMADDMSP, 137)
XX3_OP(XSNMSUBASP, 145)
XX3_OP(XSNMSUBMSP, 153)
XX3_OP(XSMAXDP, 160)
XX3_OP(XSMINDP, 168)
XX3_OP(XSCPSGNDP, 176)

void PPC64XEmitter::XSCMPUDP(CRF bf, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(bf) << 2, DecodeReg(xa), DecodeReg(xb), 35);
}

void PPC64XEmitter::XSCMPODP(CRF bf, VSR xa, VSR xb)
{
  EncodeXX3Form(DecodeReg(bf) << 2, DecodeReg(xa), DecodeReg(xb), 43);
}

XX2_OP(XSSQRTDP, 75)
XX2_OP(XSABSDP, 345)
XX2_OP(XSNABSDP, 361)
XX2_OP(XSNEGDP, 377)
XX2_OP(XSREDP, 90)
XX2_OP(XSRSQRTEDP, 74)
XX2_OP(XSRSP, 281)
XX2_OP(XSCVDPSP, 265)
XX2_OP(XSCVDPSPN, 267)
XX2_OP(XSCVSPDP, 329)
XX2_OP(XSCVSPDPN, 331)
XX2_OP(XSCVDPSXWS, 88)
XX2_OP(XSCVDPUXWS, 72)
XX2_OP(XSCVDPSXDS, 344)
XX2_OP(XSCVSXDDP, 376)
XX2_OP(XSCVUXDDP, 360)
XX2_OP(XSRDPI, 73)
XX2_OP(XSRDPIZ, 89)

// VSX vector floating-point

XX3_OP(XVADDDP, 96)
XX3_OP(XVSUBDP, 104)
XX3_OP(XVMULDP, 112)
XX3_OP(XVDIVDP, 120)
XX3_OP(XVMADDADP, 97)
XX3_OP(XVMADDMDP, 105)
XX3_OP(XVMSUBADP, 113)
XX3_OP(XVMSUBMDP, 121)
XX3_OP(XVNMADDADP, 225)
XX3_OP(XVNMADDMDP, 233)
XX3_OP(XVNMSUBADP, 241)
XX3_OP(XVNMSUBMDP, 249)
XX3_OP(XVMAXDP, 224)
XX3_OP(XVMINDP, 232)
XX3_OP(XVCPSGNDP, 240)
XX3_OP(XVCMPEQDP, 99)
XX3_OP(XVCMPGEDP, 115)
XX3_OP(XVCMPGTDP, 107)

XX2_OP(XVSQRTDP, 203)
XX2_OP(XVABSDP, 473)
XX2_OP(XVNABSDP, 489)
XX2_OP(XVNEGDP, 505)
XX2_OP(XVREDP, 218)
XX2_OP(XVRSQRTEDP, 202)
XX2_OP(XVCVDPSP, 393)
XX2_OP(XVCVSPDP, 457)
XX2_OP(XVCVSXDDP, 504)
XX2_OP(XVCVDPSXWS, 216)

#undef XX3_OP
#undef XX2_OP

// Vector status and control register

void PPC64XEmitter::MFVSCR(VR vrt)
{
  Write32((OP_VMX << 26) | (DecodeReg(vrt) << 21) | 1540);
}

void PPC64XEmitter::VSLD(VSR vrt, VSR vra, VSR vrb)
{
  Write32((OP_VMX << 26) | (VRNumber(vrt) << 21) | (VRNumber(vra) << 16) | (VRNumber(vrb) << 11) |
          1476);
}

void PPC64XEmitter::VCMPEQUD(VSR vrt, VSR vra, VSR vrb, bool rc)
{
  Write32((OP_VMX << 26) | (VRNumber(vrt) << 21) | (VRNumber(vra) << 16) | (VRNumber(vrb) << 11) |
          (u32{rc} << 10) | 199);
}

void PPC64XEmitter::VMAXSW(VSR vrt, VSR vra, VSR vrb)
{
  Write32((OP_VMX << 26) | (VRNumber(vrt) << 21) | (VRNumber(vra) << 16) | (VRNumber(vrb) << 11) |
          386);
}

void PPC64XEmitter::VMINSW(VSR vrt, VSR vra, VSR vrb)
{
  Write32((OP_VMX << 26) | (VRNumber(vrt) << 21) | (VRNumber(vra) << 16) | (VRNumber(vrb) << 11) |
          898);
}

void PPC64XEmitter::MTVSCR(VR vrb)
{
  Write32((OP_VMX << 26) | (DecodeReg(vrb) << 11) | 1604);
}

// Constants and calls

void PPC64XEmitter::XSTSTDCDP(CRF bf, VSR xb, u32 dcmx)
{
  ASSERT(dcmx < 128);
  const u32 b = DecodeReg(xb);
  Write32((OP_VSX << 26) | (DecodeReg(bf) << 23) | (dcmx << 16) | ((b & 0x1F) << 11) | (362 << 2) |
          ((b >> 5) << 1));
}

void PPC64XEmitter::XVTSTDCSP(VSR xt, VSR xb, u32 dcmx)
{
  // DCMX is split into dc (bit 6), dm (bit 5) and dx (bits 0-4).
  ASSERT(dcmx < 128);
  const u32 t = DecodeReg(xt);
  const u32 b = DecodeReg(xb);
  Write32((OP_VSX << 26) | ((t & 0x1F) << 21) | ((dcmx & 0x1F) << 16) | ((b & 0x1F) << 11) |
          (13 << 7) | (((dcmx >> 6) & 1) << 6) | (5 << 3) | (((dcmx >> 5) & 1) << 2) |
          ((b >> 5) << 1) | (t >> 5));
}

void PPC64XEmitter::XSXEXPDP(GPR rt, VSR xb)
{
  const u32 b = DecodeReg(xb);
  Write32((OP_VSX << 26) | (DecodeReg(rt) << 21) | ((b & 0x1F) << 11) | (347 << 2) |
          ((b >> 5) << 1));
}

void PPC64XEmitter::XSXSIGDP(GPR rt, VSR xb)
{
  const u32 b = DecodeReg(xb);
  Write32((OP_VSX << 26) | (DecodeReg(rt) << 21) | (1 << 16) | ((b & 0x1F) << 11) | (347 << 2) |
          ((b >> 5) << 1));
}

void PPC64XEmitter::PLI(GPR rd, s64 imm)
{
  ASSERT_MSG(DYNA_REC, IsInPrefixedRange(imm), "pli value {} out of range", imm);
  const u64 bits = static_cast<u64>(imm);
  WritePrefixed(0x06000000 | ((bits >> 16) & 0x3FFFF),
                (OP_ADDI << 26) | (DecodeReg(rd) << 21) | (bits & 0xFFFF));
}

void PPC64XEmitter::PLA(GPR rd, const void* target)
{
  AlignForPrefixed();
  const s64 offset = static_cast<const u8*>(target) - m_code;
  ASSERT_MSG(DYNA_REC, IsInPrefixedRange(offset), "pla offset {} out of range", offset);
  const u64 bits = static_cast<u64>(offset);
  WritePrefixed(0x06100000 | ((bits >> 16) & 0x3FFFF),
                (OP_ADDI << 26) | (DecodeReg(rd) << 21) | (bits & 0xFFFF));
}

void PPC64XEmitter::PLD(GPR rd, const void* target)
{
  AlignForPrefixed();
  const s64 offset = static_cast<const u8*>(target) - m_code;
  ASSERT_MSG(DYNA_REC, IsInPrefixedRange(offset), "pld offset {} out of range", offset);
  const u64 bits = static_cast<u64>(offset);
  WritePrefixed(0x04100000 | ((bits >> 16) & 0x3FFFF),
                (OP_LXSD << 26) | (DecodeReg(rd) << 21) | (bits & 0xFFFF));
}

void PPC64XEmitter::MOVI2R(GPR rd, u64 imm)
{
  const s64 simm = static_cast<s64>(imm);

  if (simm == static_cast<s16>(simm))
  {
    LI(rd, static_cast<s16>(simm));
    return;
  }

  // Power10 loads any value of 34 bits, signed, in one instruction, and others from the code's
  // constant pool if it has one.
  if (m_prefixed_instructions && simm != static_cast<s32>(simm))
  {
    if (IsInPrefixedRange(simm))
    {
      PLI(rd, simm);
      return;
    }
    if (LoadConstantFromPool(rd, imm))
      return;
  }
  else if (m_prefixed_instructions && (imm & 0xFFFF) != 0)
  {
    // Instead of lis and ori.
    PLI(rd, simm);
    return;
  }

  if (simm == static_cast<s32>(simm))
  {
    LIS(rd, static_cast<s16>(simm >> 16));
    if (imm & 0xFFFF)
      ORI(rd, rd, static_cast<u16>(imm));
    return;
  }

  if ((imm >> 32) == 0)
  {
    // A 32-bit value with the top bit set: build it sign-extended, then clear the upper word.
    LIS(rd, static_cast<s16>(imm >> 16));
    if (imm & 0xFFFF)
      ORI(rd, rd, static_cast<u16>(imm));
    CLRLDI(rd, rd, 32);
    return;
  }

  // Build the upper word, shift it into place, then fill in the lower word.
  MOVI2R(rd, static_cast<u64>(static_cast<s64>(static_cast<s32>(imm >> 32))));
  SLDI(rd, rd, 32);
  if ((imm >> 16) & 0xFFFF)
    ORIS(rd, rd, static_cast<u16>(imm >> 16));
  if (imm & 0xFFFF)
    ORI(rd, rd, static_cast<u16>(imm));
}

void PPC64XEmitter::QuickCallFunction(const void* func)
{
  MOVP2R(GPR::R12, func);
  MTCTR(GPR::R12);
  BCTRL();
}
}  // namespace PPC64Gen
