// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Convenience wrappers around python's c api for invoking stuff.

#pragma once

#include <Python.h>

#include "Scripting/Python/Utils/convert.h"
#include "Scripting/Python/Utils/fmt.h"
#include "Scripting/Python/Utils/object_wrapper.h"

namespace Py
{

template <typename... Ts>
inline PyObject* CallFunction(const Py::Object& callable_object, Ts... ts)
{
  if constexpr (sizeof...(Ts) == 0)
  {
    return PyObject_CallFunction(callable_object.Lend(), nullptr);
  }
  else if constexpr (sizeof...(Ts) == 1)
  {
    // Avoid the special behaviour for singular elements,
    // see Py_BuildValue's documentation for details.
    auto arg = Py::ToPyCompatibleValue(std::get<0>(std::make_tuple(ts...)));
    return PyObject_CallFunction(callable_object.Lend(),
                                 ("(" + Py::fmts<decltype(arg)> + ")").c_str(), arg);
  }
  else
  {
    return std::apply(
        [&](auto&&... arg) {
          return PyObject_CallFunction(callable_object.Lend(),
                                       Py::fmts<std::remove_reference_t<decltype(arg)>...>.c_str(),
                                       arg...);
        },
        Py::ToPyCompatibleValues(std::make_tuple(ts...)));
  }
}

template <typename... Ts>
inline PyObject* CallMethod(const Py::Object& callable_object, const char* name, Ts... ts)
{
  if constexpr (sizeof...(Ts) == 0)
  {
    return PyObject_CallMethod(callable_object.Lend(), name, nullptr);
  }
  else if constexpr (sizeof...(Ts) == 1)
  {
    // Avoid the special behaviour for singular elements,
    // see Py_BuildValue's documentation for details.
    auto arg = Py::ToPyCompatibleValue(std::get<0>(std::make_tuple(ts...)));
    return PyObject_CallMethod(callable_object.Lend(), name,
                               ("(" + Py::fmts<decltype(arg)> + ")").c_str(), arg);
  }
  else
  {
    return std::apply(
        [&](auto&&... arg) {
          return PyObject_CallMethod(callable_object.Lend(), name,
                                     Py::fmts<std::remove_reference_t<decltype(arg)>...>.c_str(),
                                     arg...);
        },
        Py::ToPyCompatibleValues(std::make_tuple(ts...)));
  }
}

}  // namespace Py
