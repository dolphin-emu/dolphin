// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <coroutine>
#include <type_traits>
#include <utility>

#include "Common/ScopeGuard.h"

namespace Common
{
// A return type to tag functions as performing asynchronously and enable them to be coroutines.
// No information is carried in the return result. Such functions are "fire and forget".
struct Detached
{
  // Just about as simple as a coroutine promise type can be.
  // It just returns a default constructed object to the caller.
  struct promise_type
  {
    static constexpr auto initial_suspend() { return std::suspend_never{}; }
    static constexpr auto final_suspend() noexcept { return std::suspend_never{}; }
    static constexpr void unhandled_exception() {}
    static constexpr auto get_return_object() { return Detached{}; }
    static constexpr void return_void() {}
  };
};

// A coroutine awaiter that suspends then resumes execution via the provided function.
// Example:
//  co_await ResumeVia(function_that_pushes_a_job_to_another_thread);
// The current coroutine is suspended then the other thread will resume it.
// In other words, the current function will continue, now from that other thread.
template <std::invocable<void()> Func>
class ResumeVia
{
public:
  [[nodiscard]] constexpr explicit ResumeVia(Func&& function)
      : m_function(std::forward<Func>(function))
  {
  }

  // Returning false causes suspension then `await_suspend` to happen.
  static constexpr bool await_ready() { return false; }

  constexpr void await_suspend(std::coroutine_handle<> handle)
  {
    // Invoke with a lambda that resumes the coroutine or destroys it if it go out of scope.
    m_function([handle, destroy_coro = Common::ScopeGuard{[handle] { handle.destroy(); }}] mutable {
      destroy_coro.Dismiss();
      handle.resume();
    });
  }

  static constexpr void await_resume() {}

private:
  std::decay_t<Func> m_function;
};

// Deduction guide to make CTAD work properly with "perfect forwarding".
template <typename Func>
ResumeVia(Func&&) -> ResumeVia<Func>;

}  // namespace Common
