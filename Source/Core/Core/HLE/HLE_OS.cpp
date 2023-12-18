// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HLE/HLE_OS.h"

#include <cinttypes>
#include <memory>
#include <string>
#include <string_view>

#include <fmt/format.h>
#include <fmt/printf.h>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/HLE/HLE_VarArgs.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"

namespace HLE_OS
{
enum class ParameterType : bool
{
  ParameterList = false,
  VariableArgumentList = true
};

static std::string GetStringVA(Core::System& system, const Core::CPUThreadGuard& guard,
                               u32 str_reg = 3,
                               ParameterType parameter_type = ParameterType::ParameterList);

void HLE_OSPanic(const Core::CPUThreadGuard& guard)
{
  auto& system = guard.GetSystem();
  auto& ppc_state = system.GetPPCState();

  std::string error = GetStringVA(system, guard);
  std::string msg = GetStringVA(system, guard, 5);

  StringPopBackIf(&error, '\n');
  StringPopBackIf(&msg, '\n');

  PanicAlertFmt("OSPanic: {}: {}", error, msg);
  ERROR_LOG_FMT(OSREPORT_HLE, "{:08x}->{:08x}| OSPanic: {}: {}", LR(ppc_state), ppc_state.pc, error,
                msg);

  ppc_state.npc = LR(ppc_state);
}

// Generalized function for printing formatted string.
static void HLE_GeneralDebugPrint(const Core::CPUThreadGuard& guard, ParameterType parameter_type)
{
  auto& system = guard.GetSystem();
  const auto& ppc_state = system.GetPPCState();

  std::string report_message;

  // Is gpr3 pointing to a pointer (including nullptr) rather than an ASCII string
  if (PowerPC::MMU::HostIsRAMAddress(guard, ppc_state.gpr[3]) &&
      (PowerPC::MMU::HostIsRAMAddress(guard, PowerPC::MMU::HostRead_U32(guard, ppc_state.gpr[3])) ||
       PowerPC::MMU::HostRead_U32(guard, ppc_state.gpr[3]) == 0))
  {
    if (PowerPC::MMU::HostIsRAMAddress(guard, ppc_state.gpr[4]))
    {
      // ___blank(void* this, const char* fmt, ...);
      report_message = GetStringVA(system, guard, 4, parameter_type);
    }
    else
    {
      // ___blank(void* this, int log_type, const char* fmt, ...);
      report_message = GetStringVA(system, guard, 5, parameter_type);
    }
  }
  else
  {
    if (PowerPC::MMU::HostIsRAMAddress(guard, ppc_state.gpr[3]))
    {
      // ___blank(const char* fmt, ...);
      report_message = GetStringVA(system, guard, 3, parameter_type);
    }
    else
    {
      // ___blank(int log_type, const char* fmt, ...);
      report_message = GetStringVA(system, guard, 4, parameter_type);
    }
  }

  StringPopBackIf(&report_message, '\n');

  NOTICE_LOG_FMT(OSREPORT_HLE, "{:08x}->{:08x}| {}", LR(ppc_state), ppc_state.pc,
                 SHIFTJISToUTF8(report_message));
}

// Generalized function for printing formatted string using parameter list.
void HLE_GeneralDebugPrint(const Core::CPUThreadGuard& guard)
{
  HLE_GeneralDebugPrint(guard, ParameterType::ParameterList);
}

// Generalized function for printing formatted string using va_list.
void HLE_GeneralDebugVPrint(const Core::CPUThreadGuard& guard)
{
  HLE_GeneralDebugPrint(guard, ParameterType::VariableArgumentList);
}

// __write_console(int fd, const void* buffer, const u32* size)
void HLE_write_console(const Core::CPUThreadGuard& guard)
{
  auto& system = guard.GetSystem();
  const auto& ppc_state = system.GetPPCState();

  std::string report_message = GetStringVA(system, guard, 4);
  if (PowerPC::MMU::HostIsRAMAddress(guard, ppc_state.gpr[5]))
  {
    const u32 size = system.GetMMU().Read_U32(ppc_state.gpr[5]);
    if (size > report_message.size())
      WARN_LOG_FMT(OSREPORT_HLE, "__write_console uses an invalid size of {:#010x}", size);
    else if (size == 0)
      WARN_LOG_FMT(OSREPORT_HLE, "__write_console uses a size of zero");
    else
      report_message = report_message.substr(0, size);
  }
  else
  {
    ERROR_LOG_FMT(OSREPORT_HLE, "__write_console uses an unreachable size pointer");
  }

  StringPopBackIf(&report_message, '\n');

  NOTICE_LOG_FMT(OSREPORT_HLE, "{:08x}->{:08x}| {}", LR(ppc_state), ppc_state.pc,
                 SHIFTJISToUTF8(report_message));
}

// Log (v)dprintf message if fd is 1 (stdout) or 2 (stderr)
static void HLE_LogDPrint(const Core::CPUThreadGuard& guard, ParameterType parameter_type)
{
  auto& system = guard.GetSystem();
  const auto& ppc_state = system.GetPPCState();

  if (ppc_state.gpr[3] != 1 && ppc_state.gpr[3] != 2)
    return;

  std::string report_message = GetStringVA(system, guard, 4, parameter_type);
  StringPopBackIf(&report_message, '\n');
  NOTICE_LOG_FMT(OSREPORT_HLE, "{:08x}->{:08x}| {}", LR(ppc_state), ppc_state.pc,
                 SHIFTJISToUTF8(report_message));
}

// Log dprintf message
//  -> int dprintf(int fd, const char* format, ...);
void HLE_LogDPrint(const Core::CPUThreadGuard& guard)
{
  HLE_LogDPrint(guard, ParameterType::ParameterList);
}

// Log vdprintf message
//  -> int vdprintf(int fd, const char* format, va_list ap);
void HLE_LogVDPrint(const Core::CPUThreadGuard& guard)
{
  HLE_LogDPrint(guard, ParameterType::VariableArgumentList);
}

// Log (v)fprintf message if FILE is stdout or stderr
static void HLE_LogFPrint(const Core::CPUThreadGuard& guard, ParameterType parameter_type)
{
  auto& system = guard.GetSystem();
  const auto& ppc_state = system.GetPPCState();

  // The structure FILE is implementation defined.
  // Both libogc and Dolphin SDK seem to store the fd at the same address.
  int fd = -1;
  if (PowerPC::MMU::HostIsRAMAddress(guard, ppc_state.gpr[3]) &&
      PowerPC::MMU::HostIsRAMAddress(guard, ppc_state.gpr[3] + 0xF))
  {
    // The fd is stored as a short at FILE+0xE.
    fd = static_cast<short>(PowerPC::MMU::HostRead_U16(guard, ppc_state.gpr[3] + 0xE));
  }
  if (fd != 1 && fd != 2)
  {
    // On RVL SDK it seems stored at FILE+0x2.
    fd = static_cast<short>(PowerPC::MMU::HostRead_U16(guard, ppc_state.gpr[3] + 0x2));
  }
  if (fd != 1 && fd != 2)
    return;

  std::string report_message = GetStringVA(system, guard, 4, parameter_type);
  StringPopBackIf(&report_message, '\n');
  NOTICE_LOG_FMT(OSREPORT_HLE, "{:08x}->{:08x}| {}", LR(ppc_state), ppc_state.pc,
                 SHIFTJISToUTF8(report_message));
}

// Log fprintf message
//  -> int fprintf(FILE* stream, const char* format, ...);
void HLE_LogFPrint(const Core::CPUThreadGuard& guard)
{
  HLE_LogFPrint(guard, ParameterType::ParameterList);
}

// Log vfprintf message
//  -> int vfprintf(FILE* stream, const char* format, va_list ap);
void HLE_LogVFPrint(const Core::CPUThreadGuard& guard)
{
  HLE_LogFPrint(guard, ParameterType::VariableArgumentList);
}

namespace
{
class HLEPrintArgsVAList final : public HLEPrintArgs
{
public:
  HLEPrintArgsVAList(const Core::CPUThreadGuard& guard, HLE::SystemVABI::VAList* list)
      : m_guard(guard), m_va_list(list)
  {
  }

