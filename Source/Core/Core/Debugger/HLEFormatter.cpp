// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "HLEFormatter.h"

#include <iterator>
#include <locale>
#include <optional>
#include <string_view>
#include <vector>

#include <fmt/args.h>
#include <fmt/format.h>

#include "Common/BitUtils.h"
#include "Common/Logging/Log.h"
#include "Core/HLE/HLE_OS.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"

template <>
struct fmt::formatter<PowerPC::PairedSingle>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const PowerPC::PairedSingle& ps, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "{:016x} {:016x}", ps.ps0, ps.ps1);
  }
};

class HLEArg
{
public:
  explicit HLEArg(u64 v) : m_value(v) {}
  explicit HLEArg(u32 v) : HLEArg(static_cast<u64>(v)) {}
  explicit HLEArg(u32 v, u32 gpr_index) : m_value(static_cast<u64>(v)), m_gpr_index(gpr_index) {}
  explicit HLEArg(double f) : m_value(Common::BitCast<u64>(f)), m_is_float(true) {}
  explicit HLEArg(float f) : HLEArg(static_cast<double>(f)) {}

  template <typename T>
  T Value() const
  {
    return static_cast<T>(m_value);
  }

  bool IsFloat() const { return m_is_float; }

  const std::optional<u32>& GprIndex() const { return m_gpr_index; }

private:
  const u64 m_value = 0;
  const bool m_is_float = false;
  const std::optional<u32> m_gpr_index;
};

template <>
double HLEArg::Value() const
{
  return Common::BitCast<double>(m_value);
}

template <>
float HLEArg::Value() const
{
  return static_cast<float>(Value<double>());
}

template <>
struct fmt::formatter<HLEArg>
{
  enum class BaseType
  {
    Default,
    Char,
    String,
    Float,
    Double,
    Pointer,
    Int8,
    Uint8,
    Int16,
    Uint16,
    Int32,
    Uint32,
    Int64,
    Uint64,
    VarArgs,
    VaList
  };
  std::string base_format;
  BaseType base_type = BaseType::Default;

  template <class It>
  constexpr std::string_view ParseBaseType(It first, It last) const
  {
    auto it = last;  // Starts at '}'
    auto type_start = it;
    while (std::isalpha(*--it, std::locale::classic()))
    {
      --type_start;
      if (it == first)
        break;
    }
    // Only available in C++20
    //
    // return {type_start, last};
    const std::size_t count = std::distance(type_start, last);
    return {type_start, count};
  }

  template <typename T>
  constexpr bool InArray(std::string_view s, T value) const
  {
    return s == value;
  }

  template <typename T, typename... Args>
  constexpr bool InArray(std::string_view s, T value, Args... values) const
  {
    return InArray(s, value) || InArray(s, values...);
  }

  constexpr BaseType GetBaseType(std::string_view specifiers) const
  {
    // Requires C++20 as std::vector isn't constexpr
    //
    // auto in_array = [&](const std::vector<const char*>& values) {
    //   return std::find(values.begin(), values.end(), specifiers) != values.end();
    // };

    auto in_array = [&](auto... values) { return InArray(specifiers, values...); };

    if (in_array("c"))  // Char values
      return BaseType::Char;
    else if (in_array("s"))
      return BaseType::String;
    else if (in_array("hhd", "hhi"))  // Signed values
      return BaseType::Int8;
    else if (in_array("hd", "hi"))
      return BaseType::Int16;
    else if (in_array("d", "i"))
      return BaseType::Int32;
    else if (in_array("ld", "li"))
      return BaseType::Int64;
    else if (in_array("hhb", "hhB", "hho", "hhO", "hhu", "hhx", "hhX"))  // Unsigned values
      return BaseType::Uint8;
    else if (in_array("hb", "hB", "ho", "hO", "hu", "hx", "hX"))
      return BaseType::Uint16;
    else if (in_array("b", "B", "o", "O", "u", "x", "X"))
      return BaseType::Uint32;
    else if (in_array("lb", "lB", "lo", "lO", "lu", "lx", "lX"))
      return BaseType::Uint64;
    else if (in_array("a", "A", "e", "E", "f", "F", "g", "G"))  // Floating-point values
      return BaseType::Float;
    else if (in_array("la", "lA", "le", "lE", "lf", "lF", "lg", "lG"))
      return BaseType::Double;
    else if (in_array("p"))  // Pointer value
      return BaseType::Pointer;
    else if (in_array("v"))
      return BaseType::VarArgs;
    else if (in_array("V"))
      return BaseType::VaList;
    return BaseType::Default;
  }

