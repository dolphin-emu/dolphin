// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Abstraction for a simple flag that can be toggled in a multithreaded way.
//
// Simple API:
// * Set(bool = true): sets the Flag
// * IsSet(): tests if the flag is set
// * Clear(): clears the flag (equivalent to Set(false)).
//
// More advanced features:
// * TestAndSet(bool = true): sets the flag to the given value. If a change was
//                            needed (the flag did not already have this value)
//                            the function returns true. Else, false.
// * TestAndClear(): alias for TestAndSet(false).

#pragma once

#include <atomic>

namespace Common
{
class Flag final
{
public:
  // Declared as explicit since we do not want "= true" to work on a flag
  // object - it should be made explicit that a flag is *not* a normal
  // variable.
  explicit Flag(bool initial_value = false) : m_val(initial_value) {}
  void Set(bool val = true) { m_val.store(val); }
  void Clear() { Set(false); }
  bool IsSet() const { return m_val.load(); }
  bool TestAndSet(bool val = true)
  {
    bool expected = !val;
    return m_val.compare_exchange_strong(expected, val);
  }

  bool TestAndClear() { return TestAndSet(false); }

private:
  std::atomic_bool m_val;
};

}  // namespace Common
