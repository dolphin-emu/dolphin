// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include "Common/CommonTypes.h"

#ifndef _WIN32

// go to debugger mode
#define Crash()                                                                                    \
  {                                                                                                \
    __builtin_trap();                                                                              \
  }

#else  // WIN32
// Function Cross-Compatibility
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define unlink _unlink
#define vscprintf _vscprintf

// 64 bit offsets for Windows
#define fseeko _fseeki64
#define ftello _ftelli64
#define atoll _atoi64
#define stat _stat64
#define fstat _fstat64
#define fileno _fileno

extern "C" {
__declspec(dllimport) void __stdcall DebugBreak(void);
}
#define Crash()                                                                                    \
  {                                                                                                \
    DebugBreak();                                                                                  \
  }
#endif  // WIN32 ndef

namespace Common
{
// strerror_r wrapper to handle XSI and GNU versions.
const char* StrErrorWrapper(int error, char* buffer, std::size_t length);

// Wrapper function to get last strerror(errno) string.
// This function might change the error code.
std::string LastStrerrorString();

#ifdef _WIN32
// Wrapper function to get GetLastError() string.
// This function might change the error code.
std::string GetLastErrorString();

// Like GetLastErrorString() but if you have already queried the error code.
std::string GetWin32ErrorString(unsigned long error_code);

// Obtains a full path to the specified module.
std::optional<std::wstring> GetModuleName(void* hInstance);
#endif
}  // namespace Common
