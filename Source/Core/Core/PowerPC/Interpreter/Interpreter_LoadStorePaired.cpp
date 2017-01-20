// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <tuple>
#include <type_traits>
#include <utility>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/PowerPC.h"

// dequantize table
const float m_dequantizeTable[] = {
    1.0 / (1ULL << 0),  1.0 / (1ULL << 1),  1.0 / (1ULL << 2),  1.0 / (1ULL << 3),
    1.0 / (1ULL << 4),  1.0 / (1ULL << 5),  1.0 / (1ULL << 6),  1.0 / (1ULL << 7),
    1.0 / (1ULL << 8),  1.0 / (1ULL << 9),  1.0 / (1ULL << 10), 1.0 / (1ULL << 11),
    1.0 / (1ULL << 12), 1.0 / (1ULL << 13), 1.0 / (1ULL << 14), 1.0 / (1ULL << 15),
    1.0 / (1ULL << 16), 1.0 / (1ULL << 17), 1.0 / (1ULL << 18), 1.0 / (1ULL << 19),
    1.0 / (1ULL << 20), 1.0 / (1ULL << 21), 1.0 / (1ULL << 22), 1.0 / (1ULL << 23),
    1.0 / (1ULL << 24), 1.0 / (1ULL << 25), 1.0 / (1ULL << 26), 1.0 / (1ULL << 27),
    1.0 / (1ULL << 28), 1.0 / (1ULL << 29), 1.0 / (1ULL << 30), 1.0 / (1ULL << 31),
    (1ULL << 32),       (1ULL << 31),       (1ULL << 30),       (1ULL << 29),
    (1ULL << 28),       (1ULL << 27),       (1ULL << 26),       (1ULL << 25),
    (1ULL << 24),       (1ULL << 23),       (1ULL << 22),       (1ULL << 21),
    (1ULL << 20),       (1ULL << 19),       (1ULL << 18),       (1ULL << 17),
    (1ULL << 16),       (1ULL << 15),       (1ULL << 14),       (1ULL << 13),
    (1ULL << 12),       (1ULL << 11),       (1ULL << 10),       (1ULL << 9),
    (1ULL << 8),        (1ULL << 7),        (1ULL << 6),        (1ULL << 5),
    (1ULL << 4),        (1ULL << 3),        (1ULL << 2),        (1ULL << 1),
};

// quantize table
const float m_quantizeTable[] = {
    (1ULL << 0),        (1ULL << 1),        (1ULL << 2),        (1ULL << 3),
    (1ULL << 4),        (1ULL << 5),        (1ULL << 6),        (1ULL << 7),
    (1ULL << 8),        (1ULL << 9),        (1ULL << 10),       (1ULL << 11),
    (1ULL << 12),       (1ULL << 13),       (1ULL << 14),       (1ULL << 15),
    (1ULL << 16),       (1ULL << 17),       (1ULL << 18),       (1ULL << 19),
    (1ULL << 20),       (1ULL << 21),       (1ULL << 22),       (1ULL << 23),
    (1ULL << 24),       (1ULL << 25),       (1ULL << 26),       (1ULL << 27),
    (1ULL << 28),       (1ULL << 29),       (1ULL << 30),       (1ULL << 31),
    1.0 / (1ULL << 32), 1.0 / (1ULL << 31), 1.0 / (1ULL << 30), 1.0 / (1ULL << 29),
    1.0 / (1ULL << 28), 1.0 / (1ULL << 27), 1.0 / (1ULL << 26), 1.0 / (1ULL << 25),
    1.0 / (1ULL << 24), 1.0 / (1ULL << 23), 1.0 / (1ULL << 22), 1.0 / (1ULL << 21),
    1.0 / (1ULL << 20), 1.0 / (1ULL << 19), 1.0 / (1ULL << 18), 1.0 / (1ULL << 17),
    1.0 / (1ULL << 16), 1.0 / (1ULL << 15), 1.0 / (1ULL << 14), 1.0 / (1ULL << 13),
    1.0 / (1ULL << 12), 1.0 / (1ULL << 11), 1.0 / (1ULL << 10), 1.0 / (1ULL << 9),
    1.0 / (1ULL << 8),  1.0 / (1ULL << 7),  1.0 / (1ULL << 6),  1.0 / (1ULL << 5),
    1.0 / (1ULL << 4),  1.0 / (1ULL << 3),  1.0 / (1ULL << 2),  1.0 / (1ULL << 1),
};

template <typename SType>
SType ScaleAndClamp(double ps, u32 stScale)
{
  float convPS = (float)ps * m_quantizeTable[stScale];
  float min = (float)std::numeric_limits<SType>::min();
  float max = (float)std::numeric_limits<SType>::max();

  return (SType)MathUtil::Clamp(convPS, min, max);
}

