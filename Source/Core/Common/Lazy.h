// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <utility>
#include <variant>

namespace Common
{
// A Lazy object holds a value. If a Lazy object is constructed using
// a function as an argument, that function will be called to compute
// the value the first time any code tries to access the value.

template <typename T>
class Lazy
{
public:
  Lazy() : m_value(T()) {}
  Lazy(const std::variant<T, std::function<T()>>& value) : m_value(value) {}
  Lazy(std::variant<T, std::function<T()>>&& value) : m_value(std::move(value)) {}
  const T& operator*() const { return *ComputeValue(); }
  const T* operator->() const { return ComputeValue(); }
  T& operator*() { return *ComputeValue(); }
  T* operator->() { return ComputeValue(); }
private:
  T* ComputeValue() const
  {
    if (!std::holds_alternative<T>(m_value))
      m_value = std::get<std::function<T()>>(m_value)();
    return &std::get<T>(m_value);
  }

  mutable std::variant<T, std::function<T()>> m_value;
};
}
