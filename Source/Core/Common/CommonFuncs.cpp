// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/CommonFuncs.h"

#include <cstddef>
#include <cstring>
#include <errno.h>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#define strerror_r(err, buf, len) strerror_s(buf, len, err)
#endif

namespace Common
{
constexpr size_t BUFFER_SIZE = 256;

// Wrapper function to get last strerror(errno) string.
// This function might change the error code.
std::string LastStrerrorString()
{
  char error_message[BUFFER_SIZE];

  // There are two variants of strerror_r. The XSI version stores the message to the passed-in
  // buffer and returns an int (0 on success). The GNU version returns a pointer to the message,
  // which might have been stored in the passed-in buffer or might be a static string.

  // We check defines in order to figure out variant is in use, and we store the returned value
  // to a variable so that we'll get a compile-time check that our assumption was correct.

#if (defined(__GLIBC__) || __ANDROID_API__ >= 23) &&                                               \
    (_GNU_SOURCE || (_POSIX_C_SOURCE < 200112L && _XOPEN_SOURCE < 600))
  const char* str = strerror_r(errno, error_message, BUFFER_SIZE);
  return std::string(str);
#else
  int error_code = strerror_r(errno, error_message, BUFFER_SIZE);
  return error_code == 0 ? std::string(error_message) : "";
#endif
}

#ifdef _WIN32
// Wrapper function to get GetLastError() string.
// This function might change the error code.
std::string GetLastErrorString()
{
  char error_message[BUFFER_SIZE];

  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(),
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error_message, BUFFER_SIZE, nullptr);
  return std::string(error_message);
}

// Obtains a full path to the specified module.
std::optional<std::wstring> GetModuleName(void* hInstance)
{
  DWORD max_size = 50;  // Start with space for 50 characters and grow if needed
  std::wstring name(max_size, L'\0');

  DWORD size;
  while ((size = GetModuleFileNameW(static_cast<HMODULE>(hInstance), name.data(), max_size)) ==
             max_size &&
         GetLastError() == ERROR_INSUFFICIENT_BUFFER)
  {
    max_size *= 2;
    name.resize(max_size);
  }

  if (size == 0)
  {
    return std::nullopt;
  }
  name.resize(size);
  return name;
}
#endif
}  // namespace Common
