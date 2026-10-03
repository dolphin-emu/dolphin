// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Based on: https://github.com/encounter/cwdemangle
// Copyright 2024 Luke Street <luke@street.dev>
// SPDX-License-Identifier: CC0-1.0

#pragma once

#include <optional>
#include <string>

#include "Common/CommonTypes.h"

namespace CWDemangler
{

// Options for [demangle].
struct DemangleOptions
{
  // Replace `(void)` function parameters with `()`
  bool omit_empty_parameters = true;
  // Enable Metrowerks extension types (`__int128`, `__vec2x32float__`, etc.)
  //
  // Disabled by default since they conflict with template argument literals
  // and can't always be demangled correctly.
  bool mw_extensions = false;
};

struct DemangleTemplateArgsResult
{
  std::string_view name;
  std::string args;
};

struct DemangleNameResult
{
  std::string class_name;
  std::string full;
  std::string_view rest;
};

struct DemangleArgResult
{
  std::string arg_pre;
  std::string arg_post;
  std::string_view rest;
};

struct DemangleFunctionArgsResult
{
  std::string args;
  std::string_view rest;
};

std::optional<DemangleTemplateArgsResult> demangle_template_args(std::string_view str,
                                                                 DemangleOptions options);
std::optional<DemangleNameResult> demangle_name(std::string_view str, DemangleOptions options);
std::optional<DemangleNameResult> demangle_qualified_name(std::string_view str,
                                                          DemangleOptions options);
std::optional<DemangleArgResult> demangle_arg(std::string_view str, DemangleOptions options);
std::optional<DemangleFunctionArgsResult> demangle_function_args(std::string_view str,
                                                                 DemangleOptions options);
std::optional<std::string> demangle_special_function(std::string_view str,
                                                     std::string_view class_name,
                                                     DemangleOptions options);
std::optional<std::string> demangle(std::string_view str, DemangleOptions options);
}  // namespace CWDemangler
