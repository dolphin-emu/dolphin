// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <string_view>

namespace Common
{
constexpr std::size_t CountFmtReplacementFields(std::string_view s)
{
  std::size_t count = 0;
  for (std::size_t i = 0; i < s.size(); ++i)
  {
    if (s[i] != '{')
      continue;

    // If the opening brace is followed by another brace, what we have is
    // an escaped brace, not a replacement field.
    if (i + 1 < s.size() && s[i + 1] == '{')
    {
      // Skip the second brace.
      // This ensures that e.g. {{{}}} is counted correctly: when the first brace character
      // is read and detected as being part of an '{{' escape sequence, the second character
      // is skipped so the most inner brace (the third character) is not detected
      // as the end of an '{{' pair.
      ++i;
      continue;
    }

    ++count;
  }
  return count;
}

static_assert(CountFmtReplacementFields("") == 0);
static_assert(CountFmtReplacementFields("{} test {:x}") == 2);
static_assert(CountFmtReplacementFields("{} {{}} test {{{}}}") == 2);

constexpr bool ContainsNonPositionalArguments(std::string_view s)
{
  for (std::size_t i = 0; i < s.size(); ++i)
  {
    if (s[i] != '{' || i + 1 == s.size())
      continue;

    const char next = s[i + 1];

    // If the opening brace is followed by another brace, what we have is
    // an escaped brace, not a replacement field.
    if (next == '{')
    {
      // Skip the second brace.
      // This ensures that e.g. {{{}}} is counted correctly: when the first brace character
      // is read and detected as being part of an '{{' escape sequence, the second character
      // is skipped so the most inner brace (the third character) is not detected
      // as the end of an '{{' pair.
      ++i;
    }
    else if (next == '}' || next == ':')
    {
      return true;
    }
  }
  return false;
}

static_assert(!ContainsNonPositionalArguments(""));
static_assert(ContainsNonPositionalArguments("{}"));
static_assert(!ContainsNonPositionalArguments("{0}"));
static_assert(ContainsNonPositionalArguments("{:x}"));
static_assert(!ContainsNonPositionalArguments("{0:x}"));
static_assert(!ContainsNonPositionalArguments("{0} {{}} test {{{1}}}"));
}  // namespace Common
