// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

// Message alerts
enum class MsgType
{
  Information,
  Question,
  Warning,
  Critical
};

typedef bool (*MsgAlertHandler)(const char* caption, const char* text, bool yes_no, MsgType style);
typedef std::string (*StringTranslator)(const char* text);

void RegisterMsgAlertHandler(MsgAlertHandler handler);
void RegisterStringTranslator(StringTranslator translator);

std::string GetStringT(const char* string);
bool MsgAlert(bool yes_no, MsgType style, const char* format, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 3, 4)))
#endif
    ;
void SetEnableAlert(bool enable);

#ifdef _WIN32
#define SuccessAlert(format, ...) MsgAlert(false, MsgType::Information, format, __VA_ARGS__)
#define PanicAlert(format, ...) MsgAlert(false, MsgType::Warning, format, __VA_ARGS__)
#define PanicYesNo(format, ...) MsgAlert(true, MsgType::Warning, format, __VA_ARGS__)
#define AskYesNo(format, ...) MsgAlert(true, MsgType::Question, format, __VA_ARGS__)
#define CriticalAlert(format, ...) MsgAlert(false, MsgType::Critical, format, __VA_ARGS__)
// Use these macros (that do the same thing) if the message should be translated.
#define SuccessAlertT(format, ...) MsgAlert(false, MsgType::Information, format, __VA_ARGS__)
#define PanicAlertT(format, ...) MsgAlert(false, MsgType::Warning, format, __VA_ARGS__)
#define PanicYesNoT(format, ...) MsgAlert(true, MsgType::Warning, format, __VA_ARGS__)
#define AskYesNoT(format, ...) MsgAlert(true, MsgType::Question, format, __VA_ARGS__)
#define CriticalAlertT(format, ...) MsgAlert(false, MsgType::Critical, format, __VA_ARGS__)

#else
#define SuccessAlert(format, ...) MsgAlert(false, MsgType::Information, format, ##__VA_ARGS__)
#define PanicAlert(format, ...) MsgAlert(false, MsgType::Warning, format, ##__VA_ARGS__)
#define PanicYesNo(format, ...) MsgAlert(true, MsgType::Warning, format, ##__VA_ARGS__)
#define AskYesNo(format, ...) MsgAlert(true, MsgType::Question, format, ##__VA_ARGS__)
#define CriticalAlert(format, ...) MsgAlert(false, MsgType::Critical, format, ##__VA_ARGS__)
// Use these macros (that do the same thing) if the message should be translated.
#define SuccessAlertT(format, ...) MsgAlert(false, MsgType::Information, format, ##__VA_ARGS__)
#define PanicAlertT(format, ...) MsgAlert(false, MsgType::Warning, format, ##__VA_ARGS__)
#define PanicYesNoT(format, ...) MsgAlert(true, MsgType::Warning, format, ##__VA_ARGS__)
#define AskYesNoT(format, ...) MsgAlert(true, MsgType::Question, format, ##__VA_ARGS__)
#define CriticalAlertT(format, ...) MsgAlert(false, MsgType::Critical, format, ##__VA_ARGS__)
#endif
