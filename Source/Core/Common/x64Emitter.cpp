// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/x64Emitter.h"

#include <cstring>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/x64Reg.h"

namespace Gen
{
struct NormalOpDef
{
  u8 toRm8, toRm32, fromRm8, fromRm32, imm8, imm32, simm8, eaximm8, eaximm32, ext;
};

// 0xCC is code for invalid combination of immediates
static const NormalOpDef normalops[11] = {
    {0x00, 0x01, 0x02, 0x03, 0x80, 0x81, 0x83, 0x04, 0x05, 0},  // ADD
    {0x10, 0x11, 0x12, 0x13, 0x80, 0x81, 0x83, 0x14, 0x15, 2},  // ADC

    {0x28, 0x29, 0x2A, 0x2B, 0x80, 0x81, 0x83, 0x2C, 0x2D, 5},  // SUB
    {0x18, 0x19, 0x1A, 0x1B, 0x80, 0x81, 0x83, 0x1C, 0x1D, 3},  // SBB

    {0x20, 0x21, 0x22, 0x23, 0x80, 0x81, 0x83, 0x24, 0x25, 4},  // AND
    {0x08, 0x09, 0x0A, 0x0B, 0x80, 0x81, 0x83, 0x0C, 0x0D, 1},  // OR

    {0x30, 0x31, 0x32, 0x33, 0x80, 0x81, 0x83, 0x34, 0x35, 6},  // XOR
    {0x88, 0x89, 0x8A, 0x8B, 0xC6, 0xC7, 0xCC, 0xCC, 0xCC, 0},  // MOV

    {0x84, 0x85, 0x84, 0x85, 0xF6, 0xF7, 0xCC, 0xA8, 0xA9, 0},  // TEST (to == from)
    {0x38, 0x39, 0x3A, 0x3B, 0x80, 0x81, 0x83, 0x3C, 0x3D, 7},  // CMP

    {0x86, 0x87, 0x86, 0x87, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 7},  // XCHG
};

enum NormalSSEOps
{
  sseCMP = 0xC2,
  sseADD = 0x58,   // ADD
  sseSUB = 0x5C,   // SUB
  sseAND = 0x54,   // AND
  sseANDN = 0x55,  // ANDN
  sseOR = 0x56,
  sseXOR = 0x57,
  sseMUL = 0x59,          // MUL
  sseDIV = 0x5E,          // DIV
  sseMIN = 0x5D,          // MIN
  sseMAX = 0x5F,          // MAX
  sseCOMIS = 0x2F,        // COMIS
  sseUCOMIS = 0x2E,       // UCOMIS
  sseSQRT = 0x51,         // SQRT
  sseRCP = 0x53,          // RCP
  sseRSQRT = 0x52,        // RSQRT (NO DOUBLE PRECISION!!!)
  sseMOVAPfromRM = 0x28,  // MOVAP from RM
  sseMOVAPtoRM = 0x29,    // MOVAP to RM
  sseMOVUPfromRM = 0x10,  // MOVUP from RM
  sseMOVUPtoRM = 0x11,    // MOVUP to RM
  sseMOVLPfromRM = 0x12,
  sseMOVLPtoRM = 0x13,
  sseMOVHPfromRM = 0x16,
  sseMOVHPtoRM = 0x17,
  sseMOVHLPS = 0x12,
  sseMOVLHPS = 0x16,
  sseMOVDQfromRM = 0x6F,
  sseMOVDQtoRM = 0x7F,
  sseMASKMOVDQU = 0xF7,
  sseLDDQU = 0xF0,
  sseSHUF = 0xC6,
  sseMOVNTDQ = 0xE7,
  sseMOVNTP = 0x2B,
};

enum class NormalOp
{
  ADD,
  ADC,
  SUB,
  SBB,
  AND,
  OR,
  XOR,
  MOV,
  TEST,
  CMP,
  XCHG,
};

enum class FloatOp
{
  LD = 0,
  ST = 2,
  STP = 3,
  LD80 = 5,
  STP80 = 7,

