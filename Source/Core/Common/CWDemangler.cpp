// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Based on: https://github.com/encounter/cwdemangle
// Copyright 2024 Luke Street <luke@street.dev>
// SPDX-License-Identifier: CC0-1.0

#include "Common/CWDemangler.h"

#include <algorithm>
#include <cctype>
#include <map>

#include <fmt/format.h>

#include "Common/StringUtil.h"

namespace CWDemangler
{

struct ParseQualifiersResult
{
  std::string pre;
  std::string post;
  std::string_view rest;
};

struct ParseDigitsResult
{
  std::size_t digits;
  std::string_view rest;
};

// Forward declarations
std::optional<std::size_t> find_split(std::string_view s, bool special, DemangleOptions options);

inline static bool IsAscii(std::string_view s)
{
  return std::ranges::none_of(s, [](u8 c) { return c > 127; });
}

static const std::map<std::string, std::string> operators = {
    {"nw", "operator new"},    {"nwa", "operator new[]"},
    {"dl", "operator delete"}, {"dla", "operator delete[]"},
    {"pl", "operator+"},       {"mi", "operator-"},
    {"ml", "operator*"},       {"dv", "operator/"},
    {"md", "operator%"},       {"er", "operator^"},
    {"ad", "operator&"},       {"or", "operator|"},
    {"co", "operator~"},       {"nt", "operator!"},
    {"as", "operator="},       {"lt", "operator<"},
    {"gt", "operator>"},       {"apl", "operator+="},
    {"ami", "operator-="},     {"amu", "operator*="},
    {"adv", "operator/="},     {"amd", "operator%="},
    {"aer", "operator^="},     {"aad", "operator&="},
    {"aor", "operator|="},     {"ls", "operator<<"},
    {"rs", "operator>>"},      {"ars", "operator>>="},
    {"als", "operator<<="},    {"eq", "operator=="},
    {"ne", "operator!="},      {"le", "operator<="},
    {"ge", "operator>="},      {"aa", "operator&&"},
    {"oo", "operator||"},      {"pp", "operator++"},
    {"mm", "operator--"},      {"cm", "operator,"},
    {"rm", "operator->*"},     {"rf", "operator->"},
    {"cl", "operator()"},      {"vc", "operator[]"},
    {"vt", "__vtable"}};

static const std::map<char, std::string> types = {
    {'i', "int"},     {'b', "bool"},      {'c', "char"},  {'s', "short"},
    {'l', "long"},    {'x', "long long"}, {'f', "float"}, {'d', "double"},
    {'w', "wchar_t"}, {'v', "void"},      {'e', "..."},
};

ParseQualifiersResult parse_qualifiers(std::string_view str)
{
  std::string pre;
  std::string post;
  std::size_t index = 0;

  for (char c : str)
  {
    bool found_non_qualifier = false;

    switch (c)
    {
    case 'P':
      if (pre.empty())
      {
        post.insert(0, "*");
      }
      else
      {
        post.insert(0, fmt::format("* {0}", StripTrailingWhitespace(pre)));
        pre.clear();
      }
      break;
    case 'R':
      if (pre.empty())
      {
        post.insert(0, "&");
      }
      else
      {
        post.insert(0, fmt::format("& {0}", StripTrailingWhitespace(pre)));
        pre.clear();
      }
      break;
    case 'C':
      pre += "const ";
      break;
    case 'V':
      pre += "volatile ";
      break;
    case 'U':
      pre += "unsigned ";
      break;
    case 'S':
      pre += "signed ";
      break;
    default:
      found_non_qualifier = true;
      break;
    }

    if (found_non_qualifier)
      break;
    index++;
  }
  str.remove_prefix(index);
  post = StripTrailingWhitespace(post);
  return {pre, post, str};
}

std::optional<ParseDigitsResult> parse_digits(std::string_view str)
{
  if (str.empty())
    return std::nullopt;

  const char* str_start = str.data();
  const char* str_end = str_start + str.size();
  std::size_t val = 0;
  std::string_view remainder;

  const auto result = std::from_chars(str_start, str_end, val);

  if (result.ec != std::errc{})
    return std::nullopt;

  const char* rest_start = result.ptr;
  if (rest_start != str_end)
    remainder = std::string_view(rest_start, str_end - rest_start);

  return {{val, remainder}};
}

std::optional<DemangleTemplateArgsResult> demangle_template_args(std::string_view str,
                                                                 DemangleOptions options)
{
  const std::size_t start_idx = str.find('<');

  if (start_idx == std::string::npos)
    return {{str, ""}};

  const std::size_t end_idx = str.rfind('>');
  if (end_idx == std::string::npos || end_idx < start_idx)
  {
    return std::nullopt;
  }

  std::string_view args(&str[start_idx + 1], &str[end_idx]);
  const std::string_view template_name = str.substr(0, start_idx);
  std::string tmpl_args = "<";

  while (!args.empty())
  {
    const auto result = demangle_arg(args, options);
    if (!result)
      return std::nullopt;

    const auto demangled_arg = result.value();
    const std::string_view rest = demangled_arg.rest;
    tmpl_args += demangled_arg.arg_pre;
    tmpl_args += demangled_arg.arg_post;

    if (rest.empty())
      break;

    tmpl_args += ", ";

    args = rest.substr(1);
  }

  tmpl_args += ">";

  return {{template_name, tmpl_args}};
}

std::optional<DemangleNameResult> demangle_name(std::string_view str, DemangleOptions options)
{
  const auto result = parse_digits(str);
  if (!result)
    return std::nullopt;

  auto [size, rest] = result.value();

  if (rest.length() < size)
  {
    return std::nullopt;
  }

  auto result1 = demangle_template_args(rest.substr(0, size), options);
  if (!result1)
    return std::nullopt;

  auto [name, args] = result1.value();
  const std::string result_name{name};
  return {{result_name, fmt::format("{0}{1}", name, args), rest.substr(size)}};
}

std::optional<DemangleNameResult> demangle_qualified_name(std::string_view str,
                                                          DemangleOptions options)
{
  if (!str.starts_with('Q'))
    return demangle_name(str, options);

  if (str.length() < 3)
  {
    return std::nullopt;
  }

  // MWCC only preserves up to 9 layers of namespace/class depth in symbol names, so we
  // can just check one digit
  const int digit = (int)(str[1] - '0');
  if (digit < 0 || digit > 9)
    return std::nullopt;

  const int count = digit;

  str = str.substr(2);

  std::string last_class;
  std::string qualified;

  for (int i = 0; i < count; i++)
  {
    const auto result = demangle_name(str, options);
    if (!result)
      return std::nullopt;

    auto [class_name, full, rest] = result.value();
    qualified += full;
    last_class = class_name;
    str = rest;
    if (i < count - 1)
    {
      qualified += "::";
    }
  }
  return {{last_class, qualified, str}};
}

std::optional<DemangleArgResult> demangle_arg(std::string_view str, DemangleOptions options)
{
  // Negative constant
  if (str.starts_with('-'))
  {
    const auto parse_result = parse_digits(str.substr(1));
    if (!parse_result)
      return std::nullopt;

    std::size_t size = parse_result->digits;
    const std::string out_val = fmt::format("-{}", size);
    return {{out_val, "", parse_result->rest}};
  }

  std::string result;

  const auto parse_qual_result = parse_qualifiers(str);
  std::string pre = parse_qual_result.pre;
  std::string post = parse_qual_result.post;
  std::string_view rest = parse_qual_result.rest;
  result += pre;
  str = rest;

  // Disambiguate arguments starting with a number
  if (str.length() > 0 && std::isdigit((u8)str[0]))
  {
    const auto parse_result = parse_digits(str);
    if (!parse_result)
      return std::nullopt;

    auto& [num, rest_value] = parse_result.value();
    rest = rest_value;

    // If the number is followed by a comma or the end of the string, it's a template argument
    if (rest.empty() || rest.starts_with(','))
    {
      // ...or a Metrowerks extension type
      if (options.mw_extensions)
      {
        const std::string t = num == 1 ? "__int128" : num == 2 ? "__vec2x32float__" : "";

        if (!t.empty())
        {
          result += t;
          return {{result, post, rest}};
        }
      }
      result += fmt::format("{}{}", num, post);
      return {{result, "", rest}};
    }
    // Otherwise, it's (probably) the size of a type
    const auto demangle_name_result = demangle_name(str, options);
    if (!demangle_name_result)
      return std::nullopt;

    result += demangle_name_result->full;
    result += post;
    return {{result, "", demangle_name_result->rest}};
  }

  // Handle qualified names
  if (str.starts_with('Q'))
  {
    const auto demangle_qual_result = demangle_qualified_name(str, options);
    if (!demangle_qual_result)
      return std::nullopt;

    result += demangle_qual_result->full;
    result += post;
    return {{result, "", demangle_qual_result->rest}};
  }

  bool is_member = false;
  bool const_member = false;
  if (str.starts_with('M'))
  {
    is_member = true;
    const auto demangle_qual_result = demangle_qualified_name(str.substr(1), options);
    if (!demangle_qual_result)
      return std::nullopt;

    rest = demangle_qual_result->rest;
    pre = fmt::format("{}::*{}", demangle_qual_result->full, pre);
    if (!rest.starts_with('F'))
    {
      return std::nullopt;
    }
    str = rest;
  }
  if (is_member || str.starts_with('F'))
  {
    str.remove_prefix(1);
    if (is_member)
    {
      // "const void*, const void*" or "const void*, void*"
      if (str.starts_with("PCvPCv"))
      {
        const_member = true;
        str.remove_prefix(6);
      }
      else if (str.starts_with("PCvPv"))
      {
        str.remove_prefix(5);
      }
      else
      {
        return std::nullopt;
      }
    }
    else if (post.starts_with('*'))
    {
      post = StripLeadingWhitespace(post.substr(1));
      pre = fmt::format("*{}", pre);
    }
    else
    {
      return std::nullopt;
    }

    const auto demangle_func_args_result = demangle_function_args(str, options);
    if (!demangle_func_args_result)
      return std::nullopt;

    const auto demangled_func_args = demangle_func_args_result.value();

    if (!demangled_func_args.rest.starts_with('_'))
    {
      return std::nullopt;
    }

    const auto demangle_arg_result = demangle_arg(demangled_func_args.rest.substr(1), options);
    if (!demangle_arg_result)
      return std::nullopt;

    const std::string const_str = const_member ? " const" : "";
    const std::string res_pre = fmt::format("{} ({}{}", demangle_arg_result->arg_pre, pre, post);
    const std::string res_post = fmt::format(")({}){}{}", demangled_func_args.args, const_str,
                                             demangle_arg_result->arg_post);
    return {{res_pre, res_post, demangle_arg_result->rest}};
  }

  if (str.starts_with('A'))
  {
    const auto parse_result = parse_digits(str.substr(1));
    if (!parse_result)
      return std::nullopt;

    auto& [count, rest_value] = parse_result.value();
    rest = rest_value;

    if (!rest.starts_with('_'))
    {
      return std::nullopt;
    }

    const auto demangle_arg_result = demangle_arg(rest.substr(1), options);
    if (!demangle_arg_result)
      return std::nullopt;

    if (!post.empty())
    {
      post = fmt::format("({})", post);
    }
    result = fmt::format("{}{}{}", pre, demangle_arg_result->arg_pre, post);
    const std::string ret_post = fmt::format("[{}]{}", count, demangle_arg_result->arg_post);
    return {{result, ret_post, demangle_arg_result->rest}};
  }

  if (str.length() == 0)
    return std::nullopt;

  std::string type;

  char c = str[0];

  if (types.contains(c))
  {
    type = types.at(c);
  }
  else
  {
    // Handle special cases
    switch (c)
    {
    case '1':
      if (options.mw_extensions)
        type = "__int128";
      break;
    case '2':
      if (options.mw_extensions)
        type = "__vec2x32float__";
      break;
    case '_':
      return {{result, "", rest}};
    default:
      return std::nullopt;
    }
  }

  result += type;

  result += post;
  return {{result, "", str.substr(1)}};
}

std::optional<DemangleFunctionArgsResult> demangle_function_args(std::string_view str,
                                                                 DemangleOptions options)
{
  std::string result;

  while (!str.empty())
  {
    if (!result.empty())
    {
      result += ", ";
    }

    const auto demangle_arg_result = demangle_arg(str, options);
    if (!demangle_arg_result)
      return std::nullopt;

    result += demangle_arg_result->arg_pre;
    result += demangle_arg_result->arg_post;
    str = demangle_arg_result->rest;

    if (str.starts_with('_') || str.starts_with(','))
    {
      break;
    }
  }

  return {{result, str}};
}

std::optional<std::string> demangle_special_function(std::string_view str,
                                                     std::string_view class_name,
                                                     DemangleOptions options)
{
  if (str.starts_with("op"))
  {
    const std::string_view rest = str.substr(2);
    const auto demangle_arg_result = demangle_arg(rest, options);
    if (!demangle_arg_result)
      return std::nullopt;

    return fmt::format("operator {}{}", demangle_arg_result->arg_pre,
                       demangle_arg_result->arg_post);
  }

  const auto result = demangle_template_args(str, options);
  if (!result)
    return std::nullopt;

  auto& [op, args] = result.value();

  std::string func_name;

  if (op == "dt")
  {
    return fmt::format("~{}{}", class_name, args);
  }
  else if (op == "ct")
  {
    func_name = class_name;
  }
  else if (operators.contains(op.data()))
  {
    func_name = operators.at(op.data());
  }
  else
  {
    return fmt::format("__{}{}", op, args);
  }

  return fmt::format("{0}{1}", func_name, args);
}

// Demangle a symbol name.
//
// Returns `std::nullopt` if the input is not a valid mangled name.
std::optional<std::string> demangle(std::string_view str, DemangleOptions options)
{
  if (!IsAscii(str))
  {
    return std::nullopt;
  }

  bool special = false;
  bool cnst = false;
  std::string fn_name;
  std::string static_var;

  // Handle new static function variables (Wii CW)
  bool guard = str.starts_with("@GUARD@");
  if (guard || str.starts_with("@LOCAL@"))
  {
    str = str.substr(7);
    std::size_t idx = str.rfind('@');
    if (idx == std::string::npos)
      return std::nullopt;

    const std::string_view rest = str.substr(0, idx);
    const std::string_view var = str.substr(idx);

    if (guard)
    {
      static_var = fmt::format("{0} guard", var.substr(1));
    }
    else
    {
      static_var = var.substr(1);
    }
    str = rest;
  }

  if (str.starts_with("__"))
  {
    special = true;
    str = str.substr(2);
  }
  {
    auto idx_temp = find_split(str, special, options);
    if (!idx_temp)
      return std::nullopt;
    std::size_t idx = idx_temp.value();
    // Handle any trailing underscores in the function name
    while (str[idx + 2] == '_')
    {
      idx++;
    }

    const std::string_view fn_name_out = str.substr(0, idx);
    std::string_view rest = str.substr(idx);

    if (special)
    {
      if (fn_name_out == "init")
      {
        // Special case for double __
        std::size_t rest_idx = rest.substr(2).find("__");
        if (rest_idx == std::string::npos)
          return std::nullopt;
        fn_name = str.substr(0, rest_idx + 6);
        rest.remove_prefix(rest_idx + 2);
      }
      else
      {
        fn_name = fn_name_out;
      }
    }
    else
    {
      const auto result = demangle_template_args(fn_name_out, options);
      if (!result)
        return std::nullopt;

      const auto template_args = result.value();
      fn_name = fmt::format("{}{}", template_args.name, template_args.args);
    }

    // Handle old static function variables (GC CW)
    std::size_t first_idx = fn_name.find('$');
    if (first_idx != std::string::npos)
    {
      std::size_t second_idx = fn_name.substr(first_idx + 1).find('$');
      if (second_idx == std::string::npos)
        return std::nullopt;

      const std::string var = fn_name.substr(0, first_idx);
      std::string rest_temp = fn_name.substr(first_idx);

      rest_temp = rest_temp.substr(1);
      const std::string var_type = rest_temp.substr(0, second_idx);
      rest_temp = rest_temp.substr(second_idx);

      if (!var_type.starts_with("localstatic"))
      {
        return std::nullopt;
      }

      if (var == "init")
      {
        // Sadly, $localstatic doesn't provide the variable name in guard/init
        static_var = fmt::format("{} guard", var_type);
      }
      else
      {
        static_var = var;
      }

      fn_name = rest_temp.substr(1);
    }

    str = rest.substr(2);
  }

  std::string class_name;
  std::string return_type_pre;
  std::string return_type_post;
  std::string qualified;

  if (!str.starts_with('F'))
  {
    const auto result = demangle_qualified_name(str, options);
    if (!result)
      return std::nullopt;

    class_name = result->class_name;
    qualified = result->full;
    str = result->rest;
  }
  if (special)
  {
    const auto result = demangle_special_function(fn_name, class_name, options);
    if (!result)
      return std::nullopt;
    fn_name = result.value();
  }
  if (str.starts_with('C'))
  {
    str.remove_prefix(1);
    cnst = true;
  }
  if (str.starts_with('F'))
  {
    str.remove_prefix(1);
    const auto result = demangle_function_args(str, options);
    if (!result)
      return std::nullopt;

    if (options.omit_empty_parameters && result->args == "void")
    {
      fn_name = fmt::format("{}()", fn_name);
    }
    else
    {
      fn_name = fmt::format("{}({})", fn_name, result->args);
    }
    str = result->rest;
  }
  if (str.starts_with('_'))
  {
    str.remove_prefix(1);
    const auto result = demangle_arg(str, options);
    if (!result)
      return std::nullopt;

    return_type_pre = result->arg_pre;
    return_type_post = result->arg_post;
    str = result->rest;
  }
  if (!str.empty())
  {
    return std::nullopt;
  }
  if (cnst)
  {
    fn_name = fmt::format("{} const", fn_name);
  }
  if (!qualified.empty())
  {
    fn_name = fmt::format("{}::{}", qualified, fn_name);
  }
  if (!return_type_pre.empty())
  {
    fn_name = fmt::format("{} {}{}", return_type_pre, fn_name, return_type_post);
  }
  if (!static_var.empty())
  {
    fn_name = fmt::format("{}::{}", fn_name, static_var);
  }

  return fn_name;
}

// Finds the first double underscore in the string, excluding any that are part of a
// template argument list or operator name.
std::optional<std::size_t> find_split(std::string_view s, bool special, DemangleOptions options)
{
  std::size_t start = 0;

  if (special && s.starts_with("op"))
  {
    const auto result = demangle_arg(s.substr(2), options);
    if (!result)
      return std::nullopt;

    const std::string_view rest = result->rest;

    start = s.length() - rest.length();
  }

  int depth = 0;
  std::size_t length = s.length();

  for (std::size_t i = start; i < length; i++)
  {
    switch (s[i])
    {
    case '<':
      depth++;
      break;
    case '>':
      depth--;
      break;
    case '_':
      if (i < length - 1 && s[i + 1] == '_' && depth == 0)
      {
        return i;
      }
      break;
    default:
      break;
    }
  }

  return std::nullopt;
}

}  // namespace CWDemangler
