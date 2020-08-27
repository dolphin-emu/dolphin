// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/MsgHandler.h"

#include <cstdarg>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdio>
#include <fmt/format.h>
#endif

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace Common
{
namespace
{
// Default non library dependent panic alert
bool DefaultMsgHandler(const char* caption, const char* text, bool yes_no, MsgType style)
{
#ifdef _WIN32
  int window_style = MB_ICONINFORMATION;
  if (style == MsgType::Question)
    window_style = MB_ICONQUESTION;
  if (style == MsgType::Warning)
    window_style = MB_ICONWARNING;

  return IDYES == MessageBox(0, UTF8ToTStr(text).c_str(), UTF8ToTStr(caption).c_str(),
                             window_style | (yes_no ? MB_YESNO : MB_OK));
#else
  fmt::print(stderr, "{}\n", text);

  // Return no to any question (which will in general crash the emulator)
  return false;
#endif
}

// Default (non) translator
std::string DefaultStringTranslator(const char* text)
{
  return text;
}

MsgAlertHandler s_msg_handler = DefaultMsgHandler;
StringTranslator s_str_translator = DefaultStringTranslator;
bool s_alert_enabled = true;
}  // Anonymous namespace

// Select which of these functions that are used for message boxes. If
// Qt is enabled we will use QtMsgAlertHandler() that is defined in Main.cpp
void RegisterMsgAlertHandler(MsgAlertHandler handler)
{
  s_msg_handler = handler;
}

// Select translation function.
void RegisterStringTranslator(StringTranslator translator)
{
  s_str_translator = translator;
}

// enable/disable the alert handler
void SetEnableAlert(bool enable)
{
  s_alert_enabled = enable;
}

std::string GetStringT(const char* string)
{
  return s_str_translator(string);
}

// This is the first stop for gui alerts where the log is updated and the
// correct window is shown
bool MsgAlert(bool yes_no, MsgType style, const char* format, ...)
{
  // Read message and write it to the log
  const char* caption = "";
  char buffer[2048];

  static const std::string info_caption = s_str_translator(_trans("Information"));
  static const std::string warn_caption = s_str_translator(_trans("Question"));
  static const std::string ques_caption = s_str_translator(_trans("Warning"));
  static const std::string crit_caption = s_str_translator(_trans("Critical"));

  switch (style)
  {
  case MsgType::Information:
    caption = info_caption.c_str();
    break;
  case MsgType::Question:
    caption = ques_caption.c_str();
    break;
  case MsgType::Warning:
    caption = warn_caption.c_str();
    break;
  case MsgType::Critical:
    caption = crit_caption.c_str();
    break;
  }

  va_list args;
  va_start(args, format);
  CharArrayFromFormatV(buffer, sizeof(buffer) - 1, s_str_translator(format).c_str(), args);
  va_end(args);

  ERROR_LOG(MASTER_LOG, "%s: %s", caption, buffer);

  // Don't ignore questions, especially AskYesNo, PanicYesNo could be ignored
  if (s_msg_handler != nullptr &&
      (s_alert_enabled || style == MsgType::Question || style == MsgType::Critical))
  {
    return s_msg_handler(caption, buffer, yes_no, style);
  }

  return true;
}
}  // namespace Common
