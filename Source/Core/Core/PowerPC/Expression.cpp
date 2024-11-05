// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Expression.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fmt/format.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// https://github.com/zserge/expr/ is a C program and sorta valid C++.
// When included in a C++ program, it's treated as a C++ code, and it may cause
// issues: <cmath> may already be included, if so, including <math.h> may
// not do anything. <math.h> is obligated to put its functions in the global
// namespace, while <cmath> may or may not. The C code we're interpreting as
// C++ won't call functions by their qualified names. The code may work anyway
// if <cmath> puts its functions in the global namespace, or if the functions
// are actually macros that expand inline, both of which are common.
// NetBSD 10.0 i386 is an exception, and we need `using` there.
using std::isinf;
using std::isnan;
#include <expr.h>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

template <typename T>
static T HostRead(const Core::CPUThreadGuard& guard, u32 address);

template <typename T>
static void HostWrite(const Core::CPUThreadGuard& guard, T var, u32 address);

template <>
u8 HostRead(const Core::CPUThreadGuard& guard, u32 address)
{
  return PowerPC::MMU::HostRead_U8(guard, address);
}

template <>
u16 HostRead(const Core::CPUThreadGuard& guard, u32 address)
{
  return PowerPC::MMU::HostRead_U16(guard, address);
}

template <>
u32 HostRead(const Core::CPUThreadGuard& guard, u32 address)
{
  return PowerPC::MMU::HostRead_U32(guard, address);
}

template <>
u64 HostRead(const Core::CPUThreadGuard& guard, u32 address)
{
  return PowerPC::MMU::HostRead_U64(guard, address);
}

template <>
void HostWrite(const Core::CPUThreadGuard& guard, u8 var, u32 address)
{
  PowerPC::MMU::HostWrite_U8(guard, var, address);
}

template <>
void HostWrite(const Core::CPUThreadGuard& guard, u16 var, u32 address)
{
  PowerPC::MMU::HostWrite_U16(guard, var, address);
}

template <>
void HostWrite(const Core::CPUThreadGuard& guard, u32 var, u32 address)
{
  PowerPC::MMU::HostWrite_U32(guard, var, address);
}

template <>
void HostWrite(const Core::CPUThreadGuard& guard, u64 var, u32 address)
{
  PowerPC::MMU::HostWrite_U64(guard, var, address);
}

template <typename T, typename U = T>
static double HostReadFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 1)
    return 0;
  const u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 0)));

  Core::CPUThreadGuard guard(Core::System::GetInstance());
  return std::bit_cast<T>(HostRead<U>(guard, address));
}

template <typename T, typename U = T>
static double HostWriteFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 2)
    return 0;
  const T var = static_cast<T>(expr_eval(&vec_nth(args, 0)));
  const u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 1)));

  Core::CPUThreadGuard guard(Core::System::GetInstance());
  HostWrite<U>(guard, std::bit_cast<U>(var), address);
  return var;
}

template <typename T, typename U = T>
static double CastFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 1)
    return 0;
  return std::bit_cast<T>(static_cast<U>(expr_eval(&vec_nth(args, 0))));
}

static double CallstackFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 1)
    return 0;

  std::vector<Dolphin_Debugger::CallstackEntry> stack;
  {
    Core::CPUThreadGuard guard(Core::System::GetInstance());
    const bool success = Dolphin_Debugger::GetCallstack(guard, stack);
    if (!success)
      return 0;
  }

  double num = expr_eval(&vec_nth(args, 0));
  if (!std::isnan(num))
  {
    u32 address = static_cast<u32>(num);
    return std::any_of(stack.begin(), stack.end(),
                       [address](const auto& s) { return s.vAddress == address; });
  }

  const char* cstr = expr_get_str(&vec_nth(args, 0));
  if (cstr != nullptr)
  {
    return std::any_of(stack.begin(), stack.end(),
                       [cstr](const auto& s) { return s.Name.find(cstr) != std::string::npos; });
  }

  return 0;
}

static std::optional<std::string> ReadStringArg(const Core::CPUThreadGuard& guard, expr* e)
{
  double num = expr_eval(e);
  if (!std::isnan(num))
  {
    u32 address = static_cast<u32>(num);
    return PowerPC::MMU::HostGetString(guard, address);
  }

  const char* cstr = expr_get_str(e);
  if (cstr != nullptr)
  {
    return std::string(cstr);
  }

  return std::nullopt;
}

