// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/PowerPC.h"

// Instructions which move data without performing operations round a bit weirdly
// Specifically they rounding the mantissa to be like that of a 32-bit float,
// going as far as to focus on the rounding mode, but never actually care about
// making sure the exponent becomes 32-bit
// Either this, or they'll truncate the mantissa down, which will always happen to
// PS1 OR PS0 in ps_rsqrte
inline u64 TruncateMantissaBits(u64 bits)
{
  // Truncation can be done by simply cutting off the mantissa bits that don't
  // exist in a single precision float
  constexpr u64 remove_bits = Common::DOUBLE_FRAC_WIDTH - Common::FLOAT_FRAC_WIDTH;
  constexpr u64 remove_mask = (1 << remove_bits) - 1;
  return bits & ~remove_mask;
}

inline double TruncateMantissa(double value)
{
  u64 bits = std::bit_cast<u64>(value);
  u64 trunc_bits = TruncateMantissaBits(bits);
  return std::bit_cast<double>(trunc_bits);
}

inline u64 RoundMantissaBits(u64 bits)
{
  // Checking if the value is non-finite
  if ((bits & Common::DOUBLE_EXP) == Common::DOUBLE_EXP)
  {
    // For infinite and NaN values, the mantissa is simply truncated
    return TruncateMantissaBits(bits);
  }

  const u64 replacement_exp = 0x4000000000000000ull;

  // To round only the mantissa, we assume the CPU can change the rounding mode,
  // create new double with an exponent that won't cause issues, round to a single,
  // and convert back to a double while restoring the original exponent again!
  // The removing the exponent is done via subtraction instead of bitwise
  // operations due to the possibility that the rounding will cause an overflow
  // into the exponent
  u64 resized_bits = (bits & (Common::DOUBLE_FRAC | Common::DOUBLE_SIGN)) | replacement_exp;

  float rounded_float = static_cast<float>(std::bit_cast<double>(resized_bits));
  double extended_float = static_cast<double>(rounded_float);
  u64 rounded_bits = std::bit_cast<u64>(extended_float);

  u64 orig_exp_bits = bits & Common::DOUBLE_EXP;
  rounded_bits = (rounded_bits - replacement_exp) | orig_exp_bits;

  return rounded_bits;
}

inline double RoundMantissa(double value)
{
  // The double version of the function just converts to and from bits again
  // This would be a necessary step anyways, so it just simplifies code
  u64 bits = std::bit_cast<u64>(value);
  u64 rounded_bits = RoundMantissaBits(bits);
  return std::bit_cast<double>(rounded_bits);
}

