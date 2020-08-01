// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Utilities for automatically converting some types
// that cannot normally be turned into a Python object,
// but _can_ be converted by turning it into some other
// representation first.
// This process is asymmetric, meaning these functions only
// assist in _building_ such values, not parsing them (yet).

#pragma once

#include <Python.h>

#include "Scripting/Python/Utils/fmt.h"
#include "Scripting/Python/Utils/object_wrapper.h"

namespace Py
{

template <typename T>
inline auto ToPyCompatibleValue(T t)
{
  static_assert(!std::is_same_v<T, bool>);
  return t;
}

inline PyObject* ToPyCompatibleValue(bool t)
{
  return Py::Take(t ? Py_True : Py_False).Leak();
}

template <typename... Ts>
constexpr auto ToPyCompatibleValues(const std::tuple<Ts...> tuple)
{
  return std::apply([](auto&&... arg) { return std::make_tuple(ToPyCompatibleValue(arg)...); },
                    tuple);
}

template <typename... TsArgs>
PyObject* BuildValueTuple(const std::tuple<TsArgs...> args)
{
  Py::GIL lock;
  if constexpr (sizeof...(TsArgs) == 0)
  {
    return Py::Take(Py_None).Leak();
  }
  else if constexpr(sizeof...(TsArgs) == 1)
  {
    // Avoid the special behaviour for singular elements,
    // see Py_BuildValue's documentation for details.
    auto arg = Py::ToPyCompatibleValue(std::get<0>(std::make_tuple(args)));
    return Py_BuildValue(("(" + Py::fmts<decltype(arg)> + ")").c_str(), arg);
  }
  else
  {
    return std::apply(
        [&](auto&&... arg) {
          return Py_BuildValue(Py::fmts<std::remove_reference_t<decltype(arg)>...>.c_str(), arg...);
        },
        Py::ToPyCompatibleValues(args));
  }
}

}  // namespace Py
