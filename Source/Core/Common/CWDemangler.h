#pragma once

#include <optional>
#include <string>
#include <tuple>

#include "Common/CommonTypes.h"

/// Options for [demangle].
struct DemangleOptions
{
  /// Replace `(void)` function parameters with `()`
  bool omit_empty_parameters;
  /// Enable Metrowerks extension types (`__int128`, `__vec2x32float__`, etc.)
  ///
  /// Disabled by default since they conflict with template argument literals
  /// and can't always be demangled correctly.
  bool mw_extensions;

  DemangleOptions()
  {
    omit_empty_parameters = true;
    mw_extensions = false;
  }

  DemangleOptions(bool omit_empty_params, bool mw_exts)
  {
    this->omit_empty_parameters = omit_empty_params;
    this->mw_extensions = mw_exts;
  }
};

class CWDemangler
{
public:
  static std::optional<std::tuple<std::string, std::string>>
  demangle_template_args(std::string str, DemangleOptions options);
  static std::optional<std::tuple<std::string, std::string, std::string>>
  demangle_name(std::string str, DemangleOptions options);
  static std::optional<std::tuple<std::string, std::string, std::string>>
  demangle_qualified_name(std::string str, DemangleOptions options);
  static std::optional<std::tuple<std::string, std::string, std::string>>
  demangle_arg(std::string str, DemangleOptions options);
  static std::optional<std::tuple<std::string, std::string>>
  demangle_function_args(std::string str, DemangleOptions options);
  static std::optional<std::string>
  demangle_special_function(std::string str, std::string class_name, DemangleOptions options);
  static std::optional<std::string> demangle(std::string str, DemangleOptions options);

private:
  inline static bool IsAscii(std::string s)
  {
    for (char c : s)
    {
      if (static_cast<u8>(c) > 127)
        return false;
    }

    return true;
  }

  static std::tuple<std::string, std::string, std::string> parse_qualifiers(std::string str);
  static std::optional<std::tuple<size_t, std::string>> parse_digits(std::string str);
  static std::optional<size_t> find_split(std::string s, bool special, DemangleOptions options);
};
