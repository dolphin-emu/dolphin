// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "object_wrapper.h"

namespace Py
{

Object& Object::operator=(Object&& other) noexcept
{
  if (m_py_object_ != nullptr)
  {
    Py_DECREF(m_py_object_);
  }
  m_py_object_ = other.m_py_object_;
  other.m_py_object_ = nullptr;
  return *this;
}

Object& Object::operator=(const Object& rhs)
{
  if (m_py_object_ != nullptr || rhs.m_py_object_ != nullptr)
  {
    Py_XINCREF(rhs.m_py_object_);
    Py_XDECREF(m_py_object_);
  }
  m_py_object_ = rhs.m_py_object_;
  return *this;
}

Object::Object()
{
  m_py_object_ = nullptr;
};

Object::Object(Object&& other) noexcept
{
  m_py_object_ = other.m_py_object_;
  other.m_py_object_ = nullptr;
};

Object::Object(const Object& other)
{
  m_py_object_ = other.m_py_object_;
  if (m_py_object_ != nullptr)
  {
    Py_INCREF(m_py_object_);
  }
}

Object::~Object()
{
  if (m_py_object_ != nullptr)
  {
    Py_DECREF(m_py_object_);
  }
  m_py_object_ = nullptr;
}

PyObject* Object::Lend() const
{
  return m_py_object_;
}

PyObject* Object::Leak()
{
  PyObject* obj = m_py_object_;
  m_py_object_ = nullptr;
  return obj;
}

bool Object::IsNull() const
{
  return m_py_object_ == nullptr;
}

Object Object::WrapExisting(PyObject* py_object)
{
  return Object(py_object);
}

Object::Object(PyObject* py_object) : m_py_object_(py_object)
{
}

Object Wrap(PyObject* py_object)
{
  return Object::WrapExisting(py_object);
}

Object Take(PyObject* py_object)
{
  if (py_object != nullptr)
  {
    Py_INCREF(py_object);
  }
  return Object::WrapExisting(py_object);
}

const Object Null()
{
  static const Object null_obj = Wrap(nullptr);
  return null_obj;
}

}  // namespace Py
