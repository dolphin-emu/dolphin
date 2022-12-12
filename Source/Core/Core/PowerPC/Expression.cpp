// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Expression.h"

#include <algorithm>
#include <cstdlib>
#include <fmt/format.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <expr.h>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/Debugger/Debugger_SymbolMap.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

template <typename T>
static T HostRead(u32 address);

template <typename T>
static void HostWrite(T var, u32 address);

template <>
u8 HostRead(u32 address)
{
  return PowerPC::HostRead_U8(address);
}

template <>
u16 HostRead(u32 address)
{
  return PowerPC::HostRead_U16(address);
}

template <>
u32 HostRead(u32 address)
{
  return PowerPC::HostRead_U32(address);
}

template <>
u64 HostRead(u32 address)
{
  return PowerPC::HostRead_U64(address);
}

template <>
void HostWrite(u8 var, u32 address)
{
  PowerPC::HostWrite_U8(var, address);
}

template <>
void HostWrite(u16 var, u32 address)
{
  PowerPC::HostWrite_U16(var, address);
}

template <>
void HostWrite(u32 var, u32 address)
{
  PowerPC::HostWrite_U32(var, address);
}

template <>
void HostWrite(u64 var, u32 address)
{
  PowerPC::HostWrite_U64(var, address);
}

template <typename T, typename U = T>
static double HostReadFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 1)
    return 0;
  const u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 0)));
  return Common::BitCast<T>(HostRead<U>(address));
}

template <typename T, typename U = T>
static double HostWriteFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 2)
    return 0;
  const T var = static_cast<T>(expr_eval(&vec_nth(args, 0)));
  const u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 1)));
  HostWrite<U>(Common::BitCast<U>(var), address);
  return var;
}

template <typename T, typename U = T>
static double CastFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 1)
    return 0;
  return Common::BitCast<T>(static_cast<U>(expr_eval(&vec_nth(args, 0))));
}

static double CallstackFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 1)
    return 0;

  std::vector<Dolphin_Debugger::CallstackEntry> stack;
  bool success = Dolphin_Debugger::GetCallstack(stack);
  if (!success)
    return 0;

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

static std::array<expr_func, 22> g_expr_funcs{{
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
  for (auto* v = m_vars->head; v != nullptr; v = v->next)
  {
    const std::string_view name = v->name;
    VarBinding bind;

    if (name.length() >= 2 && name.length() <= 3)
    {
      if (name[0] == 'r' || name[0] == 'f')
      {
        char* end = nullptr;
        const int index = std::strtol(name.data() + 1, &end, 10);
        if (index >= 0 && index <= 31 && end == name.data() + name.length())
        {
          bind.type = name[0] == 'r' ? VarBindingType::GPR : VarBindingType::FPR;
          bind.index = index;
        }
      }
      else if (name == "lr")
      {
        bind.type = VarBindingType::SPR;
        bind.index = SPR_LR;
      }
      else if (name == "ctr")
      {
        bind.type = VarBindingType::SPR;
        bind.index = SPR_CTR;
      }
      else if (name == "pc")
      {
        bind.type = VarBindingType::PCtr;
      }
    }

    m_binds.emplace_back(bind);
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

double Expression::Evaluate() const
{
  SynchronizeBindings(SynchronizeDirection::From);

  double result = expr_eval(m_expr.get());

  SynchronizeBindings(SynchronizeDirection::To);

  Reporting(result);

  return result;
}

void Expression::SynchronizeBindings(SynchronizeDirection dir) const
{
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
        v->value = static_cast<double>(GPR(bind->index));
      else
        GPR(bind->index) = static_cast<u32>(static_cast<s64>(v->value));
      break;
    case VarBindingType::FPR:
      if (dir == SynchronizeDirection::From)
        v->value = rPS(bind->index).PS0AsDouble();
      else
        rPS(bind->index).SetPS0(v->value);
      break;
    case VarBindingType::SPR:
      if (dir == SynchronizeDirection::From)
        v->value = static_cast<double>(rSPR(bind->index));
      else
        rSPR(bind->index) = static_cast<u32>(static_cast<s64>(v->value));
      break;
    case VarBindingType::PCtr:
      if (dir == SynchronizeDirection::From)
        v->value = static_cast<double>(PC);
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
