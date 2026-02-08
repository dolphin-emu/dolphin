// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <algorithm>
#include <bit>
#include <tuple>
#include <type_traits>
#include <utility>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

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
SType ScaleAndClamp(double ps, u32 st_scale)
{
  const float conv_ps = float(ps) * m_quantizeTable[st_scale];
  constexpr float min = float(std::numeric_limits<SType>::min());
  constexpr float max = float(std::numeric_limits<SType>::max());

  return SType(std::clamp(conv_ps, min, max));
}

template <typename T>
static T ReadUnpaired(PowerPC::MMU& mmu, u32 addr);

template <>
u8 ReadUnpaired<u8>(PowerPC::MMU& mmu, u32 addr)
{
  return mmu.Read_U8(addr);
}

template <>
u16 ReadUnpaired<u16>(PowerPC::MMU& mmu, u32 addr)
{
  return mmu.Read_U16(addr);
}

template <>
u32 ReadUnpaired<u32>(PowerPC::MMU& mmu, u32 addr)
{
  return mmu.Read_U32(addr);
}

template <typename T>
static std::pair<T, T> ReadPair(PowerPC::MMU& mmu, u32 addr);

template <>
std::pair<u8, u8> ReadPair<u8>(PowerPC::MMU& mmu, u32 addr)
{
  const u16 val = mmu.Read_U16(addr);
  return {u8(val >> 8), u8(val)};
}

template <>
std::pair<u16, u16> ReadPair<u16>(PowerPC::MMU& mmu, u32 addr)
{
  const u32 val = mmu.Read_U32(addr);
  return {u16(val >> 16), u16(val)};
}

template <>
std::pair<u32, u32> ReadPair<u32>(PowerPC::MMU& mmu, u32 addr)
{
  const u64 val = mmu.Read_U64(addr);
  return {u32(val >> 32), u32(val)};
}

template <typename T>
static void WriteUnpaired(PowerPC::MMU& mmu, T val, u32 addr);

template <>
void WriteUnpaired<u8>(PowerPC::MMU& mmu, u8 val, u32 addr)
{
  mmu.Write_U8(val, addr);
}

template <>
void WriteUnpaired<u16>(PowerPC::MMU& mmu, u16 val, u32 addr)
{
  mmu.Write_U16(val, addr);
}

template <>
void WriteUnpaired<u32>(PowerPC::MMU& mmu, u32 val, u32 addr)
{
  mmu.Write_U32(val, addr);
}

template <typename T>
static void WritePair(PowerPC::MMU& mmu, T val1, T val2, u32 addr);

template <>
void WritePair<u8>(PowerPC::MMU& mmu, u8 val1, u8 val2, u32 addr)
{
  mmu.Write_U16((u16{val1} << 8) | u16{val2}, addr);
}

template <>
void WritePair<u16>(PowerPC::MMU& mmu, u16 val1, u16 val2, u32 addr)
{
  mmu.Write_U32((u32{val1} << 16) | u32{val2}, addr);
}

template <>
void WritePair<u32>(PowerPC::MMU& mmu, u32 val1, u32 val2, u32 addr)
{
  mmu.Write_U64((u64{val1} << 32) | u64{val2}, addr);
}

template <typename T>
void QuantizeAndStore(PowerPC::MMU& mmu, double ps0, double ps1, u32 addr, u32 instW, u32 st_scale)
{
  using U = std::make_unsigned_t<T>;

  const U conv_ps0 = U(ScaleAndClamp<T>(ps0, st_scale));
  if (instW)
  {
    WriteUnpaired<U>(mmu, conv_ps0, addr);
  }
  else
  {
    const U conv_ps1 = U(ScaleAndClamp<T>(ps1, st_scale));
    WritePair<U>(mmu, conv_ps0, conv_ps1, addr);
  }
}

