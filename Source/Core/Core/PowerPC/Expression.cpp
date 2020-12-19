// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/Expression.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <expr.h>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
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
  u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 0)));
  return Common::BitCast<T>(HostRead<U>(address));
}

template <typename T, typename U = T>
static double HostWriteFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 2)
    return 0;
  T var = static_cast<T>(expr_eval(&vec_nth(args, 0)));
  u32 address = static_cast<u32>(expr_eval(&vec_nth(args, 1)));
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
                                   {"u8", CastFunc<u8>},
                                   {"s8", CastFunc<s8, u8>},
                                   {"u16", CastFunc<u16>},
                                   {"s16", CastFunc<s16, u16>},
                                   {"u32", CastFunc<u32>},
                                   {"s32", CastFunc<s32, u32>},
                                   {}};

const std::unordered_map<std::string_view, u32&> g_register_vars = {
    {"r0", GPR(0)},   {"r1", GPR(1)},   {"r2", GPR(2)},   {"r3", GPR(3)},   {"r4", GPR(4)},
    {"r5", GPR(5)},   {"r6", GPR(6)},   {"r7", GPR(7)},   {"r8", GPR(8)},   {"r9", GPR(9)},
    {"r10", GPR(10)}, {"r11", GPR(11)}, {"r12", GPR(12)}, {"r13", GPR(13)}, {"r14", GPR(14)},
    {"r15", GPR(15)}, {"r16", GPR(16)}, {"r17", GPR(17)}, {"r18", GPR(18)}, {"r19", GPR(19)},
    {"r20", GPR(20)}, {"r21", GPR(21)}, {"r22", GPR(22)}, {"r23", GPR(23)}, {"r24", GPR(24)},
    {"r25", GPR(25)}, {"r26", GPR(26)}, {"r27", GPR(27)}, {"r28", GPR(28)}, {"r29", GPR(29)},
    {"r30", GPR(30)}, {"r31", GPR(31)}, {"pc", PC},       {"lr", LR},       {"ctr", CTR},
};

const std::unordered_map<std::string_view, u64&> g_float_register_vars = {
    {"f0", rPS(0).ps0},   {"f1", rPS(1).ps0},   {"f2", rPS(2).ps0},   {"f3", rPS(3).ps0},
    {"f4", rPS(4).ps0},   {"f5", rPS(5).ps0},   {"f6", rPS(6).ps0},   {"f7", rPS(7).ps0},
    {"f8", rPS(8).ps0},   {"f9", rPS(9).ps0},   {"f10", rPS(10).ps0}, {"f11", rPS(11).ps0},
    {"f12", rPS(12).ps0}, {"f13", rPS(13).ps0}, {"f14", rPS(14).ps0}, {"f15", rPS(15).ps0},
    {"f16", rPS(16).ps0}, {"f17", rPS(17).ps0}, {"f18", rPS(18).ps0}, {"f19", rPS(19).ps0},
    {"f20", rPS(20).ps0}, {"f21", rPS(21).ps0}, {"f22", rPS(22).ps0}, {"f23", rPS(23).ps0},
    {"f24", rPS(24).ps0}, {"f25", rPS(25).ps0}, {"f26", rPS(26).ps0}, {"f27", rPS(27).ps0},
    {"f28", rPS(28).ps0}, {"f29", rPS(29).ps0}, {"f30", rPS(30).ps0}, {"f31", rPS(31).ps0},
};

enum class SynchronizeDirection
{
  FromRegister,
  ToRegister,
};

static void SynchronizeRegisters(const expr_var_list& vars, SynchronizeDirection dir)
{
  for (auto* v = vars.head; v != nullptr; v = v->next)
  {
    auto r = g_register_vars.find(v->name);
    if (r != g_register_vars.end())
    {
      if (dir == SynchronizeDirection::FromRegister)
      {
        v->value = static_cast<double>(r->second);
      }
      else
      {
        r->second = static_cast<u32>(static_cast<s64>(v->value));
      }
      continue;
    }

    auto fr = g_float_register_vars.find(v->name);
    if (fr != g_float_register_vars.end())
    {
      if (dir == SynchronizeDirection::FromRegister)
      {
        v->value = Common::BitCast<double>(fr->second);
      }
      else
      {
        fr->second = Common::BitCast<u64>(v->value);
      }
      continue;
    }

    if (dir == SynchronizeDirection::FromRegister)
    {
      v->value = 0;  // init expression local variables to zero
    }
  }
}

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
  SynchronizeRegisters(*m_vars, SynchronizeDirection::FromRegister);

  double result = expr_eval(m_expr.get());

  SynchronizeRegisters(*m_vars, SynchronizeDirection::ToRegister);

  return result;
}

std::string Expression::GetText() const
{
  return m_text;
}