static double StreqFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 2)
    return 0;

  std::array<std::string, 2> strs;
  Core::CPUThreadGuard guard(Core::System::GetInstance());
  for (int i = 0; i < 2; i++)
  {
    std::optional<std::string> arg = ReadStringArg(guard, &vec_nth(args, i));
    if (arg == std::nullopt)
      return 0;

    strs[i] = std::move(*arg);
  }

  return strs[0] == strs[1];
}

static std::array<expr_func, 23> g_expr_funcs{{
    // For internal storage and comparisons, everything is auto-converted to Double.
    // If u64 ints are added, this could produce incorrect results.
    {"read_u8", HostReadFunc<u8>},
    {"read_s8", HostReadFunc<s8, u8>},
    {"read_u16", HostReadFunc<u16>},
    {"read_s16", HostReadFunc<s16, u16>},
    {"read_u32", HostReadFunc<u32>},
    {"read_s32", HostReadFunc<s32, u32>},
    {"read_f32", HostReadFunc<float, u32>},
    {"read_f64", HostReadFunc<double, u64>},
    {"write_u8", HostWriteFunc<u8>},
    {"write_u16", HostWriteFunc<u16>},
    {"write_u32", HostWriteFunc<u32>},
    {"write_f32", HostWriteFunc<float, u32>},
    {"write_f64", HostWriteFunc<double, u64>},
    {"u8", CastFunc<u8>},
    {"s8", CastFunc<s8, u8>},
    {"u16", CastFunc<u16>},
    {"s16", CastFunc<s16, u16>},
    {"u32", CastFunc<u32>},
    {"s32", CastFunc<s32, u32>},
    {"callstack", CallstackFunc},
    {"streq", StreqFunc},
    {},
}};

void ExprDeleter::operator()(expr* expression) const
{
  expr_destroy(expression, nullptr);
}

void ExprVarListDeleter::operator()(expr_var_list* vars) const
{
  // Free list elements
  expr_destroy(nullptr, vars);
  // Free list object
  delete vars;
}