  Invalid = -1,
};

void XEmitter::SetCodePtr(u8* ptr, u8* end, bool write_failed)
{
  code = ptr;
  m_code_end = end;
  m_write_failed = write_failed;
}

const u8* XEmitter::GetCodePtr() const
{
  return code;
}

u8* XEmitter::GetWritableCodePtr()
{
  return code;
}

const u8* XEmitter::GetCodeEnd() const
{
  return m_code_end;
}

u8* XEmitter::GetWritableCodeEnd()
{
  return m_code_end;
}

void XEmitter::Write8(u8 value)
{
  if (code >= m_code_end)
  {
    code = m_code_end;
    m_write_failed = true;
    return;
  }

  *code++ = value;
}

void XEmitter::Write16(u16 value)
{
  if (code + sizeof(u16) > m_code_end)
  {
    code = m_code_end;
    m_write_failed = true;
    return;
  }

  std::memcpy(code, &value, sizeof(u16));
  code += sizeof(u16);
}

void XEmitter::Write32(u32 value)
{
  if (code + sizeof(u32) > m_code_end)
  {
    code = m_code_end;
    m_write_failed = true;
    return;
  }

  std::memcpy(code, &value, sizeof(u32));
  code += sizeof(u32);
}

void XEmitter::Write64(u64 value)
{
  if (code + sizeof(u64) > m_code_end)
  {
    code = m_code_end;
    m_write_failed = true;
    return;
  }

  std::memcpy(code, &value, sizeof(u64));
  code += sizeof(u64);
}

void XEmitter::ReserveCodeSpace(int bytes)
{
  if (code + bytes > m_code_end)
  {
    code = m_code_end;
    m_write_failed = true;
    return;
  }

  for (int i = 0; i < bytes; i++)
    *code++ = 0xCC;
}

u8* XEmitter::AlignCodeTo(size_t alignment)
{
  ASSERT_MSG(DYNA_REC, alignment != 0 && (alignment & (alignment - 1)) == 0,
             "Alignment must be power of two");
  u64 c = reinterpret_cast<u64>(code) & (alignment - 1);
  if (c)
    ReserveCodeSpace(static_cast<int>(alignment - c));
  return code;
}

u8* XEmitter::AlignCode4()
{
  return AlignCodeTo(4);
}

u8* XEmitter::AlignCode16()
{
  return AlignCodeTo(16);
}

u8* XEmitter::AlignCodePage()
{
  return AlignCodeTo(4096);
}

// This operation modifies flags; check to see the flags are locked.
// If the flags are locked, we should immediately and loudly fail before
// causing a subtle JIT bug.
void XEmitter::CheckFlags()
{
  ASSERT_MSG(DYNA_REC, !flags_locked, "Attempt to modify flags while flags locked!");
}

void XEmitter::WriteModRM(int mod, int reg, int rm)
{
  Write8((u8)((mod << 6) | ((reg & 7) << 3) | (rm & 7)));
}

void XEmitter::WriteSIB(int scale, int index, int base)
{
  Write8((u8)((scale << 6) | ((index & 7) << 3) | (base & 7)));
}

void OpArg::WriteREX(XEmitter* emit, int opBits, int bits, int customOp) const
{
  if (customOp == -1)
    customOp = operandReg;
  u8 op = 0x40;
  // REX.W (whether operation is a 64-bit operation)
  if (opBits == 64)
    op |= 8;
  // REX.R (whether ModR/M reg field refers to R8-R15.
  if (customOp & 8)
    op |= 4;
  // REX.X (whether ModR/M SIB index field refers to R8-R15)
  if (indexReg & 8)
    op |= 2;
  // REX.B (whether ModR/M rm or SIB base or opcode reg field refers to R8-R15)
  if (offsetOrBaseReg & 8)
    op |= 1;
  // Write REX if wr have REX bits to write, or if the operation accesses
  // SIL, DIL, BPL, or SPL.
  if (op != 0x40 || (scale == SCALE_NONE && bits == 8 && (offsetOrBaseReg & 0x10c) == 4) ||
      (opBits == 8 && (customOp & 0x10c) == 4))
  {
    emit->Write8(op);
    // Check the operation doesn't access AH, BH, CH, or DH.
    DEBUG_ASSERT((offsetOrBaseReg & 0x100) == 0);
    DEBUG_ASSERT((customOp & 0x100) == 0);
  }
}

void OpArg::WriteVEX(XEmitter* emit, X64Reg regOp1, X64Reg regOp2, int L, int pp, int mmmmm,
                     int W) const
{
  int R = !(regOp1 & 8);
  int X = !(indexReg & 8);
  int B = !(offsetOrBaseReg & 8);

  int vvvv = (regOp2 == X64Reg::INVALID_REG) ? 0xf : (regOp2 ^ 0xf);

  // do we need any VEX fields that only appear in the three-byte form?
  if (X == 1 && B == 1 && W == 0 && mmmmm == 1)
  {
    u8 RvvvvLpp = (R << 7) | (vvvv << 3) | (L << 2) | pp;
    emit->Write8(0xC5);
    emit->Write8(RvvvvLpp);
  }
  else
  {
    u8 RXBmmmmm = (R << 7) | (X << 6) | (B << 5) | mmmmm;
    u8 WvvvvLpp = (W << 7) | (vvvv << 3) | (L << 2) | pp;
    emit->Write8(0xC4);
    emit->Write8(RXBmmmmm);
    emit->Write8(WvvvvLpp);
  }
}

void OpArg::WriteRest(XEmitter* emit, int extraBytes, X64Reg _operandReg,
                      bool warn_64bit_offset) const
{
  if (_operandReg == INVALID_REG)
    _operandReg = (X64Reg)this->operandReg;
  int mod = 0;
  int ireg = indexReg;
  bool SIB = false;
  int _offsetOrBaseReg = this->offsetOrBaseReg;

  if (scale == SCALE_RIP)  // Also, on 32-bit, just an immediate address
  {
    // Oh, RIP addressing.
    _offsetOrBaseReg = 5;
    emit->WriteModRM(0, _operandReg, _offsetOrBaseReg);
    // TODO : add some checks
    u64 ripAddr = (u64)emit->GetCodePtr() + 4 + extraBytes;
    s64 distance = (s64)offset - (s64)ripAddr;
    ASSERT_MSG(DYNA_REC,
               (distance < 0x80000000LL && distance >= -0x80000000LL) || !warn_64bit_offset,
               "WriteRest: op out of range ({:#x} uses {:#x})", ripAddr, offset);
    s32 offs = (s32)distance;
    emit->Write32((u32)offs);
    return;
  }

  if (scale == SCALE_NONE)
  {
    // Oh, no memory, Just a reg.
    mod = 3;  // 11
  }
  else if (scale >= SCALE_NOBASE_2 && scale <= SCALE_NOBASE_8)
  {
    SIB = true;
    mod = 0;
    _offsetOrBaseReg = 5;
    // Always has 32-bit displacement
  }
  else
  {
    if (scale != SCALE_ATREG)
    {
      SIB = true;
    }
    else if ((_offsetOrBaseReg & 7) == 4)
    {
      // Special case for which SCALE_ATREG needs SIB
      SIB = true;
      ireg = _offsetOrBaseReg;
    }

    // Okay, we're fine. Just disp encoding.
    // We need displacement. Which size?
    int ioff = (int)(s64)offset;
    if (ioff == 0 && (_offsetOrBaseReg & 7) != 5)
    {
      mod = 0;  // No displacement
    }
    else if (ioff >= -128 && ioff <= 127)
    {
      mod = 1;  // 8-bit displacement
    }
    else
    {
      mod = 2;  // 32-bit displacement
    }
  }

  // Okay. Time to do the actual writing
  // ModRM byte:
  int oreg = _offsetOrBaseReg;
  if (SIB)
    oreg = 4;

  emit->WriteModRM(mod, _operandReg & 7, oreg & 7);

  if (SIB)
  {
    // SIB byte
    int ss;
    switch (scale)
    {
    case SCALE_NONE:
      _offsetOrBaseReg = 4;
      ss = 0;
      break;  // RSP
    case SCALE_1:
      ss = 0;
      break;
    case SCALE_2:
      ss = 1;
      break;
    case SCALE_4:
      ss = 2;
      break;
    case SCALE_8:
      ss = 3;
      break;
    case SCALE_NOBASE_2:
      ss = 1;
      break;
    case SCALE_NOBASE_4:
      ss = 2;
      break;
    case SCALE_NOBASE_8:
      ss = 3;
      break;
    case SCALE_ATREG:
      ss = 0;
      break;
    default:
      ASSERT_MSG(DYNA_REC, 0, "Invalid scale for SIB byte");
      ss = 0;
      break;
    }
    emit->Write8((u8)((ss << 6) | ((ireg & 7) << 3) | (_offsetOrBaseReg & 7)));
  }

  if (mod == 1)  // 8-bit disp
  {
    emit->Write8((u8)(s8)(s32)offset);
  }
  else if (mod == 2 || (scale >= SCALE_NOBASE_2 && scale <= SCALE_NOBASE_8))  // 32-bit disp
  {
    emit->Write32((u32)offset);
  }
}

// W = operand extended width (1 if 64-bit)
// R = register# upper bit
// X = scale amnt upper bit
// B = base register# upper bit
void XEmitter::Rex(int w, int r, int x, int b)
{
  w = w ? 1 : 0;
  r = r ? 1 : 0;
  x = x ? 1 : 0;
  b = b ? 1 : 0;
  u8 rx = (u8)(0x40 | (w << 3) | (r << 2) | (x << 1) | (b));
  if (rx != 0x40)
    Write8(rx);
}

void XEmitter::JMP(const u8* addr, bool force5Bytes)
{
  u64 fn = (u64)addr;
  if (!force5Bytes)
  {
    s64 distance = (s64)(fn - ((u64)code + 2));
    ASSERT_MSG(DYNA_REC, distance >= -0x80 && distance < 0x80,
               "Jump target too far away ({}), needs force5Bytes = true", distance);
    // 8 bits will do
    Write8(0xEB);
    Write8((u8)(s8)distance);
  }
  else
  {
    s64 distance = (s64)(fn - ((u64)code + 5));

    ASSERT_MSG(DYNA_REC, distance >= -0x80000000LL && distance < 0x80000000LL,
               "Jump target too far away ({}), needs indirect register", distance);
    Write8(0xE9);
    Write32((u32)(s32)distance);
  }
}

void XEmitter::JMPptr(const OpArg& arg2)
{
  OpArg arg = arg2;
  if (arg.IsImm())
    ASSERT_MSG(DYNA_REC, 0, "JMPptr - Imm argument");
  arg.operandReg = 4;
  arg.WriteREX(this, 0, 0);
  Write8(0xFF);
  arg.WriteRest(this);
}

// Can be used to trap other processors, before overwriting their code
// not used in Dolphin
void XEmitter::JMPself()
{
  Write8(0xEB);
  Write8(0xFE);
}

void XEmitter::CALLptr(OpArg arg)
{
  if (arg.IsImm())
    ASSERT_MSG(DYNA_REC, 0, "CALLptr - Imm argument");
  arg.operandReg = 2;
  arg.WriteREX(this, 0, 0);
  Write8(0xFF);
  arg.WriteRest(this);
}

void XEmitter::CALL(const void* fnptr)
{
  u64 distance = u64(fnptr) - (u64(code) + 5);
  ASSERT_MSG(DYNA_REC, distance < 0x0000000080000000ULL || distance >= 0xFFFFFFFF80000000ULL,
             "CALL out of range ({} calls {})", fmt::ptr(code), fmt::ptr(fnptr));
  Write8(0xE8);
  Write32(u32(distance));
}

FixupBranch XEmitter::CALL()
{
  FixupBranch branch;
  branch.type = FixupBranch::Type::Branch32Bit;
  branch.ptr = code + 5;
  Write8(0xE8);
  Write32(0);

  // If we couldn't write the full call instruction, indicate that in the returned FixupBranch by
  // setting the branch's address to null. This will prevent a later SetJumpTarget() from writing to
  // invalid memory.
  if (HasWriteFailed())
    branch.ptr = nullptr;

  return branch;
}

FixupBranch XEmitter::J(bool force5bytes)
{
  FixupBranch branch;
  branch.type = force5bytes ? FixupBranch::Type::Branch32Bit : FixupBranch::Type::Branch8Bit;
  branch.ptr = code + (force5bytes ? 5 : 2);
  if (!force5bytes)
  {
    // 8 bits will do
    Write8(0xEB);
    Write8(0);
  }
  else
  {
    Write8(0xE9);
    Write32(0);
  }

  // If we couldn't write the full jump instruction, indicate that in the returned FixupBranch by
  // setting the branch's address to null. This will prevent a later SetJumpTarget() from writing to
  // invalid memory.
  if (HasWriteFailed())
    branch.ptr = nullptr;

  return branch;
}

FixupBranch XEmitter::J_CC(CCFlags conditionCode, bool force5bytes)
{
  FixupBranch branch;
  branch.type = force5bytes ? FixupBranch::Type::Branch32Bit : FixupBranch::Type::Branch8Bit;
  branch.ptr = code + (force5bytes ? 6 : 2);
  if (!force5bytes)
  {
    // 8 bits will do
    Write8(0x70 + conditionCode);
    Write8(0);
  }
  else
  {
    Write8(0x0F);
    Write8(0x80 + conditionCode);
    Write32(0);
  }

  // If we couldn't write the full jump instruction, indicate that in the returned FixupBranch by
  // setting the branch's address to null. This will prevent a later SetJumpTarget() from writing to
  // invalid memory.
  if (HasWriteFailed())
    branch.ptr = nullptr;

  return branch;
}

void XEmitter::J_CC(CCFlags conditionCode, const u8* addr)
{
  u64 fn = (u64)addr;
  s64 distance = (s64)(fn - ((u64)code + 2));
  if (distance < -0x80 || distance >= 0x80)
  {
    distance = (s64)(fn - ((u64)code + 6));
    ASSERT_MSG(DYNA_REC, distance >= -0x80000000LL && distance < 0x80000000LL,
               "Jump target too far away ({}), needs indirect register", distance);
    Write8(0x0F);
    Write8(0x80 + conditionCode);
    Write32((u32)(s32)distance);
  }
  else
  {
    Write8(0x70 + conditionCode);
    Write8((u8)(s8)distance);
  }
}

void XEmitter::SetJumpTarget(const FixupBranch& branch)
{
  if (!branch.ptr)
    return;

  if (branch.type == FixupBranch::Type::Branch8Bit)
  {
    s64 distance = (s64)(code - branch.ptr);
    ASSERT_MSG(DYNA_REC, distance >= -0x80 && distance < 0x80,
               "Jump target too far away ({}), needs force5Bytes = true", distance);
    branch.ptr[-1] = (u8)(s8)distance;
  }
  else if (branch.type == FixupBranch::Type::Branch32Bit)
  {
    s64 distance = (s64)(code - branch.ptr);
    ASSERT_MSG(DYNA_REC, distance >= -0x80000000LL && distance < 0x80000000LL,
               "Jump target too far away ({}), needs indirect register", distance);

    s32 valid_distance = static_cast<s32>(distance);
    std::memcpy(&branch.ptr[-4], &valid_distance, sizeof(s32));
  }
}

// Single byte opcodes
// There is no PUSHAD/POPAD in 64-bit mode.
void XEmitter::INT3()
{
  Write8(0xCC);
}
void XEmitter::RET()
{
  Write8(0xC3);
}
void XEmitter::RET_FAST()
{
  Write8(0xF3);
  Write8(0xC3);
}  // two-byte return (rep ret) - recommended by AMD optimization manual for the case of jumping to
   // a ret

// The first sign of decadence: optimized NOPs.
void XEmitter::NOP(size_t size)
{
  DEBUG_ASSERT((int)size > 0);
  while (true)
  {
    switch (size)
    {
    case 0:
      return;
    case 1:
      Write8(0x90);
      return;
    case 2:
      Write8(0x66);
      Write8(0x90);
      return;
    case 3:
      Write8(0x0F);
      Write8(0x1F);
      Write8(0x00);
      return;
    case 4:
      Write8(0x0F);
      Write8(0x1F);
      Write8(0x40);
      Write8(0x00);
      return;
    case 5:
      Write8(0x0F);
      Write8(0x1F);
      Write8(0x44);
      Write8(0x00);
      Write8(0x00);
      return;
    case 6:
      Write8(0x66);
      Write8(0x0F);
      Write8(0x1F);
      Write8(0x44);
      Write8(0x00);
      Write8(0x00);
      return;
    case 7:
      Write8(0x0F);
      Write8(0x1F);
      Write8(0x80);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      return;
    case 8:
      Write8(0x0F);
      Write8(0x1F);
      Write8(0x84);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      return;
    case 9:
      Write8(0x66);
      Write8(0x0F);
      Write8(0x1F);
      Write8(0x84);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      return;
    case 10:
      Write8(0x66);
      Write8(0x66);
      Write8(0x0F);
      Write8(0x1F);
      Write8(0x84);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      return;
    default:
      // Even though x86 instructions are allowed to be up to 15 bytes long,
      // AMD advises against using NOPs longer than 11 bytes because they
      // carry a performance penalty on CPUs older than AMD family 16h.
      Write8(0x66);
      Write8(0x66);
      Write8(0x66);
      Write8(0x0F);
      Write8(0x1F);
      Write8(0x84);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      Write8(0x00);
      size -= 11;
      continue;
    }
  }
}

void XEmitter::PAUSE()
{
  Write8(0xF3);
  NOP();
}  // use in tight spinloops for energy saving on some CPU
void XEmitter::CLC()
{
  CheckFlags();
  Write8(0xF8);
}  // clear carry
void XEmitter::CMC()
{
  CheckFlags();
  Write8(0xF5);
}  // flip carry
void XEmitter::STC()
{
  CheckFlags();
  Write8(0xF9);
}  // set carry

// TODO: xchg ah, al ???
void XEmitter::XCHG_AHAL()
{
  Write8(0x86);
  Write8(0xe0);
  // alt. 86 c4
}

// These two can not be executed on early Intel 64-bit CPU:s, only on AMD!
void XEmitter::LAHF()
{
  Write8(0x9F);
}
void XEmitter::SAHF()
{
  CheckFlags();
  Write8(0x9E);
}

void XEmitter::PUSHF()
{
  Write8(0x9C);
}
void XEmitter::POPF()
{
  CheckFlags();
  Write8(0x9D);
}

void XEmitter::LFENCE()
{
  Write8(0x0F);
  Write8(0xAE);
  Write8(0xE8);
}
void XEmitter::MFENCE()
{
  Write8(0x0F);
  Write8(0xAE);
  Write8(0xF0);
}
void XEmitter::SFENCE()
{
  Write8(0x0F);
  Write8(0xAE);
  Write8(0xF8);
}

void XEmitter::WriteSimple1Byte(int bits, u8 byte, X64Reg reg)
{
  if (bits == 16)
    Write8(0x66);
  Rex(bits == 64, 0, 0, (int)reg >> 3);
  Write8(byte + ((int)reg & 7));
}

void XEmitter::WriteSimple2Byte(int bits, u8 byte1, u8 byte2, X64Reg reg)
{
  if (bits == 16)
    Write8(0x66);
  Rex(bits == 64, 0, 0, (int)reg >> 3);
  Write8(byte1);
  Write8(byte2 + ((int)reg & 7));
}

void XEmitter::CWD(int bits)
{
  if (bits == 16)
    Write8(0x66);
  Rex(bits == 64, 0, 0, 0);
  Write8(0x99);
}

void XEmitter::CBW(int bits)
{
  if (bits == 8)
    Write8(0x66);
  Rex(bits == 32, 0, 0, 0);
  Write8(0x98);
}

// Simple opcodes

// push/pop do not need wide to be 64-bit
void XEmitter::PUSH(X64Reg reg)
{
  WriteSimple1Byte(32, 0x50, reg);
}
void XEmitter::POP(X64Reg reg)
{
  WriteSimple1Byte(32, 0x58, reg);
}

void XEmitter::PUSH(int bits, const OpArg& reg)
{
  if (reg.IsSimpleReg())
    PUSH(reg.GetSimpleReg());
  else if (reg.IsImm())
  {
    switch (reg.GetImmBits())
    {
    case 8:
      Write8(0x6A);
      Write8((u8)(s8)reg.offset);
      break;
    case 16:
      Write8(0x66);
      Write8(0x68);
      Write16((u16)(s16)(s32)reg.offset);
      break;
    case 32:
      Write8(0x68);
      Write32((u32)reg.offset);
      break;
    default:
      ASSERT_MSG(DYNA_REC, 0, "PUSH - Bad imm bits");
      break;
    }
  }
  else
  {
    if (bits == 16)
      Write8(0x66);
    reg.WriteREX(this, bits, bits);
    Write8(0xFF);
    reg.WriteRest(this, 0, (X64Reg)6);
  }
}

void XEmitter::POP(int /*bits*/, const OpArg& reg)
{
  if (reg.IsSimpleReg())
    POP(reg.GetSimpleReg());
  else
    ASSERT_MSG(DYNA_REC, 0, "POP - Unsupported encoding");
}

void XEmitter::BSWAP(int bits, X64Reg reg)
{
  if (bits >= 32)
  {
    WriteSimple2Byte(bits, 0x0F, 0xC8, reg);
  }
  else if (bits == 16)
  {
    ROL(16, R(reg), Imm8(8));
  }
  else if (bits == 8)
  {
    // Do nothing - can't bswap a single byte...
  }
  else
  {
    ASSERT_MSG(DYNA_REC, 0, "BSWAP - Wrong number of bits");
  }
}

// Undefined opcode - reserved
// If we ever need a way to always cause a non-breakpoint hard exception...
void XEmitter::UD2()
{
  Write8(0x0F);
  Write8(0x0B);
}

void XEmitter::PREFETCH(PrefetchLevel level, OpArg arg)
{
  ASSERT_MSG(DYNA_REC, !arg.IsImm(), "PREFETCH - Imm argument");
  arg.operandReg = (u8)level;
  arg.WriteREX(this, 0, 0);
  Write8(0x0F);
  Write8(0x18);
  arg.WriteRest(this);
}

void XEmitter::SETcc(CCFlags flag, OpArg dest)
{
  ASSERT_MSG(DYNA_REC, !dest.IsImm(), "SETcc - Imm argument");
  dest.operandReg = 0;
  dest.WriteREX(this, 0, 8);
  Write8(0x0F);
  Write8(0x90 + (u8)flag);
  dest.WriteRest(this);
}

void XEmitter::CMOVcc(int bits, X64Reg dest, OpArg src, CCFlags flag)
{
  ASSERT_MSG(DYNA_REC, !src.IsImm(), "CMOVcc - Imm argument");
  ASSERT_MSG(DYNA_REC, bits != 8, "CMOVcc - 8 bits unsupported");
  if (bits == 16)
    Write8(0x66);
  src.operandReg = dest;
  src.WriteREX(this, bits, bits);
  Write8(0x0F);
  Write8(0x40 + (u8)flag);
  src.WriteRest(this);
}

void XEmitter::WriteMulDivType(int bits, OpArg src, int ext)
{
  ASSERT_MSG(DYNA_REC, !src.IsImm(), "WriteMulDivType - Imm argument");
  CheckFlags();
  src.operandReg = ext;
  if (bits == 16)
    Write8(0x66);
  src.WriteREX(this, bits, bits, 0);
  if (bits == 8)
  {
    Write8(0xF6);
  }
  else
  {
    Write8(0xF7);
  }
  src.WriteRest(this);
}

void XEmitter::MUL(int bits, const OpArg& src)
{
  WriteMulDivType(bits, src, 4);
}
void XEmitter::DIV(int bits, const OpArg& src)
{
  WriteMulDivType(bits, src, 6);
}
void XEmitter::IMUL(int bits, const OpArg& src)
{
  WriteMulDivType(bits, src, 5);
}
void XEmitter::IDIV(int bits, const OpArg& src)
{
  WriteMulDivType(bits, src, 7);
}
void XEmitter::NEG(int bits, const OpArg& src)
{
  WriteMulDivType(bits, src, 3);
}
void XEmitter::NOT(int bits, const OpArg& src)
{
  WriteMulDivType(bits, src, 2);
}

void XEmitter::WriteBitSearchType(int bits, X64Reg dest, OpArg src, u8 byte2, bool rep)
{
  ASSERT_MSG(DYNA_REC, !src.IsImm(), "WriteBitSearchType - Imm argument");
  CheckFlags();
  src.operandReg = (u8)dest;
  if (bits == 16)
    Write8(0x66);
  if (rep)
    Write8(0xF3);
  src.WriteREX(this, bits, bits);
  Write8(0x0F);
  Write8(byte2);
  src.WriteRest(this);
}

void XEmitter::MOVNTI(int bits, const OpArg& dest, X64Reg src)
{
  if (bits <= 16)
    ASSERT_MSG(DYNA_REC, 0, "MOVNTI - bits<=16");
  WriteBitSearchType(bits, src, dest, 0xC3);
}

void XEmitter::BSF(int bits, X64Reg dest, const OpArg& src)
{
  WriteBitSearchType(bits, dest, src, 0xBC);
}  // Bottom bit to top bit
void XEmitter::BSR(int bits, X64Reg dest, const OpArg& src)
{
  WriteBitSearchType(bits, dest, src, 0xBD);
}  // Top bit to bottom bit

void XEmitter::TZCNT(int bits, X64Reg dest, const OpArg& src)
{
  CheckFlags();
  if (!cpu_info.bBMI1)
    PanicAlertFmt("Trying to use BMI1 on a system that doesn't support it. Bad programmer.");
  WriteBitSearchType(bits, dest, src, 0xBC, true);
}
void XEmitter::LZCNT(int bits, X64Reg dest, const OpArg& src)
{
  CheckFlags();
  if (!cpu_info.bLZCNT)
    PanicAlertFmt("Trying to use LZCNT on a system that doesn't support it. Bad programmer.");
  WriteBitSearchType(bits, dest, src, 0xBD, true);
}

void XEmitter::MOVSX(int dbits, int sbits, X64Reg dest, OpArg src)
{
  ASSERT_MSG(DYNA_REC, !src.IsImm(), "MOVSX - Imm argument");
  if (dbits == sbits)
  {
    MOV(dbits, R(dest), src);
    return;
  }
  src.operandReg = (u8)dest;
  if (dbits == 16)
    Write8(0x66);
  src.WriteREX(this, dbits, sbits);
  if (sbits == 8)
  {
    Write8(0x0F);
    Write8(0xBE);
  }
  else if (sbits == 16)
  {
    Write8(0x0F);
    Write8(0xBF);
  }
  else if (sbits == 32 && dbits == 64)
  {
    Write8(0x63);
  }
  else
  {
    Crash();
  }
  src.WriteRest(this);
}

void XEmitter::MOVZX(int dbits, int sbits, X64Reg dest, OpArg src)
{
  ASSERT_MSG(DYNA_REC, !src.IsImm(), "MOVZX - Imm argument");
  if (dbits == sbits)
  {
    MOV(dbits, R(dest), src);
    return;
  }
  src.operandReg = (u8)dest;
  if (dbits == 16)
    Write8(0x66);
  // the 32bit result is automatically zero extended to 64bit
  src.WriteREX(this, dbits == 64 ? 32 : dbits, sbits);
  if (sbits == 8)
  {
    Write8(0x0F);
    Write8(0xB6);
  }
  else if (sbits == 16)
  {
    Write8(0x0F);
    Write8(0xB7);
  }
  else if (sbits == 32 && dbits == 64)
  {
    Write8(0x8B);
  }
  else
  {
    ASSERT_MSG(DYNA_REC, 0, "MOVZX - Invalid size");
  }
  src.WriteRest(this);
}

void XEmitter::WriteMOVBE(int bits, u8 op, X64Reg reg, const OpArg& arg)
{
  ASSERT_MSG(DYNA_REC, cpu_info.bMOVBE, "Generating MOVBE on a system that does not support it.");
  if (bits == 8)
  {
    MOV(8, op & 1 ? arg : R(reg), op & 1 ? R(reg) : arg);
    return;
  }
  if (bits == 16)
    Write8(0x66);
  ASSERT_MSG(DYNA_REC, !arg.IsSimpleReg() && !arg.IsImm(), "MOVBE: need r<-m or m<-r!");
  arg.WriteREX(this, bits, bits, reg);
  Write8(0x0F);
  Write8(0x38);
  Write8(op);
  arg.WriteRest(this, 0, reg);
}
void XEmitter::MOVBE(int bits, X64Reg dest, const OpArg& src)
{
  WriteMOVBE(bits, 0xF0, dest, src);
}
void XEmitter::MOVBE(int bits, const OpArg& dest, X64Reg src)
{
  WriteMOVBE(bits, 0xF1, src, dest);
}

void XEmitter::LoadAndSwap(int size, X64Reg dst, const OpArg& src, bool sign_extend, MovInfo* info)
{
  if (info)
  {
    info->address = GetWritableCodePtr();
    info->nonAtomicSwapStore = false;
  }

  switch (size)
  {
  case 8:
    if (sign_extend)
      MOVSX(32, 8, dst, src);
    else
      MOVZX(32, 8, dst, src);
    break;
  case 16:
    MOVZX(32, 16, dst, src);
    if (sign_extend)
    {
      BSWAP(32, dst);
      SAR(32, R(dst), Imm8(16));
    }
    else
    {
      ROL(16, R(dst), Imm8(8));
    }
    break;
  case 32:
  case 64:
    if (cpu_info.bMOVBE)
    {
      MOVBE(size, dst, src);
    }
    else
    {
      MOV(size, R(dst), src);
      BSWAP(size, dst);
    }
    break;
  }
}

void XEmitter::SwapAndStore(int size, const OpArg& dst, X64Reg src, MovInfo* info)
{
  if (cpu_info.bMOVBE)
  {
    if (info)
    {
      info->address = GetWritableCodePtr();
      info->nonAtomicSwapStore = false;
    }
    MOVBE(size, dst, src);
  }
  else
  {
    BSWAP(size, src);
    if (info)
    {
      info->address = GetWritableCodePtr();
      info->nonAtomicSwapStore = true;
      info->nonAtomicSwapStoreSrc = src;
    }
    MOV(size, dst, R(src));
  }
}

void XEmitter::LEA(int bits, X64Reg dest, OpArg src)
{
  ASSERT_MSG(DYNA_REC, !src.IsImm(), "LEA - Imm argument");
  src.operandReg = (u8)dest;
  if (bits == 16)
    Write8(0x66);  // TODO: performance warning
  src.WriteREX(this, bits, bits);
  Write8(0x8D);
  src.WriteRest(this, 0, INVALID_REG, bits == 64);
}

// shift can be either imm8 or cl
void XEmitter::WriteShift(int bits, OpArg dest, const OpArg& shift, int ext)
{
  CheckFlags();
  bool writeImm = false;
  if (dest.IsImm())
  {
    ASSERT_MSG(DYNA_REC, 0, "WriteShift - can't shift imms");
  }
  if ((shift.IsSimpleReg() && shift.GetSimpleReg() != ECX) ||
      (shift.IsImm() && shift.GetImmBits() != 8))
  {
    ASSERT_MSG(DYNA_REC, 0, "WriteShift - illegal argument");
  }
  dest.operandReg = ext;
  if (bits == 16)
    Write8(0x66);
  dest.WriteREX(this, bits, bits, 0);
  if (shift.GetImmBits() == 8)
  {
    // ok an imm
    u8 imm = (u8)shift.offset;
    if (imm == 1)
    {
      Write8(bits == 8 ? 0xD0 : 0xD1);
    }
    else
    {
      writeImm = true;
      Write8(bits == 8 ? 0xC0 : 0xC1);
    }
  }
  else
  {
    Write8(bits == 8 ? 0xD2 : 0xD3);
  }
  dest.WriteRest(this, writeImm ? 1 : 0);
  if (writeImm)
    Write8((u8)shift.offset);
}

// large rotates and shift are slower on Intel than AMD
// Intel likes to rotate by 1, and the op is smaller too
void XEmitter::ROL(int bits, const OpArg& dest, const OpArg& shift)
{
  WriteShift(bits, dest, shift, 0);
}
void XEmitter::ROR(int bits, const OpArg& dest, const OpArg& shift)
{
  WriteShift(bits, dest, shift, 1);
}
void XEmitter::RCL(int bits, const OpArg& dest, const OpArg& shift)
{
  WriteShift(bits, dest, shift, 2);
}
void XEmitter::RCR(int bits, const OpArg& dest, const OpArg& shift)
{
  WriteShift(bits, dest, shift, 3);
}
void XEmitter::SHL(int bits, const OpArg& dest, const OpArg& shift)
{
  WriteShift(bits, dest, shift, 4);
}
void XEmitter::SHR(int bits, const OpArg& dest, const OpArg& shift)
{
  WriteShift(bits, dest, shift, 5);
}
void XEmitter::SAR(int bits, const OpArg& dest, const OpArg& shift)
{
  WriteShift(bits, dest, shift, 7);
}

// index can be either imm8 or register, don't use memory destination because it's slow
void XEmitter::WriteBitTest(int bits, const OpArg& dest, const OpArg& index, int ext)
{
  CheckFlags();
  if (dest.IsImm())
  {
    ASSERT_MSG(DYNA_REC, 0, "WriteBitTest - can't test imms");
  }
  if ((index.IsImm() && index.GetImmBits() != 8))
  {
    ASSERT_MSG(DYNA_REC, 0, "WriteBitTest - illegal argument");
  }
  if (bits == 16)
    Write8(0x66);
  if (index.IsImm())
  {
    dest.WriteREX(this, bits, bits);
    Write8(0x0F);
    Write8(0xBA);
    dest.WriteRest(this, 1, (X64Reg)ext);
    Write8((u8)index.offset);
  }
  else
  {
    X64Reg operand = index.GetSimpleReg();
    dest.WriteREX(this, bits, bits, operand);
    Write8(0x0F);
    Write8(0x83 + 8 * ext);
    dest.WriteRest(this, 1, operand);
  }
}

void XEmitter::BT(int bits, const OpArg& dest, const OpArg& index)
{
  WriteBitTest(bits, dest, index, 4);
}
void XEmitter::BTS(int bits, const OpArg& dest, const OpArg& index)
{
  WriteBitTest(bits, dest, index, 5);
}
void XEmitter::BTR(int bits, const OpArg& dest, const OpArg& index)
{
  WriteBitTest(bits, dest, index, 6);
}
void XEmitter::BTC(int bits, const OpArg& dest, const OpArg& index)
{
  WriteBitTest(bits, dest, index, 7);
}

// shift can be either imm8 or cl
void XEmitter::SHRD(int bits, const OpArg& dest, const OpArg& src, const OpArg& shift)
{
  CheckFlags();
  if (dest.IsImm())
  {
    ASSERT_MSG(DYNA_REC, 0, "SHRD - can't use imms as destination");
  }
  if (!src.IsSimpleReg())
  {
    ASSERT_MSG(DYNA_REC, 0, "SHRD - must use simple register as source");
  }
  if ((shift.IsSimpleReg() && shift.GetSimpleReg() != ECX) ||
      (shift.IsImm() && shift.GetImmBits() != 8))
  {
    ASSERT_MSG(DYNA_REC, 0, "SHRD - illegal shift");
  }
  if (bits == 16)
    Write8(0x66);
  X64Reg operand = src.GetSimpleReg();
  dest.WriteREX(this, bits, bits, operand);
  if (shift.GetImmBits() == 8)
  {
    Write8(0x0F);
    Write8(0xAC);
    dest.WriteRest(this, 1, operand);
    Write8((u8)shift.offset);
  }
  else
  {
    Write8(0x0F);
    Write8(0xAD);
    dest.WriteRest(this, 0, operand);
  }
}

void XEmitter::SHLD(int bits, const OpArg& dest, const OpArg& src, const OpArg& shift)
{
  CheckFlags();
  if (dest.IsImm())
  {
    ASSERT_MSG(DYNA_REC, 0, "SHLD - can't use imms as destination");
  }
  if (!src.IsSimpleReg())
  {
    ASSERT_MSG(DYNA_REC, 0, "SHLD - must use simple register as source");
  }
  if ((shift.IsSimpleReg() && shift.GetSimpleReg() != ECX) ||
      (shift.IsImm() && shift.GetImmBits() != 8))
  {
    ASSERT_MSG(DYNA_REC, 0, "SHLD - illegal shift");
  }
  if (bits == 16)
    Write8(0x66);
  X64Reg operand = src.GetSimpleReg();
  dest.WriteREX(this, bits, bits, operand);
  if (shift.GetImmBits() == 8)
  {
    Write8(0x0F);
    Write8(0xA4);
    dest.WriteRest(this, 1, operand);
    Write8((u8)shift.offset);
  }
  else
  {
    Write8(0x0F);
    Write8(0xA5);
    dest.WriteRest(this, 0, operand);
  }
}

void OpArg::WriteSingleByteOp(XEmitter* emit, u8 op, X64Reg _operandReg, int bits)
{
  if (bits == 16)
    emit->Write8(0x66);

  this->operandReg = (u8)_operandReg;
  WriteREX(emit, bits, bits);
  emit->Write8(op);
  WriteRest(emit);
}

// operand can either be immediate or register
void OpArg::WriteNormalOp(XEmitter* emit, bool toRM, NormalOp op, const OpArg& operand,
                          int bits) const
{
  X64Reg _operandReg;
  if (IsImm())
  {
    ASSERT_MSG(DYNA_REC, 0, "WriteNormalOp - Imm argument, wrong order");
  }

  if (bits == 16)
    emit->Write8(0x66);

  int immToWrite = 0;
  const NormalOpDef& op_def = normalops[static_cast<int>(op)];

  if (operand.IsImm())
  {
    WriteREX(emit, bits, bits);

    if (!toRM)
    {
      ASSERT_MSG(DYNA_REC, 0, "WriteNormalOp - Writing to Imm (!toRM)");
    }

    if (operand.scale == SCALE_IMM8 && bits == 8)
    {
      // op al, imm8
      if (!scale && offsetOrBaseReg == AL && op_def.eaximm8 != 0xCC)
      {
        emit->Write8(op_def.eaximm8);
        emit->Write8((u8)operand.offset);
        return;
      }
      // mov reg, imm8
      if (!scale && op == NormalOp::MOV)
      {
        emit->Write8(0xB0 + (offsetOrBaseReg & 7));
        emit->Write8((u8)operand.offset);
        return;
      }
      // op r/m8, imm8
      emit->Write8(op_def.imm8);
      immToWrite = 8;
    }
    else if ((operand.scale == SCALE_IMM16 && bits == 16) ||
             (operand.scale == SCALE_IMM32 && bits == 32) ||
             (operand.scale == SCALE_IMM32 && bits == 64))
    {
      // Try to save immediate size if we can, but first check to see
      // if the instruction supports simm8.
      // op r/m, imm8
      if (op_def.simm8 != 0xCC &&
          ((operand.scale == SCALE_IMM16 && (s16)operand.offset == (s8)operand.offset) ||
           (operand.scale == SCALE_IMM32 && (s32)operand.offset == (s8)operand.offset)))
      {
        emit->Write8(op_def.simm8);
        immToWrite = 8;
      }
      else
      {
        // mov reg, imm
        if (!scale && op == NormalOp::MOV && bits != 64)
        {
          emit->Write8(0xB8 + (offsetOrBaseReg & 7));
          if (bits == 16)
            emit->Write16((u16)operand.offset);
          else
            emit->Write32((u32)operand.offset);
          return;
        }
        // op eax, imm
        if (!scale && offsetOrBaseReg == EAX && op_def.eaximm32 != 0xCC)
        {
          emit->Write8(op_def.eaximm32);
          if (bits == 16)
            emit->Write16((u16)operand.offset);
          else
            emit->Write32((u32)operand.offset);
          return;
        }
        // op r/m, imm
        emit->Write8(op_def.imm32);
        immToWrite = bits == 16 ? 16 : 32;
      }
    }
    else if ((operand.scale == SCALE_IMM8 && bits == 16) ||
             (operand.scale == SCALE_IMM8 && bits == 32) ||
             (operand.scale == SCALE_IMM8 && bits == 64))
    {
      // op r/m, imm8
      emit->Write8(op_def.simm8);
      immToWrite = 8;
    }
    else if (operand.scale == SCALE_IMM64 && bits == 64)
    {
      if (scale)
      {
        ASSERT_MSG(DYNA_REC, 0,
                   "WriteNormalOp - MOV with 64-bit imm requires register destination");
      }
      // mov reg64, imm64
      else if (op == NormalOp::MOV)
      {
        // movabs reg64, imm64 (10 bytes)
        if (static_cast<s64>(operand.offset) != static_cast<s32>(operand.offset))
        {
          emit->Write8(0xB8 + (offsetOrBaseReg & 7));
          emit->Write64(operand.offset);
          return;
        }
        // mov reg64, simm32 (7 bytes)
        emit->Write8(op_def.imm32);
        immToWrite = 32;
      }
      else
      {
        ASSERT_MSG(DYNA_REC, 0, "WriteNormalOp - Only MOV can take 64-bit imm");
      }
    }
    else
    {
      ASSERT_MSG(DYNA_REC, 0, "WriteNormalOp - Unhandled case {} {}", operand.scale, bits);
    }

    // pass extension in REG of ModRM
    _operandReg = static_cast<X64Reg>(op_def.ext);
  }
  else
  {
    _operandReg = (X64Reg)operand.offsetOrBaseReg;
    WriteREX(emit, bits, bits, _operandReg);
    // op r/m, reg
    if (toRM)
    {
      emit->Write8(bits == 8 ? op_def.toRm8 : op_def.toRm32);
    }
    // op reg, r/m
    else
    {
      emit->Write8(bits == 8 ? op_def.fromRm8 : op_def.fromRm32);
    }
  }
  WriteRest(emit, immToWrite >> 3, _operandReg);
  switch (immToWrite)
  {
  case 0:
    break;
  case 8:
    emit->Write8((u8)operand.offset);
    break;
  case 16:
    emit->Write16((u16)operand.offset);
    break;
  case 32:
    emit->Write32((u32)operand.offset);
    break;
  default:
    ASSERT_MSG(DYNA_REC, 0, "WriteNormalOp - Unhandled case");
  }
}

void XEmitter::WriteNormalOp(int bits, NormalOp op, const OpArg& a1, const OpArg& a2)
{
  if (a1.IsImm())
  {
    // Booh! Can't write to an imm
    ASSERT_MSG(DYNA_REC, 0, "WriteNormalOp - a1 cannot be imm");
    return;
  }
  if (a2.IsImm())
  {
    a1.WriteNormalOp(this, true, op, a2, bits);
  }
  else
  {
    if (a1.IsSimpleReg())
    {
      a2.WriteNormalOp(this, false, op, a1, bits);
    }
    else
    {
      ASSERT_MSG(DYNA_REC, a2.IsSimpleReg() || a2.IsImm(),
                 "WriteNormalOp - a1 and a2 cannot both be memory");
      a1.WriteNormalOp(this, true, op, a2, bits);
    }
  }
}

void XEmitter::ADD(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  WriteNormalOp(bits, NormalOp::ADD, a1, a2);
}
void XEmitter::ADC(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  WriteNormalOp(bits, NormalOp::ADC, a1, a2);
}
void XEmitter::SUB(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  WriteNormalOp(bits, NormalOp::SUB, a1, a2);
}
void XEmitter::SBB(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  WriteNormalOp(bits, NormalOp::SBB, a1, a2);
}
void XEmitter::AND(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  WriteNormalOp(bits, NormalOp::AND, a1, a2);
}
void XEmitter::OR(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  WriteNormalOp(bits, NormalOp::OR, a1, a2);
}
void XEmitter::XOR(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  WriteNormalOp(bits, NormalOp::XOR, a1, a2);
}
void XEmitter::MOV(int bits, const OpArg& a1, const OpArg& a2)
{
  if (bits == 64 && a1.IsSimpleReg() &&
      ((a2.scale == SCALE_IMM64 && a2.offset == static_cast<u32>(a2.offset)) ||
       (a2.scale == SCALE_IMM32 && static_cast<s32>(a2.offset) >= 0)))
  {
    WriteNormalOp(32, NormalOp::MOV, a1, a2.AsImm32());
    return;
  }
  if (a1.IsSimpleReg() && a2.IsSimpleReg() && a1.GetSimpleReg() == a2.GetSimpleReg())
    ERROR_LOG_FMT(DYNA_REC, "Redundant MOV @ {} - bug in JIT?", fmt::ptr(code));
  WriteNormalOp(bits, NormalOp::MOV, a1, a2);
}
void XEmitter::TEST(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  WriteNormalOp(bits, NormalOp::TEST, a1, a2);
}
void XEmitter::CMP(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  WriteNormalOp(bits, NormalOp::CMP, a1, a2);
}
void XEmitter::XCHG(int bits, const OpArg& a1, const OpArg& a2)
{
  WriteNormalOp(bits, NormalOp::XCHG, a1, a2);
}
void XEmitter::CMP_or_TEST(int bits, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  if (a1.IsSimpleReg() && a2.IsZero())  // turn 'CMP reg, 0' into shorter 'TEST reg, reg'
  {
    WriteNormalOp(bits, NormalOp::TEST, a1, a1);
  }
  else
  {
    WriteNormalOp(bits, NormalOp::CMP, a1, a2);
  }
}

void XEmitter::MOV_sum(int bits, X64Reg dest, const OpArg& a1, const OpArg& a2)
{
  // This stomps on flags, so ensure they aren't locked
  DEBUG_ASSERT(!flags_locked);

  // Zero shortcuts (note that this can generate no code in the case where a1 == dest && a2 == zero
  // or a2 == dest && a1 == zero)
  if (a1.IsZero())
  {
    if (!a2.IsSimpleReg() || a2.GetSimpleReg() != dest)
    {
      MOV(bits, R(dest), a2);
    }
    return;
  }
  if (a2.IsZero())
  {
    if (!a1.IsSimpleReg() || a1.GetSimpleReg() != dest)
    {
      MOV(bits, R(dest), a1);
    }
    return;
  }

  // If dest == a1 or dest == a2 we can simplify this
  if (a1.IsSimpleReg() && a1.GetSimpleReg() == dest)
  {
    ADD(bits, R(dest), a2);
    return;
  }

  if (a2.IsSimpleReg() && a2.GetSimpleReg() == dest)
  {
    ADD(bits, R(dest), a1);
    return;
  }

  // TODO: 32-bit optimizations may apply to other bit sizes (confirm)
  if (bits == 32)
  {
    if (a1.IsImm() && a2.IsImm())
    {
      MOV(32, R(dest), Imm32(a1.Imm32() + a2.Imm32()));
      return;
    }

    if (a1.IsSimpleReg() && a2.IsSimpleReg())
    {
      LEA(32, dest, MRegSum(a1.GetSimpleReg(), a2.GetSimpleReg()));
      return;
    }

    if (a1.IsSimpleReg() && a2.IsImm())
    {
      LEA(32, dest, MDisp(a1.GetSimpleReg(), a2.Imm32()));
      return;
    }

    if (a1.IsImm() && a2.IsSimpleReg())
    {
      LEA(32, dest, MDisp(a2.GetSimpleReg(), a1.Imm32()));
      return;
    }
  }

  // Fallback
  MOV(bits, R(dest), a1);
  ADD(bits, R(dest), a2);
}

void XEmitter::IMUL(int bits, X64Reg regOp, const OpArg& a1, const OpArg& a2)
{
  CheckFlags();
  if (bits == 8)
  {
    ASSERT_MSG(DYNA_REC, 0, "IMUL - illegal bit size!");
    return;
  }

  if (a1.IsImm())
  {
    ASSERT_MSG(DYNA_REC, 0, "IMUL - second arg cannot be imm!");
    return;
  }

  if (!a2.IsImm())
  {
    ASSERT_MSG(DYNA_REC, 0, "IMUL - third arg must be imm!");
    return;
  }

  if (bits == 16)
    Write8(0x66);
  a1.WriteREX(this, bits, bits, regOp);

  if (a2.GetImmBits() == 8 || (a2.GetImmBits() == 16 && (s8)a2.offset == (s16)a2.offset) ||
      (a2.GetImmBits() == 32 && (s8)a2.offset == (s32)a2.offset))
  {
    Write8(0x6B);
    a1.WriteRest(this, 1, regOp);
    Write8((u8)a2.offset);
  }
  else
  {
    Write8(0x69);
    if (a2.GetImmBits() == 16 && bits == 16)
    {
      a1.WriteRest(this, 2, regOp);
      Write16((u16)a2.offset);
    }
    else if (a2.GetImmBits() == 32 && (bits == 32 || bits == 64))
    {
      a1.WriteRest(this, 4, regOp);
      Write32((u32)a2.offset);
    }
    else
    {
      ASSERT_MSG(DYNA_REC, 0, "IMUL - unhandled case!");
    }
  }
}

void XEmitter::IMUL(int bits, X64Reg regOp, const OpArg& a)
{
  CheckFlags();
  if (bits == 8)
  {
    ASSERT_MSG(DYNA_REC, 0, "IMUL - illegal bit size!");
    return;
  }

  if (a.IsImm())
  {
    IMUL(bits, regOp, R(regOp), a);
    return;
  }

  if (bits == 16)
    Write8(0x66);
  a.WriteREX(this, bits, bits, regOp);
  Write8(0x0F);
  Write8(0xAF);
  a.WriteRest(this, 0, regOp);
}

void XEmitter::WriteSSEOp(u8 opPrefix, u16 op, X64Reg regOp, OpArg arg, int extrabytes)
{
  if (opPrefix)
    Write8(opPrefix);
  arg.operandReg = regOp;
  arg.WriteREX(this, 0, 0);
  Write8(0x0F);
  if (op > 0xFF)
    Write8((op >> 8) & 0xFF);
  Write8(op & 0xFF);
  arg.WriteRest(this, extrabytes);
}

static int GetVEXmmmmm(u16 op)
{
  // Currently, only 0x38 and 0x3A are used as secondary escape byte.
  if ((op >> 8) == 0x3A)
    return 3;
  else if ((op >> 8) == 0x38)
    return 2;
  else
    return 1;
}

static int GetVEXpp(u8 opPrefix)
{
  if (opPrefix == 0x66)
    return 1;
  else if (opPrefix == 0xF3)
    return 2;
  else if (opPrefix == 0xF2)
    return 3;
  else
    return 0;
}

void XEmitter::WriteVEXOp(u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                          int W, int extrabytes)
{
  int mmmmm = GetVEXmmmmm(op);
  int pp = GetVEXpp(opPrefix);
  // FIXME: we currently don't support 256-bit instructions, and "size" is not the vector size here
  arg.WriteVEX(this, regOp1, regOp2, 0, pp, mmmmm, W);
  Write8(op & 0xFF);
  arg.WriteRest(this, extrabytes, regOp1);
}

void XEmitter::WriteVEXOp4(u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                           X64Reg regOp3, int W)
{
  WriteVEXOp(opPrefix, op, regOp1, regOp2, arg, W, 1);
  Write8((u8)regOp3 << 4);
}

void XEmitter::WriteAVXOp(u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                          int W, int extrabytes)
{
  if (!cpu_info.bAVX)
    PanicAlertFmt("Trying to use AVX on a system that doesn't support it. Bad programmer.");
  WriteVEXOp(opPrefix, op, regOp1, regOp2, arg, W, extrabytes);
}

void XEmitter::WriteAVXOp4(u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                           X64Reg regOp3, int W)
{
  if (!cpu_info.bAVX)
    PanicAlertFmt("Trying to use AVX on a system that doesn't support it. Bad programmer.");
  WriteVEXOp4(opPrefix, op, regOp1, regOp2, arg, regOp3, W);
}

void XEmitter::WriteFMA3Op(u8 op, X64Reg regOp1, X64Reg regOp2, const OpArg& arg, int W)
{
  if (!cpu_info.bFMA)
  {
    PanicAlertFmt(
        "Trying to use FMA3 on a system that doesn't support it. Computer is v. f'n madd.");
  }
  WriteVEXOp(0x66, 0x3800 | op, regOp1, regOp2, arg, W);
}

void XEmitter::WriteFMA4Op(u8 op, X64Reg dest, X64Reg regOp1, X64Reg regOp2, const OpArg& arg,
                           int W)
{
  if (!cpu_info.bFMA4)
  {
    PanicAlertFmt(
        "Trying to use FMA4 on a system that doesn't support it. Computer is v. f'n madd.");
  }
  WriteVEXOp4(0x66, 0x3A00 | op, dest, regOp1, arg, regOp2, W);
}

void XEmitter::WriteBMIOp(int size, u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2,
                          const OpArg& arg, int extrabytes)
{
  if (arg.IsImm())
    PanicAlertFmt("BMI1/2 instructions don't support immediate operands.");
  if (size != 32 && size != 64)
    PanicAlertFmt("BMI1/2 instructions only support 32-bit and 64-bit modes!");
  const int W = size == 64;
  WriteVEXOp(opPrefix, op, regOp1, regOp2, arg, W, extrabytes);
}

void XEmitter::WriteBMI1Op(int size, u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2,
                           const OpArg& arg, int extrabytes)
{
  CheckFlags();
  if (!cpu_info.bBMI1)
    PanicAlertFmt("Trying to use BMI1 on a system that doesn't support it. Bad programmer.");
  WriteBMIOp(size, opPrefix, op, regOp1, regOp2, arg, extrabytes);
}

void XEmitter::WriteBMI2Op(int size, u8 opPrefix, u16 op, X64Reg regOp1, X64Reg regOp2,
                           const OpArg& arg, int extrabytes)
{
  if (!cpu_info.bBMI2)
    PanicAlertFmt("Trying to use BMI2 on a system that doesn't support it. Bad programmer.");
  WriteBMIOp(size, opPrefix, op, regOp1, regOp2, arg, extrabytes);
}

void XEmitter::MOVD_xmm(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x6E, dest, arg, 0);
}
void XEmitter::MOVD_xmm(const OpArg& arg, X64Reg src)
{
  WriteSSEOp(0x66, 0x7E, src, arg, 0);
}