template <typename T>
static T ReadUnpaired(u32 addr);

template <>
u8 ReadUnpaired<u8>(u32 addr)
{
  return PowerPC::Read_U8(addr);
}

template <>
u16 ReadUnpaired<u16>(u32 addr)
{
  return PowerPC::Read_U16(addr);
}

template <>
u32 ReadUnpaired<u32>(u32 addr)
{
  return PowerPC::Read_U32(addr);
}

template <typename T>
static std::pair<T, T> ReadPair(u32 addr);

template <>
std::pair<u8, u8> ReadPair<u8>(u32 addr)
{
  u16 val = PowerPC::Read_U16(addr);
  return {(u8)(val >> 8), (u8)val};
}

template <>
std::pair<u16, u16> ReadPair<u16>(u32 addr)
{
  u32 val = PowerPC::Read_U32(addr);
  return {(u16)(val >> 16), (u16)val};
}

template <>
std::pair<u32, u32> ReadPair<u32>(u32 addr)
{
  u64 val = PowerPC::Read_U64(addr);
  return {(u32)(val >> 32), (u32)val};
}

template <typename T>
static void WriteUnpaired(T val, u32 addr);

template <>
void WriteUnpaired<u8>(u8 val, u32 addr)
{
  PowerPC::Write_U8(val, addr);
}

template <>
void WriteUnpaired<u16>(u16 val, u32 addr)
{
  PowerPC::Write_U16(val, addr);
}

template <>
void WriteUnpaired<u32>(u32 val, u32 addr)
{
  PowerPC::Write_U32(val, addr);
}

template <typename T>
static void WritePair(T val1, T val2, u32 addr);

template <>
void WritePair<u8>(u8 val1, u8 val2, u32 addr)
{
  PowerPC::Write_U16(((u16)val1 << 8) | (u16)val2, addr);
}

template <>
void WritePair<u16>(u16 val1, u16 val2, u32 addr)
{
  PowerPC::Write_U32(((u32)val1 << 16) | (u32)val2, addr);
}

template <>
void WritePair<u32>(u32 val1, u32 val2, u32 addr)
{
  PowerPC::Write_U64(((u64)val1 << 32) | (u64)val2, addr);
}

template <typename T>
void QuantizeAndStore(double ps0, double ps1, u32 addr, u32 instW, u32 stScale)
{
  typedef typename std::make_unsigned<T>::type U;
  U convPS0 = (U)ScaleAndClamp<T>(ps0, stScale);
  if (instW)
  {
    WriteUnpaired<U>(convPS0, addr);
  }
  else
  {
    U convPS1 = (U)ScaleAndClamp<T>(ps1, stScale);
    WritePair<U>(convPS0, convPS1, addr);
  }
}

void Interpreter::Helper_Quantize(u32 addr, u32 instI, u32 instRS, u32 instW)
{
  const UGQR gqr(rSPR(SPR_GQR0 + instI));
  const EQuantizeType stType = gqr.st_type;
  const unsigned int stScale = gqr.st_scale;

  double ps0 = rPS0(instRS);
  double ps1 = rPS1(instRS);
  switch (stType)
  {
  case QUANTIZE_FLOAT:
  {
    u32 convPS0 = ConvertToSingleFTZ(MathUtil::IntDouble(ps0).i);
    if (instW)
    {
      WriteUnpaired<u32>(convPS0, addr);
    }
    else
    {
      u32 convPS1 = ConvertToSingleFTZ(MathUtil::IntDouble(ps1).i);
      WritePair<u32>(convPS0, convPS1, addr);
    }
    break;
  }

  case QUANTIZE_U8:
    QuantizeAndStore<u8>(ps0, ps1, addr, instW, stScale);
    break;

  case QUANTIZE_U16:
    QuantizeAndStore<u16>(ps0, ps1, addr, instW, stScale);
    break;

  case QUANTIZE_S8:
    QuantizeAndStore<s8>(ps0, ps1, addr, instW, stScale);
    break;

  case QUANTIZE_S16:
    QuantizeAndStore<s16>(ps0, ps1, addr, instW, stScale);
    break;

  case QUANTIZE_INVALID1:
  case QUANTIZE_INVALID2:
  case QUANTIZE_INVALID3:
    _assert_msg_(POWERPC, 0, "PS dequantize - unknown type to read");
    break;
  }
}