static void Helper_Quantize(PowerPC::MMU& mmu, const PowerPC::PowerPCState* ppcs, u32 addr,
                            u32 instI, u32 instRS, u32 instW)
{
  const UGQR gqr(ppcs->spr[SPR_GQR0 + instI]);
  const EQuantizeType st_type = gqr.st_type;
  const u32 st_scale = gqr.st_scale;

  const double ps0 = ppcs->ps[instRS].PS0AsDouble();
  const double ps1 = ppcs->ps[instRS].PS1AsDouble();

  switch (st_type)
  {
  case QUANTIZE_FLOAT:
  {
    const u64 integral_ps0 = std::bit_cast<u64>(ps0);
    const u32 conv_ps0 = ConvertToSingleFTZ(integral_ps0);

    if (instW != 0)
    {
      WriteUnpaired<u32>(mmu, conv_ps0, addr);
    }
    else
    {
      const u64 integral_ps1 = std::bit_cast<u64>(ps1);
      const u32 conv_ps1 = ConvertToSingleFTZ(integral_ps1);

      WritePair<u32>(mmu, conv_ps0, conv_ps1, addr);
    }
    break;
  }

  case QUANTIZE_U8:
    QuantizeAndStore<u8>(mmu, ps0, ps1, addr, instW, st_scale);
    break;

  case QUANTIZE_U16:
    QuantizeAndStore<u16>(mmu, ps0, ps1, addr, instW, st_scale);
    break;

  case QUANTIZE_S8:
    QuantizeAndStore<s8>(mmu, ps0, ps1, addr, instW, st_scale);
    break;

  case QUANTIZE_S16:
    QuantizeAndStore<s16>(mmu, ps0, ps1, addr, instW, st_scale);
    break;

  case QUANTIZE_INVALID1:
  case QUANTIZE_INVALID2:
  case QUANTIZE_INVALID3:
    ASSERT_MSG(POWERPC, 0, "PS dequantize - unknown type to read");
    break;
  }
}

template <typename T>
std::pair<double, double> LoadAndDequantize(PowerPC::MMU& mmu, u32 addr, u32 instW, u32 ld_scale)
{
  using U = std::make_unsigned_t<T>;

  float ps0, ps1;
  if (instW != 0)
  {
    const U value = ReadUnpaired<U>(mmu, addr);
    ps0 = float(T(value)) * m_dequantizeTable[ld_scale];
    ps1 = 1.0f;
  }
  else
  {
    const auto [first, second] = ReadPair<U>(mmu, addr);
    ps0 = float(T(first)) * m_dequantizeTable[ld_scale];
    ps1 = float(T(second)) * m_dequantizeTable[ld_scale];
  }
  // ps0 and ps1 always contain finite and normal numbers. So we can just cast them to double
  return {static_cast<double>(ps0), static_cast<double>(ps1)};
}

static void Helper_Dequantize(PowerPC::MMU& mmu, PowerPC::PowerPCState* ppcs, u32 addr, u32 instI,
                              u32 instRD, u32 instW)
{
  const UGQR gqr(ppcs->spr[SPR_GQR0 + instI]);
  const EQuantizeType ld_type = gqr.ld_type;
  const u32 ld_scale = gqr.ld_scale;

  double ps0 = 0.0;
  double ps1 = 0.0;

  switch (ld_type)
  {
  case QUANTIZE_FLOAT:
    if (instW != 0)
    {
      const u32 value = ReadUnpaired<u32>(mmu, addr);
      ps0 = std::bit_cast<double>(ConvertToDouble(value));
      ps1 = 1.0;
    }
    else
    {
      const auto [first, second] = ReadPair<u32>(mmu, addr);
      ps0 = std::bit_cast<double>(ConvertToDouble(first));
      ps1 = std::bit_cast<double>(ConvertToDouble(second));
    }
    break;

  case QUANTIZE_U8:
    std::tie(ps0, ps1) = LoadAndDequantize<u8>(mmu, addr, instW, ld_scale);
    break;

  case QUANTIZE_U16:
    std::tie(ps0, ps1) = LoadAndDequantize<u16>(mmu, addr, instW, ld_scale);
    break;

  case QUANTIZE_S8:
    std::tie(ps0, ps1) = LoadAndDequantize<s8>(mmu, addr, instW, ld_scale);
    break;

  case QUANTIZE_S16:
    std::tie(ps0, ps1) = LoadAndDequantize<s16>(mmu, addr, instW, ld_scale);
    break;

  case QUANTIZE_INVALID1:
  case QUANTIZE_INVALID2:
  case QUANTIZE_INVALID3:
    ASSERT_MSG(POWERPC, 0, "PS dequantize - unknown type to read");
    ps0 = 0.0;
    ps1 = 0.0;
    break;
  }

  if ((ppcs->Exceptions & EXCEPTION_DSI) != 0)
  {
    return;
  }

  ppcs->ps[instRD].SetBoth(ps0, ps1);
}