void XEmitter::MOVQ_xmm(X64Reg dest, OpArg arg)
{
  // Alternate encoding
  // This does not display correctly in MSVC's debugger, it thinks it's a MOVD
  arg.operandReg = dest;
  Write8(0x66);
  arg.WriteREX(this, 64, 0);
  Write8(0x0f);
  Write8(0x6E);
  arg.WriteRest(this, 0);
}

void XEmitter::MOVQ_xmm(OpArg arg, X64Reg src)
{
  if (src > 7 || arg.IsSimpleReg())
  {
    // Alternate encoding
    // This does not display correctly in MSVC's debugger, it thinks it's a MOVD
    arg.operandReg = src;
    Write8(0x66);
    arg.WriteREX(this, 64, 0);
    Write8(0x0f);
    Write8(0x7E);
    arg.WriteRest(this, 0);
  }
  else
  {
    arg.operandReg = src;
    arg.WriteREX(this, 0, 0);
    Write8(0x66);
    Write8(0x0f);
    Write8(0xD6);
    arg.WriteRest(this, 0);
  }
}

void XEmitter::WriteMXCSR(OpArg arg, int ext)
{
  if (arg.IsImm() || arg.IsSimpleReg())
    ASSERT_MSG(DYNA_REC, 0, "MXCSR - invalid operand");

  arg.operandReg = ext;
  arg.WriteREX(this, 0, 0);
  Write8(0x0F);
  Write8(0xAE);
  arg.WriteRest(this);
}

