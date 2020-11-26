// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Utilities for automatically converting between C++ and python types.
// For the conversion _to_ python, there exist a number of types
// that cannot normally be turned into a Python object,
// but _can_ be converted by turning it into some other
// representation first.
// Such conversions are also automatically performed here.

#pragma once

#include <optional>
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

template <typename TArg>
PyObject* BuildValue(const TArg arg)
{
  return Py_BuildValue(Py::fmt<TArg>, Py::ToPyCompatibleValue(arg));
}

template <typename... TsArgs>
PyObject* BuildValueTuple(const std::tuple<TsArgs...> args)
{
  if constexpr (sizeof...(TsArgs) == 0)
  {
    return Py::Take(Py_None).Leak();
  }
  else if constexpr(sizeof...(TsArgs) == 1)
  {
    // Avoid the special behaviour for singular elements,
    // see Py_BuildValue's documentation for details.
    auto arg = Py::ToPyCompatibleValue(std::get<0>(args));
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

template<typename... TsArgs>
static std::optional<std::tuple<TsArgs...>> ParseTuple(PyObject* args)
{
  std::tuple<TsArgs...> args_tpl;
  if constexpr (sizeof...(TsArgs) > 0)
  {
    // PyArg_ParseTuple writes the parse results to passed-in pointers.
    // Turn all our T into T* for that.
    std::tuple<const TsArgs*...> args_pointers =
        std::apply([&](const auto&... obj) { return std::make_tuple(&obj...); }, args_tpl);
    const std::tuple<PyObject*, const char*> py_args_and_fmt =
        std::make_tuple(args, Py::fmts<TsArgs...>.c_str());
    auto invoke_args = std::tuple_cat(py_args_and_fmt, args_pointers);
    if (!std::apply(PyArg_ParseTuple, invoke_args))
    {
      // argument parsing failed, exception has been raised
      std::optional<std::tuple<TsArgs...>> opt_empty;
      return opt_empty;
    }
  }
  return std::make_optional(args_tpl);
}

}  // namespace Py