void Interpreter::psq_l(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (HID2(ppc_state).LSQE == 0)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::IllegalInstruction);
    return;
  }

  const u32 EA = inst.RA ? (ppc_state.gpr[inst.RA] + u32(inst.SIMM_12)) : u32(inst.SIMM_12);
  Helper_Dequantize(interpreter.m_mmu, &ppc_state, EA, inst.I, inst.RD, inst.W);
}

void Interpreter::psq_lu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (HID2(ppc_state).LSQE == 0)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::IllegalInstruction);
    return;
  }

  const u32 EA = ppc_state.gpr[inst.RA] + u32(inst.SIMM_12);
  Helper_Dequantize(interpreter.m_mmu, &ppc_state, EA, inst.I, inst.RD, inst.W);

  if ((ppc_state.Exceptions & EXCEPTION_DSI) != 0)
  {
    return;
  }

  ppc_state.gpr[inst.RA] = EA;
}

void Interpreter::psq_st(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (HID2(ppc_state).LSQE == 0)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::IllegalInstruction);
    return;
  }

  const u32 EA = inst.RA ? (ppc_state.gpr[inst.RA] + u32(inst.SIMM_12)) : u32(inst.SIMM_12);
  Helper_Quantize(interpreter.m_mmu, &ppc_state, EA, inst.I, inst.RS, inst.W);
}

void Interpreter::psq_stu(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  if (HID2(ppc_state).LSQE == 0)
  {
    GenerateProgramException(ppc_state, ProgramExceptionCause::IllegalInstruction);
    return;
  }

  const u32 EA = ppc_state.gpr[inst.RA] + u32(inst.SIMM_12);
  Helper_Quantize(interpreter.m_mmu, &ppc_state, EA, inst.I, inst.RS, inst.W);

  if ((ppc_state.Exceptions & EXCEPTION_DSI) != 0)
  {
    return;
  }

  ppc_state.gpr[inst.RA] = EA;
}

void Interpreter::psq_lx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 EA =
      inst.RA ? (ppc_state.gpr[inst.RA] + ppc_state.gpr[inst.RB]) : ppc_state.gpr[inst.RB];
  Helper_Dequantize(interpreter.m_mmu, &ppc_state, EA, inst.Ix, inst.RD, inst.Wx);
}

void Interpreter::psq_stx(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 EA =
      inst.RA ? (ppc_state.gpr[inst.RA] + ppc_state.gpr[inst.RB]) : ppc_state.gpr[inst.RB];
  Helper_Quantize(interpreter.m_mmu, &ppc_state, EA, inst.Ix, inst.RS, inst.Wx);
}

void Interpreter::psq_lux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 EA = ppc_state.gpr[inst.RA] + ppc_state.gpr[inst.RB];
  Helper_Dequantize(interpreter.m_mmu, &ppc_state, EA, inst.Ix, inst.RD, inst.Wx);

  if ((ppc_state.Exceptions & EXCEPTION_DSI) != 0)
  {
    return;
  }

  ppc_state.gpr[inst.RA] = EA;
}

void Interpreter::psq_stux(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const u32 EA = ppc_state.gpr[inst.RA] + ppc_state.gpr[inst.RB];
  Helper_Quantize(interpreter.m_mmu, &ppc_state, EA, inst.Ix, inst.RS, inst.Wx);

  if ((ppc_state.Exceptions & EXCEPTION_DSI) != 0)
  {
    return;
  }

  ppc_state.gpr[inst.RA] = EA;
}