void XEmitter::STMXCSR(const OpArg& memloc)
{
  WriteMXCSR(memloc, 3);
}
void XEmitter::LDMXCSR(const OpArg& memloc)
{
  WriteMXCSR(memloc, 2);
}

void XEmitter::MOVNTDQ(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x66, sseMOVNTDQ, regOp, arg);
}
void XEmitter::MOVNTPS(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x00, sseMOVNTP, regOp, arg);
}
void XEmitter::MOVNTPD(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x66, sseMOVNTP, regOp, arg);
}

void XEmitter::ADDSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseADD, regOp, arg);
}
void XEmitter::ADDSD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, sseADD, regOp, arg);
}
void XEmitter::SUBSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseSUB, regOp, arg);
}
void XEmitter::SUBSD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, sseSUB, regOp, arg);
}
void XEmitter::CMPSS(X64Reg regOp, const OpArg& arg, u8 compare)
{
  WriteSSEOp(0xF3, sseCMP, regOp, arg, 1);
  Write8(compare);
}
void XEmitter::CMPSD(X64Reg regOp, const OpArg& arg, u8 compare)
{
  WriteSSEOp(0xF2, sseCMP, regOp, arg, 1);
  Write8(compare);
}
void XEmitter::MULSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseMUL, regOp, arg);
}
void XEmitter::MULSD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, sseMUL, regOp, arg);
}
void XEmitter::DIVSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseDIV, regOp, arg);
}
void XEmitter::DIVSD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, sseDIV, regOp, arg);
}
void XEmitter::MINSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseMIN, regOp, arg);
}
void XEmitter::MINSD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, sseMIN, regOp, arg);
}
void XEmitter::MAXSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseMAX, regOp, arg);
}
void XEmitter::MAXSD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, sseMAX, regOp, arg);
}
void XEmitter::SQRTSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseSQRT, regOp, arg);
}
void XEmitter::SQRTSD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, sseSQRT, regOp, arg);
}
void XEmitter::RCPSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseRCP, regOp, arg);
}
void XEmitter::RSQRTSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseRSQRT, regOp, arg);
}

