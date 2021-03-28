// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace Core
{
// Central class that encapsulates the running system.
class System
{
public:
  ~System();

  System(const System&) = delete;
  System& operator=(const System&) = delete;

  System(System&&) = delete;
  System& operator=(System&&) = delete;

  // Intermediate instance accessor until global state is eliminated.
  static System& GetInstance()
  {
    static System instance;
    return instance;
  }

private:
  System();

  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
}  // namespace Core