Expression::Expression(std::string_view text, ExprPointer ex, ExprVarListPointer vars)
    : m_text(text), m_expr(std::move(ex)), m_vars(std::move(vars))
{
  using LookupKV = std::pair<std::string_view, Expression::VarBinding>;
  static constexpr auto sorted_lookup = []() consteval
  {
    using enum Expression::VarBindingType;
    auto unsorted_lookup = std::to_array<LookupKV>({
        {"r0", {GPR, 0}},
        {"r1", {GPR, 1}},
        {"r2", {GPR, 2}},
        {"r3", {GPR, 3}},
        {"r4", {GPR, 4}},
        {"r5", {GPR, 5}},
        {"r6", {GPR, 6}},
        {"r7", {GPR, 7}},
        {"r8", {GPR, 8}},
        {"r9", {GPR, 9}},
        {"r10", {GPR, 10}},
        {"r11", {GPR, 11}},
        {"r12", {GPR, 12}},
        {"r13", {GPR, 13}},
        {"r14", {GPR, 14}},
        {"r15", {GPR, 15}},
        {"r16", {GPR, 16}},
        {"r17", {GPR, 17}},
        {"r18", {GPR, 18}},
        {"r19", {GPR, 19}},
        {"r20", {GPR, 20}},
        {"r21", {GPR, 21}},
        {"r22", {GPR, 22}},
        {"r23", {GPR, 23}},
        {"r24", {GPR, 24}},
        {"r25", {GPR, 25}},
        {"r26", {GPR, 26}},
        {"r27", {GPR, 27}},
        {"r28", {GPR, 28}},
        {"r29", {GPR, 29}},
        {"r30", {GPR, 30}},
        {"r31", {GPR, 31}},
        {"f0", {FPR, 0}},
        {"f1", {FPR, 1}},
        {"f2", {FPR, 2}},
        {"f3", {FPR, 3}},
        {"f4", {FPR, 4}},
        {"f5", {FPR, 5}},
        {"f6", {FPR, 6}},
        {"f7", {FPR, 7}},
        {"f8", {FPR, 8}},
        {"f9", {FPR, 9}},
        {"f10", {FPR, 10}},
        {"f11", {FPR, 11}},
        {"f12", {FPR, 12}},
        {"f13", {FPR, 13}},
        {"f14", {FPR, 14}},
        {"f15", {FPR, 15}},
        {"f16", {FPR, 16}},
        {"f17", {FPR, 17}},
        {"f18", {FPR, 18}},
        {"f19", {FPR, 19}},
        {"f20", {FPR, 20}},
        {"f21", {FPR, 21}},
        {"f22", {FPR, 22}},
        {"f23", {FPR, 23}},
        {"f24", {FPR, 24}},
        {"f25", {FPR, 25}},
        {"f26", {FPR, 26}},
        {"f27", {FPR, 27}},
        {"f28", {FPR, 28}},
        {"f29", {FPR, 29}},
        {"f30", {FPR, 30}},
        {"f31", {FPR, 31}},
        {"pc", {PCtr}},
        {"msr", {MSR}},
        {"xer", {SPR, SPR_XER}},
        {"lr", {SPR, SPR_LR}},
        {"ctr", {SPR, SPR_CTR}},
        {"dsisr", {SPR, SPR_DSISR}},
        {"dar", {SPR, SPR_DAR}},
        {"dec", {SPR, SPR_DEC}},
        {"sdr1", {SPR, SPR_SDR}},
        {"srr0", {SPR, SPR_SRR0}},
        {"srr1", {SPR, SPR_SRR1}},
        {"tbl", {SPR, SPR_TL}},
        {"tbu", {SPR, SPR_TU}},
        {"pvr", {SPR, SPR_PVR}},
        {"sprg0", {SPR, SPR_SPRG0}},
        {"sprg1", {SPR, SPR_SPRG1}},
        {"sprg2", {SPR, SPR_SPRG2}},
        {"sprg3", {SPR, SPR_SPRG3}},
        {"ear", {SPR, SPR_EAR}},
        {"ibat0u", {SPR, SPR_IBAT0U}},
        {"ibat0l", {SPR, SPR_IBAT0L}},
        {"ibat1u", {SPR, SPR_IBAT1U}},
        {"ibat1l", {SPR, SPR_IBAT1L}},
        {"ibat2u", {SPR, SPR_IBAT2U}},
        {"ibat2l", {SPR, SPR_IBAT2L}},
        {"ibat3u", {SPR, SPR_IBAT3U}},
        {"ibat3l", {SPR, SPR_IBAT3L}},
        {"ibat4u", {SPR, SPR_IBAT4U}},
        {"ibat4l", {SPR, SPR_IBAT4L}},
        {"ibat5u", {SPR, SPR_IBAT5U}},
        {"ibat5l", {SPR, SPR_IBAT5L}},
        {"ibat6u", {SPR, SPR_IBAT6U}},
        {"ibat6l", {SPR, SPR_IBAT6L}},
        {"ibat7u", {SPR, SPR_IBAT7U}},
        {"ibat7l", {SPR, SPR_IBAT7L}},
        {"dbat0u", {SPR, SPR_DBAT0U}},
        {"dbat0l", {SPR, SPR_DBAT0L}},
        {"dbat1u", {SPR, SPR_DBAT1U}},
        {"dbat1l", {SPR, SPR_DBAT1L}},
        {"dbat2u", {SPR, SPR_DBAT2U}},
        {"dbat2l", {SPR, SPR_DBAT2L}},
        {"dbat3u", {SPR, SPR_DBAT3U}},
        {"dbat3l", {SPR, SPR_DBAT3L}},
        {"dbat4u", {SPR, SPR_DBAT4U}},
        {"dbat4l", {SPR, SPR_DBAT4L}},
        {"dbat5u", {SPR, SPR_DBAT5U}},
        {"dbat5l", {SPR, SPR_DBAT5L}},
        {"dbat6u", {SPR, SPR_DBAT6U}},
        {"dbat6l", {SPR, SPR_DBAT6L}},
        {"dbat7u", {SPR, SPR_DBAT7U}},
        {"dbat7l", {SPR, SPR_DBAT7L}},
        {"gqr0", {SPR, SPR_GQR0 + 0}},
        {"gqr1", {SPR, SPR_GQR0 + 1}},
        {"gqr2", {SPR, SPR_GQR0 + 2}},
        {"gqr3", {SPR, SPR_GQR0 + 3}},
        {"gqr4", {SPR, SPR_GQR0 + 4}},
        {"gqr5", {SPR, SPR_GQR0 + 5}},
        {"gqr6", {SPR, SPR_GQR0 + 6}},
        {"gqr7", {SPR, SPR_GQR0 + 7}},
        {"hid0", {SPR, SPR_HID0}},
        {"hid1", {SPR, SPR_HID1}},
        {"hid2", {SPR, SPR_HID2}},
        {"hid4", {SPR, SPR_HID4}},
        {"iabr", {SPR, SPR_IABR}},
        {"dabr", {SPR, SPR_DABR}},
        {"wpar", {SPR, SPR_WPAR}},
        {"dmau", {SPR, SPR_DMAU}},
        {"dmal", {SPR, SPR_DMAL}},
        {"ecid_u", {SPR, SPR_ECID_U}},
        {"ecid_m", {SPR, SPR_ECID_M}},
        {"ecid_l", {SPR, SPR_ECID_L}},
        {"usia", {SPR, SPR_USIA}},
        {"sia", {SPR, SPR_SIA}},
        {"l2cr", {SPR, SPR_L2CR}},
        {"ictc", {SPR, SPR_ICTC}},
        {"mmcr0", {SPR, SPR_MMCR0}},
        {"mmcr1", {SPR, SPR_MMCR1}},
        {"pmc1", {SPR, SPR_PMC1}},
        {"pmc2", {SPR, SPR_PMC2}},
        {"pmc3", {SPR, SPR_PMC3}},
        {"pmc4", {SPR, SPR_PMC4}},
        {"thrm1", {SPR, SPR_THRM1}},
        {"thrm2", {SPR, SPR_THRM2}},
        {"thrm3", {SPR, SPR_THRM3}},
    });
    std::ranges::sort(unsorted_lookup, {}, &LookupKV::first);
    return unsorted_lookup;
  }
  ();
  static_assert(std::ranges::adjacent_find(sorted_lookup, {}, &LookupKV::first) ==
                    sorted_lookup.end(),
                "Expression: Sorted lookup should not contain duplicate keys.");
  for (auto* v = m_vars->head; v != nullptr; v = v->next)
  {
    const auto iter = std::ranges::lower_bound(sorted_lookup, v->name, {}, &LookupKV::first);
    if (iter != sorted_lookup.end() && iter->first == v->name)
      m_binds.emplace_back(iter->second);
    else
      m_binds.emplace_back();
  }
}