void XEmitter::ADDPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseADD, regOp, arg);
}
void XEmitter::ADDPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseADD, regOp, arg);
}
void XEmitter::SUBPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseSUB, regOp, arg);
}
void XEmitter::SUBPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseSUB, regOp, arg);
}
void XEmitter::CMPPS(X64Reg regOp, const OpArg& arg, u8 compare)
{
  WriteSSEOp(0x00, sseCMP, regOp, arg, 1);
  Write8(compare);
}
void XEmitter::CMPPD(X64Reg regOp, const OpArg& arg, u8 compare)
{
  WriteSSEOp(0x66, sseCMP, regOp, arg, 1);
  Write8(compare);
}
void XEmitter::ANDPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseAND, regOp, arg);
}
void XEmitter::ANDPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseAND, regOp, arg);
}
void XEmitter::ANDNPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseANDN, regOp, arg);
}
void XEmitter::ANDNPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseANDN, regOp, arg);
}
void XEmitter::ORPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseOR, regOp, arg);
}
void XEmitter::ORPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseOR, regOp, arg);
}
void XEmitter::XORPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseXOR, regOp, arg);
}
void XEmitter::XORPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseXOR, regOp, arg);
}
void XEmitter::MULPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseMUL, regOp, arg);
}
void XEmitter::MULPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseMUL, regOp, arg);
}
void XEmitter::DIVPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseDIV, regOp, arg);
}
void XEmitter::DIVPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseDIV, regOp, arg);
}
void XEmitter::MINPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseMIN, regOp, arg);
}
void XEmitter::MINPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseMIN, regOp, arg);
}
void XEmitter::MAXPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseMAX, regOp, arg);
}
void XEmitter::MAXPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseMAX, regOp, arg);
}
void XEmitter::SQRTPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseSQRT, regOp, arg);
}
void XEmitter::SQRTPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseSQRT, regOp, arg);
}
void XEmitter::RCPPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseRCP, regOp, arg);
}
void XEmitter::RSQRTPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseRSQRT, regOp, arg);
}
void XEmitter::SHUFPS(X64Reg regOp, const OpArg& arg, u8 shuffle)
{
  WriteSSEOp(0x00, sseSHUF, regOp, arg, 1);
  Write8(shuffle);
}
void XEmitter::SHUFPD(X64Reg regOp, const OpArg& arg, u8 shuffle)
{
  WriteSSEOp(0x66, sseSHUF, regOp, arg, 1);
  Write8(shuffle);
}