  void TranslateBaseFormat()
  {
    // Convert the type specifiers to the ones supported by {fmt}
    for (auto c = base_format.begin(); c != base_format.end();)
    {
      if (*c == 'u')
      {
        *c = 'd';
      }
      else if (*c == 'h' || *c == 'l')
      {
        c = base_format.erase(c);
        continue;
      }
      ++c;
    }
  }

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx)
  {
    const auto begin = ctx.begin();
    const auto end = ctx.end();
    if (begin == end || *begin == '}')
    {
      base_format = "{}";
      return begin;
    }

    // Let {fmt} handle the invalid end iterator
    const auto error = begin;

    // Search the closing brace
    const auto format_end = std::find(begin, end, '}');
    if (format_end == end)
      return error;

    // Get the format and type specifiers
    const std::string_view type_view = ParseBaseType(begin, format_end);
    base_type = GetBaseType(type_view);
    if (base_type == BaseType::Default && !type_view.empty())
      return error;

    if (base_type == BaseType::Pointer)
      base_format = "{:#010x}";
    else
      base_format = fmt::format("{{:{}", std::string(begin, format_end + 1));

    TranslateBaseFormat();

    return format_end;
  }

  template <typename FormatContext>
  auto format(const HLEArg& arg, FormatContext& ctx) const
  {
    const auto format_string = fmt::runtime(base_format);

    switch (base_type)
    {
    default:
    case BaseType::Default:
    {
      if (arg.IsFloat())
        return fmt::format_to(ctx.out(), format_string, arg.Value<double>());
      return fmt::format_to(ctx.out(), format_string, arg.Value<s64>());
    }
    case BaseType::Double:
      return fmt::format_to(ctx.out(), format_string, arg.Value<double>());
    case BaseType::Int64:
      return fmt::format_to(ctx.out(), format_string, arg.Value<s64>());
    case BaseType::Char:
      return fmt::format_to(ctx.out(), format_string, arg.Value<char>());
    case BaseType::String:
    {
      auto result = PowerPC::HostTryReadString(arg.Value<u32>());
      const std::string str = result ? result->value : "{InvalidRamAddress}";
      return fmt::format_to(ctx.out(), format_string, str);
    }
    case BaseType::Float:
      return fmt::format_to(ctx.out(), format_string, arg.Value<float>());
    case BaseType::Int8:
      return fmt::format_to(ctx.out(), format_string, arg.Value<s8>());
    case BaseType::Uint8:
      return fmt::format_to(ctx.out(), format_string, arg.Value<u8>());
    case BaseType::Int16:
      return fmt::format_to(ctx.out(), format_string, arg.Value<s16>());
    case BaseType::Uint16:
      return fmt::format_to(ctx.out(), format_string, arg.Value<u16>());
    case BaseType::Int32:
      return fmt::format_to(ctx.out(), format_string, arg.Value<s32>());
    case BaseType::Pointer:
    case BaseType::Uint32:
      return fmt::format_to(ctx.out(), format_string, arg.Value<u32>());
    case BaseType::Uint64:
      return fmt::format_to(ctx.out(), format_string, arg.Value<u64>());
    case BaseType::VarArgs:
    {
      const auto gpr_index = arg.GprIndex();
      if (!gpr_index || *gpr_index < 3)
        return fmt::format_to(ctx.out(), "{}", "{InvalidVarArgsRegister}");
      const std::string str = HLE_OS::GetStringVA(*gpr_index, HLE_OS::ParameterType::ParameterList);
      return fmt::format_to(ctx.out(), "{}", str);
    }
    case BaseType::VaList:
    {
      const auto gpr_index = arg.GprIndex();
      if (!gpr_index || *gpr_index < 3)
        return fmt::format_to(ctx.out(), "{}", "{InvalidVaListRegister}");
      const std::string str =
          HLE_OS::GetStringVA(*gpr_index, HLE_OS::ParameterType::VariableArgumentList);
      return fmt::format_to(ctx.out(), "{}", str);
    }
    }
  }
};

namespace Core::Debug
{
namespace
{
using HLENamedArgs = fmt::dynamic_format_arg_store<fmt::format_context>;

HLENamedArgs GetHLENamedArgs()
{
  auto args = HLENamedArgs();
  std::string name;

  auto push_hle_arg = [&](const char* arg_name, auto value,
                          std::optional<u32> gpr_index = std::nullopt) {
    const HLEArg arg = gpr_index ? HLEArg(value, *gpr_index) : HLEArg(value);
    args.push_back(fmt::arg(arg_name, arg));
  };

  for (u32 i = 0; i < 32; i++)
  {
    // General purpose registers (int)
    name = fmt::format("r{}", i);
    push_hle_arg(name.c_str(), GPR(i), i);

    // Floating point registers (double)
    name = fmt::format("f{}", i);
    args.push_back(fmt::arg(name.c_str(), rPS(i)));
    name = fmt::format("f{}_1", i);
    push_hle_arg(name.c_str(), rPS(i).PS0AsDouble());
    name = fmt::format("f{}_2", i);
    push_hle_arg(name.c_str(), rPS(i).PS1AsDouble());
  }

  // Special registers
  push_hle_arg("TB", PowerPC::ReadFullTimeBaseValue());
  push_hle_arg("PC", PowerPC::ppcState.pc);
  push_hle_arg("LR", PowerPC::ppcState.spr[SPR_LR]);
  push_hle_arg("CTR", PowerPC::ppcState.spr[SPR_CTR]);
  push_hle_arg("CR", PowerPC::ppcState.cr.Get());
  push_hle_arg("XER", PowerPC::GetXER().Hex);
  push_hle_arg("FPSCR", PowerPC::ppcState.fpscr.Hex);
  push_hle_arg("MSR", PowerPC::ppcState.msr.Hex);
  push_hle_arg("SRR0", PowerPC::ppcState.spr[SPR_SRR0]);
  push_hle_arg("SRR1", PowerPC::ppcState.spr[SPR_SRR1]);
  push_hle_arg("Exceptions", PowerPC::ppcState.Exceptions);
  push_hle_arg("IntMask", ProcessorInterface::GetMask());
  push_hle_arg("IntCause", ProcessorInterface::GetCause());
  push_hle_arg("DSISR", PowerPC::ppcState.spr[SPR_DSISR]);
  push_hle_arg("DAR", PowerPC::ppcState.spr[SPR_DAR]);
  push_hle_arg("HashMask",
               (PowerPC::ppcState.pagetable_hashmask << 6) | PowerPC::ppcState.pagetable_base);
  return args;
}
}  // namespace

std::string HLEFormatString(const std::string_view message)
{
  const HLENamedArgs args = GetHLENamedArgs();
  return fmt::vformat(message, args);
}
}  // namespace Core::Debug