std::optional<Expression> Expression::TryParse(std::string_view text)
{
  ExprVarListPointer vars{new expr_var_list{}};
  ExprPointer ex{expr_create(text.data(), text.length(), vars.get(), g_expr_funcs.data())};
  if (!ex)
    return std::nullopt;

  return Expression{text, std::move(ex), std::move(vars)};
}

double Expression::Evaluate(Core::System& system) const
{
  SynchronizeBindings(system, SynchronizeDirection::From);

  double result = expr_eval(m_expr.get());

  SynchronizeBindings(system, SynchronizeDirection::To);

  Reporting(result);

  return result;
}

void Expression::SynchronizeBindings(Core::System& system, SynchronizeDirection dir) const
{
  auto& ppc_state = system.GetPPCState();
  auto bind = m_binds.begin();
  for (auto* v = m_vars->head; v != nullptr; v = v->next, ++bind)
  {
    switch (bind->type)
    {
    case VarBindingType::Zero:
      if (dir == SynchronizeDirection::From)
        v->value = 0;
      break;
    case VarBindingType::GPR:
      if (dir == SynchronizeDirection::From)
        v->value = static_cast<double>(ppc_state.gpr[bind->index]);
      else
        ppc_state.gpr[bind->index] = static_cast<u32>(static_cast<s64>(v->value));
      break;
    case VarBindingType::FPR:
      if (dir == SynchronizeDirection::From)
        v->value = ppc_state.ps[bind->index].PS0AsDouble();
      else
        ppc_state.ps[bind->index].SetPS0(v->value);
      break;
    case VarBindingType::SPR:
      if (dir == SynchronizeDirection::From)
        v->value = static_cast<double>(ppc_state.spr[bind->index]);
      else
        ppc_state.spr[bind->index] = static_cast<u32>(static_cast<s64>(v->value));
      break;
    case VarBindingType::PCtr:
      if (dir == SynchronizeDirection::From)
        v->value = static_cast<double>(ppc_state.pc);
      break;
    case VarBindingType::MSR:
      if (dir == SynchronizeDirection::From)
        v->value = static_cast<double>(ppc_state.msr.Hex);
      else
        ppc_state.msr.Hex = static_cast<u32>(static_cast<s64>(v->value));
      break;
    }
  }
}

void Expression::Reporting(const double result) const
{
  bool is_nan = std::isnan(result);
  std::string message;

  for (auto* v = m_vars->head; v != nullptr; v = v->next)
  {
    if (std::isnan(v->value))
      is_nan = true;

    fmt::format_to(std::back_inserter(message), "  {}={}", v->name, v->value);
  }

  if (is_nan)
  {
    message.append("\nBreakpoint condition encountered a NaN");
    Core::DisplayMessage("Breakpoint condition has encountered a NaN.", 2000);
  }

  if (result != 0.0 || is_nan)
    NOTICE_LOG_FMT(MEMMAP, "Breakpoint condition returned: {}. Vars:{}", result, message);
}

std::string Expression::GetText() const
{
  return m_text;
}