void XEmitter::COMISS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseCOMIS, regOp, arg);
}  // weird that these should be packed
void XEmitter::COMISD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseCOMIS, regOp, arg);
}  // ordered
void XEmitter::UCOMISS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseUCOMIS, regOp, arg);
}  // unordered
void XEmitter::UCOMISD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseUCOMIS, regOp, arg);
}

void XEmitter::MOVAPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseMOVAPfromRM, regOp, arg);
}
void XEmitter::MOVAPD(X64Reg regOp, const OpArg& arg)
{
  // Prefer MOVAPS to MOVAPD as there is no reason to use MOVAPD over MOVAPS:
  // - They have equivalent functionality.
  // - There has never been a microarchitecture with separate single and double domains.
  // - MOVAPD is one byte longer than MOVAPS.
  MOVAPS(regOp, arg);
}
void XEmitter::MOVAPS(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x00, sseMOVAPtoRM, regOp, arg);
}
void XEmitter::MOVAPD(const OpArg& arg, X64Reg regOp)
{
  MOVAPS(arg, regOp);
}

void XEmitter::MOVUPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseMOVUPfromRM, regOp, arg);
}
void XEmitter::MOVUPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseMOVUPfromRM, regOp, arg);
}
void XEmitter::MOVUPS(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x00, sseMOVUPtoRM, regOp, arg);
}
void XEmitter::MOVUPD(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x66, sseMOVUPtoRM, regOp, arg);
}

void XEmitter::MOVDQA(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseMOVDQfromRM, regOp, arg);
}
void XEmitter::MOVDQA(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x66, sseMOVDQtoRM, regOp, arg);
}
void XEmitter::MOVDQU(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseMOVDQfromRM, regOp, arg);
}
void XEmitter::MOVDQU(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0xF3, sseMOVDQtoRM, regOp, arg);
}

void XEmitter::MOVSS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, sseMOVUPfromRM, regOp, arg);
}
void XEmitter::MOVSD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, sseMOVUPfromRM, regOp, arg);
}
void XEmitter::MOVSS(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0xF3, sseMOVUPtoRM, regOp, arg);
}
void XEmitter::MOVSD(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0xF2, sseMOVUPtoRM, regOp, arg);
}

void XEmitter::MOVLPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseMOVLPfromRM, regOp, arg);
}
void XEmitter::MOVLPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseMOVLPfromRM, regOp, arg);
}
void XEmitter::MOVLPS(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x00, sseMOVLPtoRM, regOp, arg);
}
void XEmitter::MOVLPD(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x66, sseMOVLPtoRM, regOp, arg);
}

void XEmitter::MOVHPS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, sseMOVHPfromRM, regOp, arg);
}
void XEmitter::MOVHPD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, sseMOVHPfromRM, regOp, arg);
}
void XEmitter::MOVHPS(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x00, sseMOVHPtoRM, regOp, arg);
}
void XEmitter::MOVHPD(const OpArg& arg, X64Reg regOp)
{
  WriteSSEOp(0x66, sseMOVHPtoRM, regOp, arg);
}

void XEmitter::MOVHLPS(X64Reg regOp1, X64Reg regOp2)
{
  WriteSSEOp(0x00, sseMOVHLPS, regOp1, R(regOp2));
}
void XEmitter::MOVLHPS(X64Reg regOp1, X64Reg regOp2)
{
  WriteSSEOp(0x00, sseMOVLHPS, regOp1, R(regOp2));
}

void XEmitter::CVTPS2PD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, 0x5A, regOp, arg);
}
void XEmitter::CVTPD2PS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x5A, regOp, arg);
}

void XEmitter::CVTSD2SS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, 0x5A, regOp, arg);
}
void XEmitter::CVTSS2SD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, 0x5A, regOp, arg);
}
void XEmitter::CVTSD2SI(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, 0x2D, regOp, arg);
}
void XEmitter::CVTSS2SI(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, 0x2D, regOp, arg);
}
void XEmitter::CVTSI2SD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, 0x2A, regOp, arg);
}
void XEmitter::CVTSI2SS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, 0x2A, regOp, arg);
}

void XEmitter::CVTDQ2PD(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, 0xE6, regOp, arg);
}
void XEmitter::CVTDQ2PS(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x00, 0x5B, regOp, arg);
}
void XEmitter::CVTPD2DQ(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, 0xE6, regOp, arg);
}
void XEmitter::CVTPS2DQ(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x5B, regOp, arg);
}

void XEmitter::CVTTSD2SI(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF2, 0x2C, regOp, arg);
}
void XEmitter::CVTTSS2SI(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, 0x2C, regOp, arg);
}
void XEmitter::CVTTPS2DQ(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0xF3, 0x5B, regOp, arg);
}
void XEmitter::CVTTPD2DQ(X64Reg regOp, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xE6, regOp, arg);
}

void XEmitter::MASKMOVDQU(X64Reg dest, X64Reg src)
{
  WriteSSEOp(0x66, sseMASKMOVDQU, dest, R(src));
}