template <typename T>
std::pair<float, float> LoadAndDequantize(u32 addr, u32 instW, u32 ldScale)
{
  typedef typename std::make_unsigned<T>::type U;
  float ps0, ps1;
  if (instW)
  {
    U value = ReadUnpaired<U>(addr);
    ps0 = (float)(T)(value)*m_dequantizeTable[ldScale];
    ps1 = 1.0f;
  }
  else
  {
    std::pair<U, U> value = ReadPair<U>(addr);
    ps0 = (float)(T)(value.first) * m_dequantizeTable[ldScale];
    ps1 = (float)(T)(value.second) * m_dequantizeTable[ldScale];
  }
  return {ps0, ps1};
}

void Interpreter::Helper_Dequantize(u32 addr, u32 instI, u32 instRD, u32 instW)
{
  UGQR gqr(rSPR(SPR_GQR0 + instI));
  EQuantizeType ldType = gqr.ld_type;
  unsigned int ldScale = gqr.ld_scale;

  float ps0 = 0.0f;
  float ps1 = 0.0f;

  switch (ldType)
  {
  case QUANTIZE_FLOAT:
    if (instW)
    {
      u32 value = ReadUnpaired<u32>(addr);
      ps0 = MathUtil::IntFloat(value).f;
      ps1 = 1.0f;
    }
    else
    {
      std::pair<u32, u32> value = ReadPair<u32>(addr);
      ps0 = MathUtil::IntFloat(value.first).f;
      ps1 = MathUtil::IntFloat(value.second).f;
    }
    break;

  case QUANTIZE_U8:
    std::tie(ps0, ps1) = LoadAndDequantize<u8>(addr, instW, ldScale);
    break;

  case QUANTIZE_U16:
    std::tie(ps0, ps1) = LoadAndDequantize<u16>(addr, instW, ldScale);
    break;

  case QUANTIZE_S8:
    std::tie(ps0, ps1) = LoadAndDequantize<s8>(addr, instW, ldScale);
    break;

  case QUANTIZE_S16:
    std::tie(ps0, ps1) = LoadAndDequantize<s16>(addr, instW, ldScale);
    break;

  case QUANTIZE_INVALID1:
  case QUANTIZE_INVALID2:
  case QUANTIZE_INVALID3:
    _assert_msg_(POWERPC, 0, "PS dequantize - unknown type to read");
    ps0 = 0.f;
    ps1 = 0.f;
    break;
  }

  if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
  {
    return;
  }

  rPS0(instRD) = ps0;
  rPS1(instRD) = ps1;
}

void Interpreter::psq_l(UGeckoInstruction inst)
{
  const u32 EA = inst.RA ? (rGPR[inst.RA] + inst.SIMM_12) : (u32)inst.SIMM_12;
  Helper_Dequantize(EA, inst.I, inst.RD, inst.W);
}

void Interpreter::psq_lu(UGeckoInstruction inst)
{
  const u32 EA = rGPR[inst.RA] + inst.SIMM_12;
  Helper_Dequantize(EA, inst.I, inst.RD, inst.W);

  if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
  {
    return;
  }
  rGPR[inst.RA] = EA;
}

void Interpreter::psq_st(UGeckoInstruction inst)
{
  const u32 EA = inst.RA ? (rGPR[inst.RA] + inst.SIMM_12) : (u32)inst.SIMM_12;
  Helper_Quantize(EA, inst.I, inst.RS, inst.W);
}

void Interpreter::psq_stu(UGeckoInstruction inst)
{
  const u32 EA = rGPR[inst.RA] + inst.SIMM_12;
  Helper_Quantize(EA, inst.I, inst.RS, inst.W);

  if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
  {
    return;
  }
  rGPR[inst.RA] = EA;
}

void Interpreter::psq_lx(UGeckoInstruction inst)
{
  const u32 EA = inst.RA ? (rGPR[inst.RA] + rGPR[inst.RB]) : rGPR[inst.RB];
  Helper_Dequantize(EA, inst.Ix, inst.RD, inst.Wx);
}

void Interpreter::psq_stx(UGeckoInstruction inst)
{
  const u32 EA = inst.RA ? (rGPR[inst.RA] + rGPR[inst.RB]) : rGPR[inst.RB];
  Helper_Quantize(EA, inst.Ix, inst.RS, inst.Wx);
}

void Interpreter::psq_lux(UGeckoInstruction inst)
{
  const u32 EA = rGPR[inst.RA] + rGPR[inst.RB];
  Helper_Dequantize(EA, inst.Ix, inst.RD, inst.Wx);

  if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
  {
    return;
  }
  rGPR[inst.RA] = EA;
}

void Interpreter::psq_stux(UGeckoInstruction inst)
{
  const u32 EA = rGPR[inst.RA] + rGPR[inst.RB];
  Helper_Quantize(EA, inst.Ix, inst.RS, inst.Wx);

  if (PowerPC::ppcState.Exceptions & EXCEPTION_DSI)
  {
    return;
  }
  rGPR[inst.RA] = EA;
}
