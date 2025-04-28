// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

// TODO C++23: Replace with std::move_only_function.

namespace Common
{

template <typename T>
class MoveOnlyFunction;

template <typename R, typename... Args>
class MoveOnlyFunction<R(Args...)>
{
public:
  using result_type = R;

  MoveOnlyFunction() = default;

  template <typename F>
  MoveOnlyFunction(F&& f) : m_ptr{std::make_unique<Func<F>>(std::forward<F>(f))}
  {
  }

  result_type operator()(Args... args) const { return m_ptr->Invoke(std::forward<Args>(args)...); }
  explicit operator bool() const { return m_ptr != nullptr; };
  void swap(MoveOnlyFunction& other) { m_ptr.swap(other.m_ptr); }

private:
  struct FuncBase
  {
    virtual ~FuncBase() = default;
    virtual result_type Invoke(Args...) = 0;
  };

  template <typename F>
  struct Func : FuncBase
  {
    Func(F&& f) : func{std::forward<F>(f)} {}
    result_type Invoke(Args... args) override { return func(std::forward<Args>(args)...); }
    F func;
  };

  std::unique_ptr<FuncBase> m_ptr;
};

}  // namespace Common