void XEmitter::MOVMSKPS(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x00, 0x50, dest, arg);
}
void XEmitter::MOVMSKPD(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x50, dest, arg);
}

void XEmitter::LDDQU(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0xF2, sseLDDQU, dest, arg);
}  // For integer data only

void XEmitter::UNPCKLPS(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x00, 0x14, dest, arg);
}
void XEmitter::UNPCKHPS(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x00, 0x15, dest, arg);
}
void XEmitter::UNPCKLPD(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x14, dest, arg);
}
void XEmitter::UNPCKHPD(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x15, dest, arg);
}

// Pretty much every x86 CPU nowadays supports SSE3,
// but the SSE2 fallbacks are easy.
void XEmitter::MOVSLDUP(X64Reg regOp, const OpArg& arg)
{
  if (cpu_info.bSSE3)
  {
    WriteSSEOp(0xF3, 0x12, regOp, arg);
  }
  else
  {
    if (!arg.IsSimpleReg(regOp))
      MOVAPD(regOp, arg);
    UNPCKLPS(regOp, R(regOp));
  }
}
void XEmitter::MOVSHDUP(X64Reg regOp, const OpArg& arg)
{
  if (cpu_info.bSSE3)
  {
    WriteSSEOp(0xF3, 0x16, regOp, arg);
  }
  else
  {
    if (!arg.IsSimpleReg(regOp))
      MOVAPD(regOp, arg);
    UNPCKHPS(regOp, R(regOp));
  }
}
void XEmitter::MOVDDUP(X64Reg regOp, const OpArg& arg)
{
  if (cpu_info.bSSE3)
  {
    WriteSSEOp(0xF2, 0x12, regOp, arg);
  }
  else
  {
    if (!arg.IsSimpleReg())
    {
      MOVSD(regOp, arg);
    }
    else if (regOp != arg.GetSimpleReg())
    {
      MOVAPD(regOp, arg);
    }
    UNPCKLPD(regOp, R(regOp));
  }
}

// There are a few more left

// Also some integer instructions are missing
void XEmitter::PACKSSDW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x6B, dest, arg);
}
void XEmitter::PACKSSWB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x63, dest, arg);
}
void XEmitter::PACKUSWB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x67, dest, arg);
}

void XEmitter::PUNPCKLBW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x60, dest, arg);
}
void XEmitter::PUNPCKLWD(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x61, dest, arg);
}
void XEmitter::PUNPCKLDQ(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x62, dest, arg);
}
void XEmitter::PUNPCKLQDQ(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x6C, dest, arg);
}

void XEmitter::PSRLW(X64Reg reg, int shift)
{
  WriteSSEOp(0x66, 0x71, (X64Reg)2, R(reg));
  Write8(shift);
}

void XEmitter::PSRLD(X64Reg reg, int shift)
{
  WriteSSEOp(0x66, 0x72, (X64Reg)2, R(reg));
  Write8(shift);
}

void XEmitter::PSRLQ(X64Reg reg, int shift)
{
  WriteSSEOp(0x66, 0x73, (X64Reg)2, R(reg));
  Write8(shift);
}

void XEmitter::PSRLQ(X64Reg reg, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xd3, reg, arg);
}

void XEmitter::PSRLDQ(X64Reg reg, int shift)
{
  WriteSSEOp(0x66, 0x73, (X64Reg)3, R(reg));
  Write8(shift);
}

void XEmitter::PSLLW(X64Reg reg, int shift)
{
  WriteSSEOp(0x66, 0x71, (X64Reg)6, R(reg));
  Write8(shift);
}

void XEmitter::PSLLD(X64Reg reg, int shift)
{
  WriteSSEOp(0x66, 0x72, (X64Reg)6, R(reg));
  Write8(shift);
}

void XEmitter::PSLLQ(X64Reg reg, int shift)
{
  WriteSSEOp(0x66, 0x73, (X64Reg)6, R(reg));
  Write8(shift);
}

void XEmitter::PSLLDQ(X64Reg reg, int shift)
{
  WriteSSEOp(0x66, 0x73, (X64Reg)7, R(reg));
  Write8(shift);
}

// WARNING not REX compatible
void XEmitter::PSRAW(X64Reg reg, int shift)
{
  if (reg > 7)
    PanicAlertFmt("The PSRAW-emitter does not support regs above 7");
  Write8(0x66);
  Write8(0x0f);
  Write8(0x71);
  Write8(0xE0 | reg);
  Write8(shift);
}

// WARNING not REX compatible
void XEmitter::PSRAD(X64Reg reg, int shift)
{
  if (reg > 7)
    PanicAlertFmt("The PSRAD-emitter does not support regs above 7");
  Write8(0x66);
  Write8(0x0f);
  Write8(0x72);
  Write8(0xE0 | reg);
  Write8(shift);
}

void XEmitter::WriteSSSE3Op(u8 opPrefix, u16 op, X64Reg regOp, const OpArg& arg, int extrabytes)
{
  if (!cpu_info.bSSSE3)
    PanicAlertFmt("Trying to use SSSE3 on a system that doesn't support it. Bad programmer.");
  WriteSSEOp(opPrefix, op, regOp, arg, extrabytes);
}

void XEmitter::WriteSSE41Op(u8 opPrefix, u16 op, X64Reg regOp, const OpArg& arg, int extrabytes)
{
  if (!cpu_info.bSSE4_1)
    PanicAlertFmt("Trying to use SSE4.1 on a system that doesn't support it. Bad programmer.");
  WriteSSEOp(opPrefix, op, regOp, arg, extrabytes);
}

void XEmitter::PSHUFB(X64Reg dest, const OpArg& arg)
{
  WriteSSSE3Op(0x66, 0x3800, dest, arg);
}
void XEmitter::PTEST(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3817, dest, arg);
}
void XEmitter::PACKUSDW(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x382b, dest, arg);
}

void XEmitter::PMOVSXBW(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3820, dest, arg);
}
void XEmitter::PMOVSXBD(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3821, dest, arg);
}
void XEmitter::PMOVSXBQ(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3822, dest, arg);
}
void XEmitter::PMOVSXWD(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3823, dest, arg);
}
void XEmitter::PMOVSXWQ(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3824, dest, arg);
}
void XEmitter::PMOVSXDQ(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3825, dest, arg);
}
void XEmitter::PMOVZXBW(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3830, dest, arg);
}
void XEmitter::PMOVZXBD(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3831, dest, arg);
}
void XEmitter::PMOVZXBQ(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3832, dest, arg);
}
void XEmitter::PMOVZXWD(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3833, dest, arg);
}
void XEmitter::PMOVZXWQ(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3834, dest, arg);
}
void XEmitter::PMOVZXDQ(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3835, dest, arg);
}

void XEmitter::PBLENDVB(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3810, dest, arg);
}
void XEmitter::BLENDVPS(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3814, dest, arg);
}
void XEmitter::BLENDVPD(X64Reg dest, const OpArg& arg)
{
  WriteSSE41Op(0x66, 0x3815, dest, arg);
}
void XEmitter::BLENDPS(X64Reg dest, const OpArg& arg, u8 blend)
{
  WriteSSE41Op(0x66, 0x3A0C, dest, arg, 1);
  Write8(blend);
}
void XEmitter::BLENDPD(X64Reg dest, const OpArg& arg, u8 blend)
{
  WriteSSE41Op(0x66, 0x3A0D, dest, arg, 1);
  Write8(blend);
}

void XEmitter::PAND(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xDB, dest, arg);
}
void XEmitter::PANDN(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xDF, dest, arg);
}
void XEmitter::PXOR(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xEF, dest, arg);
}
void XEmitter::POR(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xEB, dest, arg);
}

void XEmitter::PADDB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xFC, dest, arg);
}
void XEmitter::PADDW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xFD, dest, arg);
}
void XEmitter::PADDD(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xFE, dest, arg);
}
void XEmitter::PADDQ(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xD4, dest, arg);
}

void XEmitter::PADDSB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xEC, dest, arg);
}
void XEmitter::PADDSW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xED, dest, arg);
}
void XEmitter::PADDUSB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xDC, dest, arg);
}
void XEmitter::PADDUSW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xDD, dest, arg);
}

void XEmitter::PSUBB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xF8, dest, arg);
}
void XEmitter::PSUBW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xF9, dest, arg);
}
void XEmitter::PSUBD(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xFA, dest, arg);
}
void XEmitter::PSUBQ(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xFB, dest, arg);
}

void XEmitter::PSUBSB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xE8, dest, arg);
}
void XEmitter::PSUBSW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xE9, dest, arg);
}
void XEmitter::PSUBUSB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xD8, dest, arg);
}
void XEmitter::PSUBUSW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xD9, dest, arg);
}

void XEmitter::PAVGB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xE0, dest, arg);
}
void XEmitter::PAVGW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xE3, dest, arg);
}

void XEmitter::PCMPEQB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x74, dest, arg);
}
void XEmitter::PCMPEQW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x75, dest, arg);
}
void XEmitter::PCMPEQD(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x76, dest, arg);
}

void XEmitter::PCMPGTB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x64, dest, arg);
}
void XEmitter::PCMPGTW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x65, dest, arg);
}
void XEmitter::PCMPGTD(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0x66, dest, arg);
}

void XEmitter::PEXTRW(X64Reg dest, const OpArg& arg, u8 subreg)
{
  WriteSSEOp(0x66, 0xC5, dest, arg);
  Write8(subreg);
}
void XEmitter::PINSRW(X64Reg dest, const OpArg& arg, u8 subreg)
{
  WriteSSEOp(0x66, 0xC4, dest, arg);
  Write8(subreg);
}
void XEmitter::PINSRD(X64Reg dest, const OpArg& arg, u8 subreg)
{
  WriteSSE41Op(0x66, 0x3A22, dest, arg);
  Write8(subreg);
}

void XEmitter::PMADDWD(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xF5, dest, arg);
}
void XEmitter::PSADBW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xF6, dest, arg);
}

void XEmitter::PMAXSW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xEE, dest, arg);
}
void XEmitter::PMAXUB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xDE, dest, arg);
}
void XEmitter::PMINSW(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xEA, dest, arg);
}
void XEmitter::PMINUB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xDA, dest, arg);
}

void XEmitter::PMOVMSKB(X64Reg dest, const OpArg& arg)
{
  WriteSSEOp(0x66, 0xD7, dest, arg);
}
void XEmitter::PSHUFD(X64Reg regOp, const OpArg& arg, u8 shuffle)
{
  WriteSSEOp(0x66, 0x70, regOp, arg, 1);
  Write8(shuffle);
}
void XEmitter::PSHUFLW(X64Reg regOp, const OpArg& arg, u8 shuffle)
{
  WriteSSEOp(0xF2, 0x70, regOp, arg, 1);
  Write8(shuffle);
}
void XEmitter::PSHUFHW(X64Reg regOp, const OpArg& arg, u8 shuffle)
{
  WriteSSEOp(0xF3, 0x70, regOp, arg, 1);
  Write8(shuffle);
}

