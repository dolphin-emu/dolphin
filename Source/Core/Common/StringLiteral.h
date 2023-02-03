// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>

namespace Common
{
// A useful template for passing string literals as arguments to templates
// from: https://ctrpeach.io/posts/cpp20-string-literal-template-parameters/
template <size_t N>
struct StringLiteral
{
  consteval StringLiteral(const char (&str)[N]) { std::copy_n(str, N, value); }

  char value[N];
};

}  // namespace Common
