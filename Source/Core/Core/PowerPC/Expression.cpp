// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Expression.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <expr.h>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Core/PowerPC/PowerPC.h"

template <typename T, typename U = T>
static double CastFunc(expr_func* f, vec_expr_t* args, void* c)
{
  if (vec_len(args) != 1)
    return 0;
  return Common::BitCast<T>(static_cast<U>(expr_eval(&vec_nth(args, 0))));
}

static std::array<expr_func, 8> g_expr_funcs{{
    {"u8", CastFunc<u8>},
    {"s8", CastFunc<s8, u8>},
    {"u16", CastFunc<u16>},
    {"s16", CastFunc<s16, u16>},
    {"u32", CastFunc<u32>},
    {"s32", CastFunc<s32, u32>},
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

std::string Expression::GetText() const
{
  return m_text;
}
