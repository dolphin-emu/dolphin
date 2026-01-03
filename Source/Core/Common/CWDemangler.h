// Based on https://github.com/encounter/cwdemangle originally
// created by encounter
// SPDX-License-Identifier: Apache-2.0 OR MIT

#pragma once

#include <optional>
#include <string>
#include <tuple>

#include "Common/CommonTypes.h"

/// Options for [demangle].
struct DemangleOptions
{
  /// Replace `(void)` function parameters with `()`
  bool omit_empty_parameters = true;
  /// Enable Metrowerks extension types (`__int128`, `__vec2x32float__`, etc.)
  ///
  /// Disabled by default since they conflict with template argument literals
  /// and can't always be demangled correctly.
  bool mw_extensions = false;

  /*
  DemangleOptions() {
  }

  DemangleOptions(bool omit_empty_params, bool mw_exts)
  {
    this->omit_empty_parameters = omit_empty_params;
    this->mw_extensions = mw_exts;
  }*/
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

namespace CWDemangler
{
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
