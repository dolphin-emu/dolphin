// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonFuncs.h"

namespace Common
{
// TODO C++23: Replace with std::unreachable.
[[noreturn]] inline void Unreachable()
{
#ifdef _DEBUG
  Crash();
#elif defined(_MSC_VER) && !defined(__clang__)
  __assume(false);
#else
  __builtin_unreachable();
#endif
}
}  // namespace Common
