#include "Core/PowerPC/CWDemangler.h"

#include <cctype>
#include <map>
#include <sstream>

#include <fmt/format.h>

#include "Common/StringUtil.h"

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

std::tuple<std::string, std::string, std::string> CWDemangler::parse_qualifiers(std::string str)
{
  std::string pre = "";
  std::string post = "";
  int index = 0;

  for (char c : str)
  {
    bool foundNonQualifier = false;

    switch (c)
    {
    case 'P':
      if (pre == "")
      {
        post = post.insert(0, "*");
      }
      else
      {
        post = post.insert(0, fmt::format("* {0}", StripWhitespaceEnd(pre)));
        pre = "";
      }
      break;
    case 'R':
      if (pre == "")
      {
        post = post.insert(0, "&");
      }
      else
      {
        post = post.insert(0, fmt::format("& {0}", StripWhitespaceEnd(pre)));
        pre = "";
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
      foundNonQualifier = true;
      break;
    };

    if (foundNonQualifier)
      break;
    index++;
  }
  str = str.substr(index);
  post = StripWhitespaceEnd(post);
  return {pre, post, str};
}

std::optional<std::tuple<size_t, std::string>> CWDemangler::parse_digits(std::string str)
{
  bool containsNonDigit = false;
  size_t idx = 0;
  while (idx < str.length())
  {
    if (!std::isdigit(str[idx]))
    {
      containsNonDigit = true;
      break;
    }
    idx++;
  }

  size_t val = 0;

  if (containsNonDigit)
  {
    std::istringstream iss(str.substr(0, idx));
    iss >> val;

    if (!iss)
      return {};
    return {{val, str.substr(idx)}};
  }
  else
  {
    // all digits!
    std::istringstream iss(str);
    iss >> val;

    if (!iss)
      return {};
    return {{val, ""}};
  }
}

std::optional<std::tuple<std::string, std::string>>
CWDemangler::demangle_template_args(std::string str, DemangleOptions options)
{
  size_t start_idx = str.find('<');
  std::string tmpl_args;

  if (start_idx != std::string::npos)
  {
    size_t end_idx = str.rfind('>');
    if (end_idx == std::string::npos || end_idx < start_idx)
    {
      return {};
    }

    std::string args = str.substr(start_idx + 1, end_idx - (start_idx + 1));
    str.resize(start_idx);
    tmpl_args = "<";

    while (args != "")
    {
      auto result = demangle_arg(args, options);
      if (!result)
        return {};

      std::string arg;
      std::string arg_post;
      std::string rest;

      std::tie(arg, arg_post, rest) = result.value();
      tmpl_args += arg;
      tmpl_args += arg_post;

      if (rest == "")
      {
        break;
      }
      else
      {
        tmpl_args += ", ";
      }

      args = rest.substr(1);
    }

    tmpl_args += ">";
  }
  else
  {
    tmpl_args = "";
  };
  return {{str, tmpl_args}};
}

std::optional<std::tuple<std::string, std::string, std::string>>
CWDemangler::demangle_name(std::string str, DemangleOptions options)
{
  auto result = parse_digits(str);
  if (!result)
    return {};

  size_t size;
  std::string rest;
  std::tie(size, rest) = result.value();

  if (rest.length() < size)
  {
    return {};
  }

  auto result1 = demangle_template_args(rest.substr(0, size), options);
  if (!result1)
    return {};

  std::string name, args;
  std::tie(name, args) = result1.value();
  return {{name, fmt::format("{0}{1}", name, args), rest.substr(size)}};
}

std::optional<std::tuple<std::string, std::string, std::string>>
CWDemangler::demangle_qualified_name(std::string str, DemangleOptions options)
{
  if (str.starts_with('Q'))
  {
    if (str.length() < 3)
    {
      return {};
    }

    int count = 0;

    if (!(std::istringstream(str.substr(1, 1)) >> count))
      return {};
    str = str.substr(2);

    std::string last_class = "";
    std::string qualified = "";

    for (int i = 0; i < count; i++)
    {
      auto result = demangle_name(str, options);
      if (!result)
        return {};

      std::string class_name, full, rest;

      std::tie(class_name, full, rest) = result.value();
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
  else
  {
    return demangle_name(str, options);
  }
}

std::optional<std::tuple<std::string, std::string, std::string>>
CWDemangler::demangle_arg(std::string str, DemangleOptions options)
{
  // Negative constant
  if (str.starts_with('-'))
  {
    auto parseResult = parse_digits(str.substr(1));
    if (!parseResult)
      return {};

    int size;
    std::string restVal;

    std::tie(size, restVal) = parseResult.value();
    std::string outVal = fmt::format("-{}", size);
    return {{outVal, "", restVal}};
  }

  std::string result = "";

  std::string pre, post, rest;
  std::tie(pre, post, rest) = parse_qualifiers(str);
  result += pre;
  str = rest;

  // Disambiguate arguments starting with a number
  if (str.length() > 0 && std::isdigit(str[0]))
  {
    auto parseResult = parse_digits(str);
    if (!parseResult)
      return {};

    int num;

    std::tie(num, rest) = parseResult.value();
    // If the number is followed by a comma or the end of the string, it's a template argument
    if (rest == "" || rest.starts_with(','))
    {
      // ...or a Metrowerks extension type
      if (options.mw_extensions)
      {
        std::string t = num == 1 ? "__int128" : num == 2 ? "__vec2x32float__" : "";

        if (t != "")
        {
          result += t;
          return {{result, post, rest}};
        }
      }
      result += std::to_string(num);
      result += post;
      return {{result, "", rest}};
    }
    // Otherwise, it's (probably) the size of a type
    auto demangleNameResult = demangle_name(str, options);
    if (!demangleNameResult)
      return {};

    std::string qualified;
    std::tie(std::ignore, qualified, rest) = demangleNameResult.value();
    result += qualified;
    result += post;
    return {{result, "", rest}};
  }

  // Handle qualified names
  if (str.starts_with('Q'))
  {
    auto demangleQualResult = demangle_qualified_name(str, options);
    if (!demangleQualResult)
      return {};

    std::string qualified;
    std::tie(std::ignore, qualified, rest) = demangleQualResult.value();
    result += qualified;
    result += post;
    return {{result, "", rest}};
  }

  bool is_member = false;
  bool const_member = false;
  if (str.starts_with('M'))
  {
    is_member = true;
    auto demangleQualResult = demangle_qualified_name(str.substr(1), options);
    if (!demangleQualResult)
      return {};

    std::string member;
    std::tie(std::ignore, member, rest) = demangleQualResult.value();
    pre = fmt::format("{}::*{}", member, pre);
    if (!rest.starts_with('F'))
    {
      return {};
    }
    str = rest;
  }
  if (is_member || str.starts_with('F'))
  {
    str = str.substr(1);
    if (is_member)
    {
      // "const void*, const void*" or "const void*, void*"
      if (str.starts_with("PCvPCv"))
      {
        const_member = true;
        str = str.substr(6);
      }
      else if (str.starts_with("PCvPv"))
      {
        str = str.substr(5);
      }
      else
      {
        return {};
      }
    }
    else if (post.starts_with('*'))
    {
      post = StripWhitespaceStart(post.substr(1));
      pre = fmt::format("*{}", pre);
    }
    else
    {
      return {};
    }

    auto demangleFuncArgsResult = demangle_function_args(str, options);
    if (!demangleFuncArgsResult)
      return {};

    std::string args;
    std::tie(args, rest) = demangleFuncArgsResult.value();

    if (!rest.starts_with('_'))
    {
      return {};
    }

    auto demangleArgResult = demangle_arg(rest.substr(1), options);
    if (!demangleArgResult)
      return {};

    std::string ret_pre, ret_post;
    std::tie(ret_pre, ret_post, rest) = demangleArgResult.value();
    std::string const_str = const_member ? " const" : "";
    std::string res_pre = fmt::format("{} ({}{}", ret_pre, pre, post);
    std::string res_post = fmt::format(")({}){}{}", args, const_str, ret_post);
    return {{res_pre, res_post, rest}};
  }

  if (str.starts_with('A'))
  {
    auto parseResult = parse_digits(str.substr(1));
    if (!parseResult)
      return {};

    int count;
    std::tie(count, rest) = parseResult.value();

    if (!rest.starts_with('_'))
    {
      return {};
    }

    auto demangleArgResult = demangle_arg(rest.substr(1), options);
    if (!demangleArgResult)
      return {};

    std::string arg_pre, arg_post;
    std::tie(arg_pre, arg_post, rest) = demangleArgResult.value();

    if (post != "")
    {
      post = fmt::format("({})", post);
    }
    result = fmt::format("{}{}{}", pre, arg_pre, post);
    std::string ret_post = fmt::format("[{}]{}", count, arg_post);
    return {{result, ret_post, rest}};
  }

  if (str.length() == 0)
    return {};

  std::string type = "";

  switch (str[0])
  {
  case 'i':
    type = "int";
    break;
  case 'b':
    type = "bool";
    break;
  case 'c':
    type = "char";
    break;
  case 's':
    type = "short";
    break;
  case 'l':
    type = "long";
    break;
  case 'x':
    type = "long long";
    break;
  case 'f':
    type = "float";
    break;
  case 'd':
    type = "double";
    break;
  case 'w':
    type = "wchar_t";
    break;
  case 'v':
    type = "void";
    break;
  case 'e':
    type = "...";
    break;
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
    return {};
  }

  result += type;

  result += post;
  return {{result, "", str.substr(1)}};
}

std::optional<std::tuple<std::string, std::string>>
CWDemangler::demangle_function_args(std::string str, DemangleOptions options)
{
  std::string result = "";

  while (str != "")
  {
    if (result != "")
    {
      result += ", ";
    }

    auto demangleArgResult = demangle_arg(str, options);
    if (!demangleArgResult)
      return {};

    std::string arg, arg_post, rest;

    std::tie(arg, arg_post, rest) = demangleArgResult.value();

    result += arg;
    result += arg_post;
    str = rest;

    if (str.starts_with('_') || str.starts_with(','))
    {
      break;
    }
  }

  return {{result, str}};
}

std::optional<std::string> CWDemangler::demangle_special_function(std::string str,
                                                                  std::string class_name,
                                                                  DemangleOptions options)
{
  if (str.starts_with("op"))
  {
    std::string rest = str.substr(2);
    auto demangleArgResult = demangle_arg(rest, options);
    if (!demangleArgResult)
      return {};

    std::string arg_pre, arg_post;

    std::tie(arg_pre, arg_post, std::ignore) = demangleArgResult.value();
    return fmt::format("operator {}{}", arg_pre, arg_post);
  }

  auto result = demangle_template_args(str, options);
  if (!result)
    return {};

  std::string op, args;

  std::tie(op, args) = result.value();

  std::string funcName;

  if (op == "dt")
  {
    return fmt::format("~{}{}", class_name, args);
  }
  else if (op == "ct")
  {
    funcName = class_name;
  }
  else if (operators.contains(op))
  {
    funcName = operators.at(op);
  }
  else
  {
    return fmt::format("__{}{}", op, args);
  }

  return fmt::format("{0}{1}", funcName, args);
}

/// Demangle a symbol name.
///
/// Returns `null` if the input is not a valid mangled name.
std::optional<std::string> CWDemangler::demangle(std::string str, DemangleOptions options)
{
  if (!IsAscii(str))
  {
    return {};
  }

  bool special = false;
  bool cnst = false;
  std::string fn_name;
  std::string return_type_pre = "";
  std::string return_type_post = "";
  std::string qualified = "";
  std::string static_var = "";

  // Handle new static function variables (Wii CW)
  bool guard = str.starts_with("@GUARD@");
  if (guard || str.starts_with("@LOCAL@"))
  {
    str = str.substr(7);
    size_t idx = str.rfind('@');
    if (idx == std::string::npos)
      return {};

    std::string rest = str.substr(0, idx);
    std::string var = str.substr(idx);

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
    auto idxTemp = find_split(str, special, options);
    if (!idxTemp)
      return {};
    size_t idx = idxTemp.value();
    // Handle any trailing underscores in the function name
    while (str[idx + 2] == '_')
    {
      idx++;
    }

    std::string fn_name_out = str.substr(0, idx);
    std::string rest = str.substr(idx);

    if (special)
    {
      if (fn_name_out == "init")
      {
        // Special case for double __
        size_t rest_idx = rest.substr(2).find("__");
        if (rest_idx == std::string::npos)
          return {};
        fn_name = str.substr(0, rest_idx + 6);
        rest = rest.substr(rest_idx + 2);
      }
      else
      {
        fn_name = fn_name_out;
      }
    }
    else
    {
      auto result = demangle_template_args(fn_name_out, options);
      if (!result)
        return {};

      std::string name;
      std::string args;

      std::tie(name, args) = result.value();
      fn_name = fmt::format("{}{}", name, args);
    }

    // Handle old static function variables (GC CW)
    size_t first_idx = fn_name.find('$');
    if (first_idx != std::string::npos)
    {
      size_t second_idx = fn_name.substr(first_idx + 1).find('$');
      if (second_idx == std::string::npos)
        return {};

      std::string var = fn_name.substr(0, first_idx);
      std::string restTemp = fn_name.substr(first_idx);

      restTemp = restTemp.substr(1);
      std::string var_type = restTemp.substr(0, second_idx);
      restTemp = restTemp.substr(second_idx);

      if (!var_type.starts_with("localstatic"))
      {
        return {};
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

      fn_name = restTemp.substr(1);
    }

    str = rest.substr(2);
  }

  std::string class_name = "";
  if (!str.starts_with('F'))
  {
    auto result = demangle_qualified_name(str, options);
    if (!result)
      return {};

    std::string name;
    std::string qualified_name;
    std::string rest;

    std::tie(name, qualified_name, rest) = result.value();
    class_name = name;
    qualified = qualified_name;
    str = rest;
  }
  if (special)
  {
    auto result = demangle_special_function(fn_name, class_name, options);
    if (!result)
      return {};
    fn_name = result.value();
  }
  if (str.starts_with('C'))
  {
    str = str.substr(1);
    cnst = true;
  }
  if (str.starts_with('F'))
  {
    str = str.substr(1);
    auto result = demangle_function_args(str, options);
    if (!result)
      return {};

    std::string args;
    std::string rest;

    std::tie(args, rest) = result.value();
    if (options.omit_empty_parameters && args == "void")
    {
      fn_name = fmt::format("{}()", fn_name);
    }
    else
    {
      fn_name = fmt::format("{}({})", fn_name, args);
    }
    str = rest;
  }
  if (str.starts_with('_'))
  {
    str = str.substr(1);
    auto result = demangle_arg(str, options);
    if (!result)
      return {};

    std::string ret_pre;
    std::string ret_post;
    std::string rest;

    std::tie(ret_pre, ret_post, rest) = result.value();
    return_type_pre = ret_pre;
    return_type_post = ret_post;
    str = rest;
  }
  if (str != "")
  {
    return {};
  }
  if (cnst)
  {
    fn_name = fmt::format("{} const", fn_name);
  }
  if (qualified != "")
  {
    fn_name = fmt::format("{}::{}", qualified, fn_name);
  }
  if (return_type_pre != "")
  {
    fn_name = fmt::format("{} {}{}", return_type_pre, fn_name, return_type_post);
  }
  if (static_var != "")
  {
    fn_name = fmt::format("{}::{}", fn_name, static_var);
  }

  return fn_name;
}

/// Finds the first double underscore in the string, excluding any that are part of a
/// template argument list or operator name.
std::optional<size_t> CWDemangler::find_split(std::string s, bool special, DemangleOptions options)
{
  size_t start = 0;

  if (special && s.starts_with("op"))
  {
    auto result = demangle_arg(s.substr(2), options);
    if (!result)
      return {};

    std::string rest;

    std::tie(std::ignore, std::ignore, rest) = result.value();
    start = s.length() - rest.length();
  }

  int depth = 0;

  for (size_t i = start; i < s.length(); i++)
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
      if (s[i + 1] == '_' && depth == 0)
      {
        return i;
      }
      break;
    default:
      break;
    }
  }

  return {};
}
