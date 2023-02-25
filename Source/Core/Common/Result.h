// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <variant>

namespace Common
{
template <typename ResultCode, typename T>
class Result final
{
public:
  Result(ResultCode code) : m_variant{code} {}
  Result(const T& t) : m_variant{t} {}
  Result(T&& t) : m_variant{std::move(t)} {}
  explicit operator bool() const { return Succeeded(); }
  bool Succeeded() const { return std::holds_alternative<T>(m_variant); }
  // Must only be called when Succeeded() returns false.
  ResultCode Error() const { return std::get<ResultCode>(m_variant); }
  // Must only be called when Succeeded() returns true.
  const T& operator*() const { return std::get<T>(m_variant); }
  const T* operator->() const { return &std::get<T>(m_variant); }
  T& operator*() { return std::get<T>(m_variant); }
  T* operator->() { return &std::get<T>(m_variant); }

private:
  std::variant<ResultCode, T> m_variant;
};
}  // namespace Common
