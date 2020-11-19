// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This file lets you translate types to their corresponding python format strings.
// See the python documentation for more information on those:
// https://docs.python.org/3/c-api/arg.html
//
// Examples:
//
// Get the format string of a single type:
//     const char* fmt_string = Py::fmt<float>;
//     // returns "f"
//
// Get the format string of multiple types:
//     std::string fmt_string = Py::fmts<float, PyObject*, int>;
//     // returns "fOi"

#pragma once

#include <Python.h>
#include <string>

namespace Py
{

template <typename T>
constexpr const char* GetPyFmt()
{
  // I guess automatic boolean parsing could be done with enough metaprogramming,
  // but it seems to be very complicated.
  static_assert((!std::is_same_v<T, bool>),
                "The Python C API does not have a boolean type. "
                "Consider using PyObject* and checking with PyObject_IsTrue(PyObject*)");
  static_assert(sizeof(T) != sizeof(T), R"(
  no python format string known for type.

  If you get a compile error that ends in here, typically one of these things happened:

  - You tried to use a variable amount of template parameters, and the amount was 0.
    For cases where it is possible that you need to represent "zero arguments",
    You must constexpr guard against this special case, e.g. with code like this:

        template <typename... Ts>
        void InvokeSomePyCallable(Ts... args)
        {
          if constexpr (sizeof...(Ts) == 0)
            PyObject_CallFunction(somePyCallable, nullptr);
          else
          {
            auto fmt_string = Py::fmts<Ts...>;
            PyObject_CallFunction(somePyCallable, fmt_string.c_str(), args...);
          }
        }

  - You tried to use a type for which a fitting format character exists,
    but is not implemented yet. It is possible that you could add it.

  - You tried to use a type that cannot be directly converted to a python type.
    Check that the type that failed to be converted is what you indended.
    If it is, try to break your type down to some primitives that are translatable,
    or don't use Py::fmt and instead perform the necessary conversion by hand.
  )");
  return nullptr;
}
// template specializations for each type
template <>
constexpr const char* GetPyFmt<signed char>()
{
  return "b";
}
template <>
constexpr const char* GetPyFmt<signed short>()
{
  return "h";
}
template <>
constexpr const char* GetPyFmt<signed int>()
{
  return "i";
}
template <>
constexpr const char* GetPyFmt<signed long>()
{
  return "l";
}
template <>
constexpr const char* GetPyFmt<signed long long>()
{
  return "L";
}
template <>
constexpr const char* GetPyFmt<unsigned char>()
{
  return "B";
}
template <>
constexpr const char* GetPyFmt<unsigned short>()
{
  return "H";
}
template <>
constexpr const char* GetPyFmt<unsigned int>()
{
  return "I";
}
template <>
constexpr const char* GetPyFmt<unsigned long>()
{
  return "k";
}
template <>
constexpr const char* GetPyFmt<unsigned long long>()
{
  return "K";
}
template <>
constexpr const char* GetPyFmt<float>()
{
  return "f";
}
template <>
constexpr const char* GetPyFmt<double>()
{
  return "d";
}
template <>
constexpr const char* GetPyFmt<PyObject*>()
{
  return "O";
}
template <>
constexpr const char* GetPyFmt<const char*>()
{
  return "s";
}
template <>
constexpr const char* GetPyFmt<PyBytesObject*>()
{
  return "S";
}
// end of template specializations for each type

template <typename T>
constexpr const char* fmt = GetPyFmt<T>();

template <typename TLast>
std::string GetPyFmts()
{
  return std::string{fmt<TLast>};
}

template <typename TFirst, typename TSecond, typename... TRest>
std::string GetPyFmts()
{
  return fmt<TFirst> + GetPyFmts<TSecond, TRest...>();
}

template <typename... Ts>
const std::string fmts = GetPyFmts<Ts...>();

}  // namespace Py
