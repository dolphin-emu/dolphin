// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Convenience wrappers around python's c api for invoking stuff.

#pragma once

#include <Python.h>

#include "Scripting/Python/Utils/fmt.h"
#include "Scripting/Python/Utils/gil.h"
#include "Scripting/Python/Utils/object_wrapper.h"

namespace Py
{

template <typename... Ts>
inline PyObject* CallFunction(const Py::Object& callable_object, Ts... ts)
{
  Py::GIL lock;
  if constexpr (sizeof...(Ts) > 0)
    return PyObject_CallFunction(callable_object.Lend(), Py::fmts<Ts...>.c_str(), ts...);
  else
    return PyObject_CallFunction(callable_object.Lend(), nullptr);
}

template <typename... Ts>
inline PyObject* CallMethod(const Py::Object& callable_object, const char* name, Ts... ts)
{
  Py::GIL lock;
  if constexpr (sizeof...(Ts) > 0)
    return PyObject_CallMethod(callable_object.Lend(), name, Py::fmts<Ts...>.c_str(), ts...);
  else
    return PyObject_CallMethod(callable_object.Lend(), name, nullptr);
}

}  // namespace Py
