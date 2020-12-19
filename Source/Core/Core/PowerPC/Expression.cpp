// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Expression.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <expr.h>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

static std::optional<int> ParseGPR(const char* name)
{
  if (std::strlen(name) >= 2 && name[0] == 'r' && std::isdigit(name[1]))
  {
    char* end = nullptr;
    int index = std::strtol(&name[1], &end, 10);
    if (index < 32 && *end == '\0')
      return index;
  }
  return {};
}

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
double HostReadFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 1)
    return 0;
  u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 0)));
  return Common::BitCast<T>(HostRead<U>(address));
}

template <typename T, typename U = T>
double HostWriteFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 2)
    return 0;
  T var = static_cast<T>(expr_eval(&vec_nth(args, 0)));
  u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 1)));
  HostWrite<U>(Common::BitCast<U>(var), address);
  return var;
}

static expr_func g_expr_funcs[] = {{"read_u8", HostReadFunc<u8>},
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
                                   {}};

void ExprDeleter::operator()(expr* expression) const
{
  expr_destroy(expression, nullptr);
}

void ExprVarListDeleter::operator()(expr_var_list* vars) const
{
  expr_destroy(nullptr, vars);  // free list elements
  delete vars;                  // free list object
}

Expression::Expression(std::string_view text, ExprPointer ex, ExprVarListPointer vars)
    : m_text(text), m_expr(std::move(ex)), m_vars(std::move(vars))
{
}

std::optional<Expression> Expression::TryParse(std::string_view text)
{
  ExprVarListPointer vars{new expr_var_list{}};
  ExprPointer ex{expr_create(text.data(), text.length(), vars.get(), g_expr_funcs)};
  if (!ex)
    return {};

  return Expression{text, std::move(ex), std::move(vars)};
}

double Expression::Evaluate() const
{
  for (auto* v = m_vars->head; v != nullptr; v = v->next)
  {
    auto index = ParseGPR(v->name);
    if (index)
      v->value = GPR(*index);
    else
      v->value = 0;
  }

  return expr_eval(m_expr.get());
}

std::string Expression::GetText() const
{
  return m_text;
}
