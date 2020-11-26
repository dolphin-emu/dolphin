// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This file implements `Py::as_py_func`, which lets you turn a regular function
// into a PyCFunction, which can then be registered and used from within python.
//
// Example:
//
//     static int FindIndex(PyObject* self, char* haystack, char needle)
//     {
//       for (int i = 0; haystack[i] != nullptr; i++)
//         if (haystack[i] == needle) return i;
//       return -1;
//     }
//     PyCFunction PyFindIndex = Py::as_py_func<FindIndex>;
//
// The resulting function is roughly equivalent to this:
//
//     static PyObject* PyFindIndex(PyObject* self, PyObject* args)
//     {
//       char* haystack;
//       char needle;
//       if (!PyArg_ParseTuple(args, "sb", &haystack, &needle))
//         return nullptr;
//       for (int i = 0; haystack[i] != nullptr; i++)
//         if (haystack[i] == needle) return Py_BuildValue("i", i);
//       return Py_BuildValue("i", -1);
//     }
//
// It does this by wrapping the original function with the required code to
// parse the arguments and convert the result, using compile-time metaprogramming.
// All types present in the original function's signature must be convertible using
// `Py::fmt`, otherwise the respective usage of `Py::as_py_func` won't compile.

#pragma once

#include <iostream>
#include <Python.h>
#include <tuple>

#include "Scripting/Python/Utils/convert.h"
#include "Scripting/Python/Utils/fmt.h"

namespace Py
{

// These template shenanigans are all required for as_py_func
// to be able to infer all of the c function signature's parts
// from the function only.
// Basically it takes an <auto T>,
// turns that into <decltype(T), T> captured as <InnerCFunc<TRet, TsArgs...>, TFunc>,
// which then turns that into <TRet, TsArgs..., InnerCFunc<TRet, TsArgs...> TFunc>.
// See https://stackoverflow.com/a/50775214/3688648 for a more elaborate explanation.
template <typename TRet, typename... TsArgs>
using InnerCFunc = TRet (*)(PyObject*, TsArgs...);

template <typename T, T>
struct PyCFunctionWrapperStruct;

template <typename TRet, typename... TsArgs, InnerCFunc<TRet, TsArgs...> TFunc>
struct PyCFunctionWrapperStruct<InnerCFunc<TRet, TsArgs...>, TFunc>
{
  static PyObject* PyCFunctionWrapper(PyObject* self, PyObject* args)
  {
    auto args_tpl_opt = Py::ParseTuple<TsArgs...>(args);
    if (!args_tpl_opt.has_value())
      return nullptr;
    std::tuple<TsArgs...> args_tpl = args_tpl_opt.value();

    auto self_then_args = std::tuple_cat(std::make_tuple(self), args_tpl);
    if constexpr (std::is_same_v<TRet, void>)
    {
      std::apply(TFunc, self_then_args);
      Py_RETURN_NONE;
    }
    else
    {
      TRet result = std::apply(TFunc, self_then_args);
      auto py_result = Py::ToPyCompatibleValue(result);
      return Py_BuildValue(Py::fmt<decltype(py_result)>, py_result);
    }
  }
};

template <auto T>
struct PyCFunctionWrapperWrappedStruct : PyCFunctionWrapperStruct<decltype(T), T>
{
};

template <auto T>
static PyCFunction as_py_func = PyCFunctionWrapperWrappedStruct<T>::PyCFunctionWrapper;

}  // namespace Py
