// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Wrapper around PyObject* to replace manual reference counting
// with RAII. Basically a smart pointer.
//
// - Use `Wrap(PyObject*)` to wrap a PyObject* you already own the reference to.
// - Use `Take(PyObject*)` to wrap a PyObject* you do not own the reference to yet.
// - Use `PyObject* Leak()` for extracting the underlying PyObject* from the wrapper.
//   You are responsible for keeping track of the reference count then. This is usually
//   used for passing a PyObject* to something that takes ownership of a reference.
// - Use `PyObject* Lend()` for retrieving the underlying PyObject* while
//   keeping the wrapper object intact. It will stay the owner of that reference.

#pragma once

#include <Python.h>

namespace Py
{

class Object
{
public:
  Object& operator=(Object&& other) noexcept;
  Object& operator=(const Object& rhs);
  Object();
  Object(Object&& other) noexcept;
  Object(const Object& other);
  ~Object();
  PyObject* Lend() const;
  PyObject* Leak();
  bool IsNull() const;
  static Object WrapExisting(PyObject* py_object);

private:
  PyObject* m_py_object_;
  Object(PyObject* py_object);
};

Object Wrap(PyObject* py_object);
Object Take(PyObject* py_object);
const Object Null();

}  // namespace Py