  u32 GetU32() override { return m_va_list->GetArgT<u32>(); }
  u64 GetU64() override { return m_va_list->GetArgT<u64>(); }
  double GetF64() override { return m_va_list->GetArgT<double>(); }
  std::string GetString(std::optional<u32> max_length) override
  {
    return max_length == 0u ? std::string() :
                              PowerPC::MMU::HostGetString(m_guard, m_va_list->GetArgT<u32>(),
                                                          max_length.value_or(0u));
  }
  std::u16string GetU16String(std::optional<u32> max_length) override
  {
    return max_length == 0u ? std::u16string() :
                              PowerPC::MMU::HostGetU16String(m_guard, m_va_list->GetArgT<u32>(),
                                                             max_length.value_or(0u));
  }

private:
  const Core::CPUThreadGuard& m_guard;
  HLE::SystemVABI::VAList* m_va_list;
};
}  // namespace

static std::string GetStringVA(Core::System& system, const Core::CPUThreadGuard& guard, u32 str_reg,
                               ParameterType parameter_type)
{
  auto& ppc_state = system.GetPPCState();

  std::string string = PowerPC::MMU::HostGetString(guard, ppc_state.gpr[str_reg]);
  std::unique_ptr<HLE::SystemVABI::VAList> ap =
      parameter_type == ParameterType::VariableArgumentList ?
          std::make_unique<HLE::SystemVABI::VAListStruct>(guard, ppc_state.gpr[str_reg + 1]) :
          std::make_unique<HLE::SystemVABI::VAList>(guard, ppc_state.gpr[1] + 0x8, str_reg + 1);

  HLEPrintArgsVAList args(guard, ap.get());
  return GetStringVA(&args, string);
}

std::string GetStringVA(HLEPrintArgs* args, std::string_view string)
{
  std::string result;
  for (size_t i = 0; i < string.size(); ++i)
  {
    if (string[i] != '%')
    {
      result += string[i];
      continue;
    }

    const size_t formatting_start_position = i;
    ++i;
    if (i < string.size() && string[i] == '%')
    {
      result += '%';
      continue;
    }

    bool left_justified_flag = false;
    bool sign_prepended_flag = false;
    bool space_prepended_flag = false;
    bool alternative_form_flag = false;
    bool padding_zeroes_flag = false;
    while (i < string.size())
    {
      if (string[i] == '-')
        left_justified_flag = true;
      else if (string[i] == '+')
        sign_prepended_flag = true;
      else if (string[i] == ' ')
        space_prepended_flag = true;
      else if (string[i] == '#')
        alternative_form_flag = true;
      else if (string[i] == '0')
        padding_zeroes_flag = true;
      else
        break;
      ++i;
    }

    const auto take_field_or_precision = [&](bool* left_justified_flag_ptr) -> std::optional<u32> {
      if (i >= string.size())
        return std::nullopt;

      if (string[i] == '*')
      {
        ++i;
        const s32 result_tmp = Common::BitCast<s32>(args->GetU32());
        if (result_tmp >= 0)
          return static_cast<u32>(result_tmp);

        if (left_justified_flag_ptr)
        {
          // field width; this results in positive field width and left_justified flag set
          *left_justified_flag_ptr = true;
          return static_cast<u32>(-static_cast<s64>(result_tmp));
        }

        // precision; this is ignored
        return std::nullopt;
      }

      size_t start = i;
      while (i < string.size() && string[i] >= '0' && string[i] <= '9')
        ++i;
      if (start != i)
      {
        while (start < i && string[start] == '0')
          ++start;
        if (start == i)
          return 0;
        u32 result_tmp = 0;
        const std::string tmp(string, start, i - start);
        if (TryParse(tmp, &result_tmp, 10))
          return result_tmp;
      }

      return std::nullopt;
    };

    const std::optional<u32> field_width = take_field_or_precision(&left_justified_flag);
    std::optional<u32> precision = std::nullopt;
    if (i < string.size() && string[i] == '.')
    {
      ++i;
      precision = take_field_or_precision(nullptr).value_or(0);
    }

    enum class LengthModifier
    {
      None,
      hh,
      h,
      l,
      ll,
      L,
    };
    auto length_modifier = LengthModifier::None;

    if (i < string.size() && (string[i] == 'h' || string[i] == 'l' || string[i] == 'L'))
    {
      ++i;
      if (i < string.size() && string[i - 1] == 'h' && string[i] == 'h')
      {
        ++i;
        length_modifier = LengthModifier::hh;
      }
      else if (i < string.size() && string[i - 1] == 'l' && string[i] == 'l')
      {
        ++i;
        length_modifier = LengthModifier::ll;
      }
      else if (string[i - 1] == 'h')
      {
        length_modifier = LengthModifier::h;
      }
      else if (string[i - 1] == 'l')
      {
        length_modifier = LengthModifier::l;
      }
      else if (string[i - 1] == 'L')
      {
        length_modifier = LengthModifier::L;
      }
    }

    if (i >= string.size())
    {
      // not a valid formatting string, print the formatting string as-is
      result += string.substr(formatting_start_position);
      break;
    }

    const char format_specifier = string[i];
    switch (format_specifier)
    {
    case 's':
    {
      if (length_modifier == LengthModifier::l)
      {
        // This is a bit of a mess... wchar_t could be 16 bits or 32 bits per character depending
        // on the software. Retail software seems usually to use 16 bits and homebrew 32 bits, but
        // that's really just a guess. Ideally we can figure out a way to autodetect this, but if
        // not we should probably just expose a setting for it in the debugger somewhere. For now
        // we just assume 16 bits.
        fmt::format_to(
            std::back_inserter(result), fmt::runtime(left_justified_flag ? "{0:<{1}}" : "{0:>{1}}"),
            UTF8ToSHIFTJIS(UTF16ToUTF8(args->GetU16String(precision))), field_width.value_or(0));
      }
      else
      {
        fmt::format_to(std::back_inserter(result),
                       fmt::runtime(left_justified_flag ? "{0:<{1}}" : "{0:>{1}}"),
                       args->GetString(precision), field_width.value_or(0));
      }
      break;
    }
    case 'c':
    {
      const s32 value = Common::BitCast<s32>(args->GetU32());
      if (length_modifier == LengthModifier::l)
      {
        // Same problem as with wide strings here.
        const char16_t wide_char = static_cast<char16_t>(value);
        fmt::format_to(std::back_inserter(result),
                       fmt::runtime(left_justified_flag ? "{0:<{1}}" : "{0:>{1}}"),
                       UTF8ToSHIFTJIS(UTF16ToUTF8(std::u16string_view(&wide_char, 1))),
                       field_width.value_or(0));
      }
      else
      {
        fmt::format_to(std::back_inserter(result),
                       fmt::runtime(left_justified_flag ? "{0:<{1}}" : "{0:>{1}}"),
                       static_cast<char>(value), field_width.value_or(0));
      }
      break;
    }
    case 'd':
    case 'i':
    {
      const auto options = fmt::format(
          "{}{}{}{}{}{}", left_justified_flag ? "-" : "", sign_prepended_flag ? "+" : "",
          space_prepended_flag ? " " : "", padding_zeroes_flag ? "0" : "",
          field_width ? fmt::format("{}", *field_width) : "",
          precision ? fmt::format(".{}", *precision) : "");
      if (length_modifier == LengthModifier::ll)
      {
        const s64 value = Common::BitCast<s64>(args->GetU64());
        result += fmt::sprintf(fmt::format("%{}" PRId64, options).c_str(), value);
      }
      else
      {
        s32 value = Common::BitCast<s32>(args->GetU32());
        if (length_modifier == LengthModifier::h)
          value = static_cast<s16>(value);
        else if (length_modifier == LengthModifier::hh)
          value = static_cast<s8>(value);
        result += fmt::sprintf(fmt::format("%{}" PRId32, options).c_str(), value);
      }
      break;
    }
    case 'o':
    {
      const auto options = fmt::format(
          "{}{}{}{}{}{}{}", left_justified_flag ? "-" : "", sign_prepended_flag ? "+" : "",
          space_prepended_flag ? " " : "", alternative_form_flag ? "#" : "",
          padding_zeroes_flag ? "0" : "", field_width ? fmt::format("{}", *field_width) : "",
          precision ? fmt::format(".{}", *precision) : "");
      if (length_modifier == LengthModifier::ll)
      {
        const u64 value = args->GetU64();
        result += fmt::sprintf(fmt::format("%{}" PRIo64, options).c_str(), value);
      }
      else
      {
        u32 value = args->GetU32();
        if (length_modifier == LengthModifier::h)
          value = static_cast<u16>(value);
        else if (length_modifier == LengthModifier::hh)
          value = static_cast<u8>(value);
        result += fmt::sprintf(fmt::format("%{}" PRIo32, options).c_str(), value);
      }
      break;
    }
    case 'x':
    case 'X':
    {
      const auto options = fmt::format(
          "{}{}{}{}{}{}{}", left_justified_flag ? "-" : "", sign_prepended_flag ? "+" : "",
          space_prepended_flag ? " " : "", alternative_form_flag ? "#" : "",
          padding_zeroes_flag ? "0" : "", field_width ? fmt::format("{}", *field_width) : "",
          precision ? fmt::format(".{}", *precision) : "");
      if (length_modifier == LengthModifier::ll)
      {
        const u64 value = args->GetU64();
        result += fmt::sprintf(
            fmt::format("%{}{}", options, format_specifier == 'x' ? PRIx64 : PRIX64).c_str(),
            value);
      }
      else
      {
        u32 value = args->GetU32();
        if (length_modifier == LengthModifier::h)
          value = static_cast<u16>(value);
        else if (length_modifier == LengthModifier::hh)
          value = static_cast<u8>(value);
        result += fmt::sprintf(
            fmt::format("%{}{}", options, format_specifier == 'x' ? PRIx32 : PRIX32).c_str(),
            value);
      }
      break;
    }
    case 'u':
    {
      const auto options = fmt::format(
          "{}{}{}{}{}{}", left_justified_flag ? "-" : "", sign_prepended_flag ? "+" : "",
          space_prepended_flag ? " " : "", padding_zeroes_flag ? "0" : "",
          field_width ? fmt::format("{}", *field_width) : "",
          precision ? fmt::format(".{}", *precision) : "");
      if (length_modifier == LengthModifier::ll)
      {
        const u64 value = args->GetU64();
        result += fmt::sprintf(fmt::format("%{}" PRIu64, options).c_str(), value);
      }
      else
      {
        u32 value = args->GetU32();
        if (length_modifier == LengthModifier::h)
          value = static_cast<u16>(value);
        else if (length_modifier == LengthModifier::hh)
          value = static_cast<u8>(value);
        result += fmt::sprintf(fmt::format("%{}" PRIu32, options).c_str(), value);
      }
      break;
    }
    case 'f':
    case 'F':
    case 'e':
    case 'E':
    case 'a':
    case 'A':
    case 'g':
    case 'G':
    {
      const auto options = fmt::format(
          "{}{}{}{}{}{}{}", left_justified_flag ? "-" : "", sign_prepended_flag ? "+" : "",
          space_prepended_flag ? " " : "", alternative_form_flag ? "#" : "",
          padding_zeroes_flag ? "0" : "", field_width ? fmt::format("{}", *field_width) : "",
          precision ? fmt::format(".{}", *precision) : "");
      double value = args->GetF64();
      result += fmt::sprintf(fmt::format("%{}{}", options, format_specifier).c_str(), value);
      break;
    }
    case 'n':
      // %n doesn't output anything, so the result variable is untouched
      // the actual PPC function will take care of the memory write
      break;
    case 'p':
    {
      const auto options =
          fmt::format("{}{}{}{}{}", left_justified_flag ? "-" : "", sign_prepended_flag ? "+" : "",
                      space_prepended_flag ? " " : "", padding_zeroes_flag ? "0" : "",
                      field_width ? fmt::format("{}", *field_width) : "");
      const u32 value = args->GetU32();
      result += fmt::sprintf(fmt::format("%{}" PRIx32, options).c_str(), value);
      break;
    }
    default:
      // invalid conversion specifier, print the formatting string as-is
      result += string.substr(formatting_start_position, formatting_start_position - i + 1);
      break;
    }
  }

  return result;
}

}  // namespace HLE_OS