// VEX
void XEmitter::VADDSS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0xF3, sseADD, regOp1, regOp2, arg);
}
void XEmitter::VSUBSS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0xF3, sseSUB, regOp1, regOp2, arg);
}
void XEmitter::VMULSS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0xF3, sseMUL, regOp1, regOp2, arg);
}
void XEmitter::VDIVSS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0xF3, sseDIV, regOp1, regOp2, arg);
}
void XEmitter::VADDPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x00, sseADD, regOp1, regOp2, arg);
}
void XEmitter::VSUBPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x00, sseSUB, regOp1, regOp2, arg);
}
void XEmitter::VMULPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x00, sseMUL, regOp1, regOp2, arg);
}
void XEmitter::VDIVPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x00, sseDIV, regOp1, regOp2, arg);
}
void XEmitter::VADDSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0xF2, sseADD, regOp1, regOp2, arg);
}
void XEmitter::VSUBSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0xF2, sseSUB, regOp1, regOp2, arg);
}
void XEmitter::VMULSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0xF2, sseMUL, regOp1, regOp2, arg);
}
void XEmitter::VDIVSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0xF2, sseDIV, regOp1, regOp2, arg);
}
void XEmitter::VADDPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, sseADD, regOp1, regOp2, arg);
}
void XEmitter::VSUBPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, sseSUB, regOp1, regOp2, arg);
}
void XEmitter::VMULPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, sseMUL, regOp1, regOp2, arg);
}
void XEmitter::VDIVPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, sseDIV, regOp1, regOp2, arg);
}
void XEmitter::VSQRTSD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0xF2, sseSQRT, regOp1, regOp2, arg);
}
void XEmitter::VCMPPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 compare)
{
  WriteAVXOp(0x66, sseCMP, regOp1, regOp2, arg, 0, 1);
  Write8(compare);
}
void XEmitter::VSHUFPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 shuffle)
{
  WriteAVXOp(0x00, sseSHUF, regOp1, regOp2, arg, 0, 1);
  Write8(shuffle);
}
void XEmitter::VSHUFPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 shuffle)
{
  WriteAVXOp(0x66, sseSHUF, regOp1, regOp2, arg, 0, 1);
  Write8(shuffle);
}
void XEmitter::VUNPCKLPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x00, 0x14, regOp1, regOp2, arg);
}
void XEmitter::VUNPCKLPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, 0x14, regOp1, regOp2, arg);
}
void XEmitter::VUNPCKHPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, 0x15, regOp1, regOp2, arg);
}
void XEmitter::VBLENDVPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, X64Reg regOp3)
{
  WriteAVXOp4(0x66, 0x3A4B, regOp1, regOp2, arg, regOp3);
}
void XEmitter::VBLENDPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 blend)
{
  WriteAVXOp(0x66, 0x3A0C, regOp1, regOp2, arg, 0, 1);
  Write8(blend);
}
void XEmitter::VBLENDPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg, u8 blend)
{
  WriteAVXOp(0x66, 0x3A0D, regOp1, regOp2, arg, 0, 1);
  Write8(blend);
}

void XEmitter::VANDPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x00, sseAND, regOp1, regOp2, arg);
}
void XEmitter::VANDPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, sseAND, regOp1, regOp2, arg);
}
void XEmitter::VANDNPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x00, sseANDN, regOp1, regOp2, arg);
}
void XEmitter::VANDNPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, sseANDN, regOp1, regOp2, arg);
}
void XEmitter::VORPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x00, sseOR, regOp1, regOp2, arg);
}
void XEmitter::VORPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, sseOR, regOp1, regOp2, arg);
}
void XEmitter::VXORPS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x00, sseXOR, regOp1, regOp2, arg);
}
void XEmitter::VXORPD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, sseXOR, regOp1, regOp2, arg);
}

void XEmitter::VPAND(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, 0xDB, regOp1, regOp2, arg);
}
void XEmitter::VPANDN(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, 0xDF, regOp1, regOp2, arg);
}
void XEmitter::VPOR(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, 0xEB, regOp1, regOp2, arg);
}
void XEmitter::VPXOR(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteAVXOp(0x66, 0xEF, regOp1, regOp2, arg);
}

void XEmitter::VFMADD132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x98, regOp1, regOp2, arg);
}
void XEmitter::VFMADD213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xA8, regOp1, regOp2, arg);
}
void XEmitter::VFMADD231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xB8, regOp1, regOp2, arg);
}
void XEmitter::VFMADD132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x98, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMADD213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xA8, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMADD231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xB8, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMADD132SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x99, regOp1, regOp2, arg);
}
void XEmitter::VFMADD213SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xA9, regOp1, regOp2, arg);
}
void XEmitter::VFMADD231SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xB9, regOp1, regOp2, arg);
}
void XEmitter::VFMADD132SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x99, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMADD213SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xA9, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMADD231SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xB9, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMSUB132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9A, regOp1, regOp2, arg);
}
void XEmitter::VFMSUB213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAA, regOp1, regOp2, arg);
}
void XEmitter::VFMSUB231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBA, regOp1, regOp2, arg);
}
void XEmitter::VFMSUB132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9A, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMSUB213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAA, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMSUB231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBA, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMSUB132SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9B, regOp1, regOp2, arg);
}
void XEmitter::VFMSUB213SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAB, regOp1, regOp2, arg);
}
void XEmitter::VFMSUB231SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBB, regOp1, regOp2, arg);
}
void XEmitter::VFMSUB132SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9B, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMSUB213SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAB, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMSUB231SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBB, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMADD132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9C, regOp1, regOp2, arg);
}
void XEmitter::VFNMADD213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAC, regOp1, regOp2, arg);
}
void XEmitter::VFNMADD231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBC, regOp1, regOp2, arg);
}
void XEmitter::VFNMADD132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9C, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMADD213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAC, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMADD231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBC, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMADD132SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9D, regOp1, regOp2, arg);
}
void XEmitter::VFNMADD213SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAD, regOp1, regOp2, arg);
}
void XEmitter::VFNMADD231SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBD, regOp1, regOp2, arg);
}
void XEmitter::VFNMADD132SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9D, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMADD213SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAD, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMADD231SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBD, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMSUB132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9E, regOp1, regOp2, arg);
}
void XEmitter::VFNMSUB213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAE, regOp1, regOp2, arg);
}
void XEmitter::VFNMSUB231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBE, regOp1, regOp2, arg);
}
void XEmitter::VFNMSUB132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9E, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMSUB213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAE, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMSUB231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBE, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMSUB132SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9F, regOp1, regOp2, arg);
}
void XEmitter::VFNMSUB213SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAF, regOp1, regOp2, arg);
}
void XEmitter::VFNMSUB231SS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBF, regOp1, regOp2, arg);
}
void XEmitter::VFNMSUB132SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x9F, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMSUB213SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xAF, regOp1, regOp2, arg, 1);
}
void XEmitter::VFNMSUB231SD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xBF, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMADDSUB132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x96, regOp1, regOp2, arg);
}
void XEmitter::VFMADDSUB213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xA6, regOp1, regOp2, arg);
}
void XEmitter::VFMADDSUB231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xB6, regOp1, regOp2, arg);
}
void XEmitter::VFMADDSUB132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x96, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMADDSUB213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xA6, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMADDSUB231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xB6, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMSUBADD132PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x97, regOp1, regOp2, arg);
}
void XEmitter::VFMSUBADD213PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xA7, regOp1, regOp2, arg);
}
void XEmitter::VFMSUBADD231PS(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xB7, regOp1, regOp2, arg);
}
void XEmitter::VFMSUBADD132PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0x97, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMSUBADD213PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xA7, regOp1, regOp2, arg, 1);
}
void XEmitter::VFMSUBADD231PD(X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteFMA3Op(0xB7, regOp1, regOp2, arg, 1);
}

#define FMA4(name, op)                                                                             \
  void XEmitter::name(X64Reg dest, X64Reg regOp1, X64Reg regOp2, const OpArg& arg)                 \
  {                                                                                                \
    WriteFMA4Op(op, dest, regOp1, regOp2, arg, 1);                                                 \
  }                                                                                                \
  void XEmitter::name(X64Reg dest, X64Reg regOp1, const OpArg& arg, X64Reg regOp2)                 \
  {                                                                                                \
    WriteFMA4Op(op, dest, regOp1, regOp2, arg, 0);                                                 \
  }

FMA4(VFMADDSUBPS, 0x5C)
FMA4(VFMADDSUBPD, 0x5D)
FMA4(VFMSUBADDPS, 0x5E)
FMA4(VFMSUBADDPD, 0x5F)
FMA4(VFMADDPS, 0x68)
FMA4(VFMADDPD, 0x69)
FMA4(VFMADDSS, 0x6A)
FMA4(VFMADDSD, 0x6B)
FMA4(VFMSUBPS, 0x6C)
FMA4(VFMSUBPD, 0x6D)
FMA4(VFMSUBSS, 0x6E)
FMA4(VFMSUBSD, 0x6F)
FMA4(VFNMADDPS, 0x78)
FMA4(VFNMADDPD, 0x79)
FMA4(VFNMADDSS, 0x7A)
FMA4(VFNMADDSD, 0x7B)
FMA4(VFNMSUBPS, 0x7C)
FMA4(VFNMSUBPD, 0x7D)
FMA4(VFNMSUBSS, 0x7E)
FMA4(VFNMSUBSD, 0x7F)
#undef FMA4

void XEmitter::SARX(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2)
{
  WriteBMI2Op(bits, 0xF3, 0x38F7, regOp1, regOp2, arg);
}
void XEmitter::SHLX(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2)
{
  WriteBMI2Op(bits, 0x66, 0x38F7, regOp1, regOp2, arg);
}
void XEmitter::SHRX(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2)
{
  WriteBMI2Op(bits, 0xF2, 0x38F7, regOp1, regOp2, arg);
}
void XEmitter::RORX(int bits, X64Reg regOp, const OpArg& arg, u8 rotate)
{
  WriteBMI2Op(bits, 0xF2, 0x3AF0, regOp, INVALID_REG, arg, 1);
  Write8(rotate);
}
void XEmitter::PEXT(int bits, X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteBMI2Op(bits, 0xF3, 0x38F5, regOp1, regOp2, arg);
}
void XEmitter::PDEP(int bits, X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteBMI2Op(bits, 0xF2, 0x38F5, regOp1, regOp2, arg);
}
void XEmitter::MULX(int bits, X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteBMI2Op(bits, 0xF2, 0x38F6, regOp2, regOp1, arg);
}
void XEmitter::BZHI(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2)
{
  CheckFlags();
  WriteBMI2Op(bits, 0x00, 0x38F5, regOp1, regOp2, arg);
}
void XEmitter::BLSR(int bits, X64Reg regOp, const OpArg& arg)
{
  WriteBMI1Op(bits, 0x00, 0x38F3, (X64Reg)0x1, regOp, arg);
}
void XEmitter::BLSMSK(int bits, X64Reg regOp, const OpArg& arg)
{
  WriteBMI1Op(bits, 0x00, 0x38F3, (X64Reg)0x2, regOp, arg);
}
void XEmitter::BLSI(int bits, X64Reg regOp, const OpArg& arg)
{
  WriteBMI1Op(bits, 0x00, 0x38F3, (X64Reg)0x3, regOp, arg);
}
void XEmitter::BEXTR(int bits, X64Reg regOp1, const OpArg& arg, X64Reg regOp2)
{
  WriteBMI1Op(bits, 0x00, 0x38F7, regOp1, regOp2, arg);
}
void XEmitter::ANDN(int bits, X64Reg regOp1, X64Reg regOp2, const OpArg& arg)
{
  WriteBMI1Op(bits, 0x00, 0x38F2, regOp1, regOp2, arg);
}

// Prefixes

void XEmitter::LOCK()
{
  Write8(0xF0);
}
void XEmitter::REP()
{
  Write8(0xF3);
}
void XEmitter::REPNE()
{
  Write8(0xF2);
}
void XEmitter::FSOverride()
{
  Write8(0x64);
}
void XEmitter::GSOverride()
{
  Write8(0x65);
}

void XEmitter::RDTSC()
{
  Write8(0x0F);
  Write8(0x31);
}
}  // namespace Gen
