// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <fmt/format.h>

#include "Common/FormatUtil.h"

namespace Common
{
// Message alerts
enum class MsgType
{
  Information,
  Question,
  Warning,
  Critical
};

using MsgAlertHandler = bool (*)(const char* caption, const char* text, bool yes_no, MsgType style);
using StringTranslator = std::string (*)(const char* text);

void RegisterMsgAlertHandler(MsgAlertHandler handler);
void RegisterStringTranslator(StringTranslator translator);

std::string GetStringT(const char* string);

bool MsgAlert(bool yes_no, MsgType style, const char* format, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 3, 4)))
#endif
    ;

bool MsgAlertFmtImpl(bool yes_no, MsgType style, fmt::string_view format,
                     const fmt::format_args& args);

template <std::size_t NumFields, typename S, typename... Args>
bool MsgAlertFmt(bool yes_no, MsgType style, const S& format, const Args&... args)
{
  static_assert(NumFields == sizeof...(args),
                "Unexpected number of replacement fields in format string; did you pass too few or "
                "too many arguments?");
  return MsgAlertFmtImpl(yes_no, style, format, fmt::make_args_checked<Args...>(format, args...));
}

void SetEnableAlert(bool enable);
void SetAbortOnPanicAlert(bool should_abort);

// Like fmt::format, except the string becomes translatable
template <typename... Args>
std::string FmtFormatT(const char* string, Args&&... args)
{
  return fmt::format(Common::GetStringT(string), std::forward<Args>(args)...);
}
}  // namespace Common

// Deprecated variants of the alert macros. See the fmt variants down below.

#define SuccessAlert(format, ...)                                                                  \
  Common::MsgAlert(false, Common::MsgType::Information, format, ##__VA_ARGS__)

#define PanicAlert(format, ...)                                                                    \
  Common::MsgAlert(false, Common::MsgType::Warning, format, ##__VA_ARGS__)

#define PanicYesNo(format, ...)                                                                    \
  Common::MsgAlert(true, Common::MsgType::Warning, format, ##__VA_ARGS__)

#define AskYesNo(format, ...)                                                                      \
  Common::MsgAlert(true, Common::MsgType::Question, format, ##__VA_ARGS__)

#define CriticalAlert(format, ...)                                                                 \
  Common::MsgAlert(false, Common::MsgType::Critical, format, ##__VA_ARGS__)

// Use these macros (that do the same thing) if the message should be translated.
#define SuccessAlertT(format, ...)                                                                 \
  Common::MsgAlert(false, Common::MsgType::Information, format, ##__VA_ARGS__)

#define PanicAlertT(format, ...)                                                                   \
  Common::MsgAlert(false, Common::MsgType::Warning, format, ##__VA_ARGS__)

#define PanicYesNoT(format, ...)                                                                   \
  Common::MsgAlert(true, Common::MsgType::Warning, format, ##__VA_ARGS__)

#define AskYesNoT(format, ...)                                                                     \
  Common::MsgAlert(true, Common::MsgType::Question, format, ##__VA_ARGS__)

#define CriticalAlertT(format, ...)                                                                \
  Common::MsgAlert(false, Common::MsgType::Critical, format, ##__VA_ARGS__)

// Fmt-capable variants of the macros

#define GenericAlertFmt(yes_no, style, format, ...)                                                \
  [&] {                                                                                            \
    /* Use a macro-like name to avoid shadowing warnings */                                        \
    constexpr auto GENERIC_ALERT_FMT_N = Common::CountFmtReplacementFields(format);                \
    return Common::MsgAlertFmt<GENERIC_ALERT_FMT_N>(yes_no, style, FMT_STRING(format),             \
                                                    ##__VA_ARGS__);                                \
  }()

#define GenericAlertFmtT(yes_no, style, format, ...)                                               \
  [&] {                                                                                            \
    static_assert(!Common::ContainsNonPositionalArguments(format),                                 \
                  "Translatable strings must use positional arguments (e.g. {0} instead of {})");  \
    /* Use a macro-like name to avoid shadowing warnings */                                        \
    constexpr auto GENERIC_ALERT_FMT_N = Common::CountFmtReplacementFields(format);                \
    return Common::MsgAlertFmt<GENERIC_ALERT_FMT_N>(yes_no, style, FMT_STRING(format),             \
                                                    ##__VA_ARGS__);                                \
  }()

#define SuccessAlertFmt(format, ...)                                                               \
  GenericAlertFmt(false, Common::MsgType::Information, format, ##__VA_ARGS__)

#define PanicAlertFmt(format, ...)                                                                 \
  GenericAlertFmt(false, Common::MsgType::Warning, format, ##__VA_ARGS__)

#define PanicYesNoFmt(format, ...)                                                                 \
  GenericAlertFmt(true, Common::MsgType::Warning, format, ##__VA_ARGS__)

#define AskYesNoFmt(format, ...)                                                                   \
  GenericAlertFmt(true, Common::MsgType::Question, format, ##__VA_ARGS__)

#define CriticalAlertFmt(format, ...)                                                              \
  GenericAlertFmt(false, Common::MsgType::Critical, format, ##__VA_ARGS__)

// Use these macros (that do the same thing) if the message should be translated.
#define SuccessAlertFmtT(format, ...)                                                              \
  GenericAlertFmtT(false, Common::MsgType::Information, format, ##__VA_ARGS__)

#define PanicAlertFmtT(format, ...)                                                                \
  GenericAlertFmtT(false, Common::MsgType::Warning, format, ##__VA_ARGS__)

#define PanicYesNoFmtT(format, ...)                                                                \
  GenericAlertFmtT(true, Common::MsgType::Warning, format, ##__VA_ARGS__)

#define AskYesNoFmtT(format, ...)                                                                  \
  GenericAlertFmtT(true, Common::MsgType::Question, format, ##__VA_ARGS__)

#define CriticalAlertFmtT(format, ...)                                                             \
  GenericAlertFmtT(false, Common::MsgType::Critical, format, ##__VA_ARGS__)