// These "binary instructions" do not alter FPSCR.
void Interpreter::ps_sel(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  double ps0 = a.PS0AsDouble() >= -0.0 ? c.PS0AsDouble() : b.PS0AsDouble();
  double ps1 = a.PS1AsDouble() >= -0.0 ? c.PS1AsDouble() : b.PS1AsDouble();
  ppc_state.ps[inst.FD].SetBoth(RoundMantissa(ps0), TruncateMantissa(ps1));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_neg(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& b = ppc_state.ps[inst.FB];

  u64 ps0 = b.PS0AsU64() ^ (UINT64_C(1) << 63);
  u64 ps1 = b.PS1AsU64() ^ (UINT64_C(1) << 63);
  ppc_state.ps[inst.FD].SetBoth(RoundMantissaBits(ps0), TruncateMantissaBits(ps1));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_mr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(RoundMantissa(b.PS0AsDouble()), TruncateMantissa(b.PS1AsDouble()));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_nabs(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& b = ppc_state.ps[inst.FB];

  u64 ps0 = b.PS0AsU64() | (UINT64_C(1) << 63);
  u64 ps1 = b.PS1AsU64() | (UINT64_C(1) << 63);
  ppc_state.ps[inst.FD].SetBoth(RoundMantissaBits(ps0), TruncateMantissaBits(ps1));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_abs(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& b = ppc_state.ps[inst.FB];

  u64 ps0 = b.PS0AsU64() & ~(UINT64_C(1) << 63);
  u64 ps1 = b.PS1AsU64() & ~(UINT64_C(1) << 63);
  ppc_state.ps[inst.FD].SetBoth(RoundMantissaBits(ps0), TruncateMantissaBits(ps1));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(RoundMantissa(a.PS0AsDouble()), TruncateMantissa(b.PS0AsDouble()));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_merge01(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(RoundMantissa(a.PS0AsDouble()), TruncateMantissa(b.PS1AsDouble()));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_merge10(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(RoundMantissa(a.PS1AsDouble()), TruncateMantissa(b.PS0AsDouble()));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_merge11(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(RoundMantissa(a.PS1AsDouble()), TruncateMantissa(b.PS1AsDouble()));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

// From here on, the real deal.
void Interpreter::ps_div(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_div(ppc_state, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_div(ppc_state, a.PS1AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_res(Interpreter& interpreter, UGeckoInstruction inst)
{
  // this code is based on the real hardware tests
  auto& ppc_state = interpreter.m_ppc_state;
  const double a = ppc_state.ps[inst.FB].PS0AsDouble();
  const double b = ppc_state.ps[inst.FB].PS1AsDouble();

  if (a == 0.0 || b == 0.0)
  {
    SetFPException(ppc_state, FPSCR_ZX);
    ppc_state.fpscr.ClearFIFR();
  }

  if (std::isnan(a) || std::isinf(a) || std::isnan(b) || std::isinf(b))
    ppc_state.fpscr.ClearFIFR();

  if (Common::IsSNAN(a) || Common::IsSNAN(b))
    SetFPException(ppc_state, FPSCR_VXSNAN);

  const double ps0 = Common::ApproximateReciprocal(a);
  const double ps1 = Common::ApproximateReciprocal(b);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(float(ps0));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_rsqrte(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const double ps0 = ppc_state.ps[inst.FB].PS0AsDouble();
  const double ps1 = ppc_state.ps[inst.FB].PS1AsDouble();

  if (ps0 == 0.0 || ps1 == 0.0)
  {
    SetFPException(ppc_state, FPSCR_ZX);
    ppc_state.fpscr.ClearFIFR();
  }

  if (ps0 < 0.0 || ps1 < 0.0)
  {
    SetFPException(ppc_state, FPSCR_VXSQRT);
    ppc_state.fpscr.ClearFIFR();
  }

  if (std::isnan(ps0) || std::isinf(ps0) || std::isnan(ps1) || std::isinf(ps1))
    ppc_state.fpscr.ClearFIFR();

  if (Common::IsSNAN(ps0) || Common::IsSNAN(ps1))
    SetFPException(ppc_state, FPSCR_VXSNAN);

  // For some reason ps0 is also truncated for this operation rather than rounded
  const double dst_ps0 = TruncateMantissa(Common::ApproximateReciprocalSquareRoot(ps0));
  const double dst_ps1 = TruncateMantissa(Common::ApproximateReciprocalSquareRoot(ps1));

  ppc_state.ps[inst.FD].SetBoth(dst_ps0, dst_ps1);
  ppc_state.UpdateFPRFSingle(dst_ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_sub(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_sub(ppc_state, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_sub(ppc_state, a.PS1AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_add(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_add(ppc_state, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_add(ppc_state, a.PS1AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_mul(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& c = ppc_state.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS0AsDouble(), c0).value);
  const float ps1 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS1AsDouble(), c1).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_msub(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const float ps0 = ForceSingle(
      ppc_state.fpscr,
      NI_msub<true>(ppc_state, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(
      ppc_state.fpscr,
      NI_msub<true>(ppc_state, a.PS1AsDouble(), c.PS1AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_madd(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const float ps0 = ForceSingle(
      ppc_state.fpscr,
      NI_madd<true>(ppc_state, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(
      ppc_state.fpscr,
      NI_madd<true>(ppc_state, a.PS1AsDouble(), c.PS1AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_nmsub(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const float tmp0 = ForceSingle(
      ppc_state.fpscr,
      NI_msub<true>(ppc_state, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble()).value);
  const float tmp1 = ForceSingle(
      ppc_state.fpscr,
      NI_msub<true>(ppc_state, a.PS1AsDouble(), c.PS1AsDouble(), b.PS1AsDouble()).value);

  const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_nmadd(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const float tmp0 = ForceSingle(
      ppc_state.fpscr,
      NI_madd<true>(ppc_state, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble()).value);
  const float tmp1 = ForceSingle(
      ppc_state.fpscr,
      NI_madd<true>(ppc_state, a.PS1AsDouble(), c.PS1AsDouble(), b.PS1AsDouble()).value);

  const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_sum0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_add(ppc_state, a.PS0AsDouble(), b.PS1AsDouble()).value);
  const double ps1 = TruncateMantissa(c.PS1AsDouble());

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_sum1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const double ps0 = RoundMantissa(c.PS0AsDouble());
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_add(ppc_state, a.PS0AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps1);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_muls0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& c = ppc_state.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const float ps0 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS0AsDouble(), c0).value);
  const float ps1 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS1AsDouble(), c0).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_muls1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& c = ppc_state.ps[inst.FC];

  const double c1 = Force25Bit(c.PS1AsDouble());
  const float ps0 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS0AsDouble(), c1).value);
  const float ps1 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS1AsDouble(), c1).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_madds0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const float ps0 = ForceSingle(
      ppc_state.fpscr,
      NI_madd<true>(ppc_state, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(
      ppc_state.fpscr,
      NI_madd<true>(ppc_state, a.PS1AsDouble(), c.PS0AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_madds1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const float ps0 = ForceSingle(
      ppc_state.fpscr,
      NI_madd<true>(ppc_state, a.PS0AsDouble(), c.PS1AsDouble(), b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(
      ppc_state.fpscr,
      NI_madd<true>(ppc_state, a.PS1AsDouble(), c.PS1AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_cmpu0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  Helper_FloatCompareUnordered(ppc_state, inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpo0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  Helper_FloatCompareOrdered(ppc_state, inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpu1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  Helper_FloatCompareUnordered(ppc_state, inst, a.PS1AsDouble(), b.PS1AsDouble());
}

void Interpreter::ps_cmpo1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  Helper_FloatCompareOrdered(ppc_state, inst, a.PS1AsDouble(), b.PS1AsDouble());
}
