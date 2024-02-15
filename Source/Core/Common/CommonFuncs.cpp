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

#include "Common/StringUtil.h"
#endif

namespace Common
{
constexpr size_t BUFFER_SIZE = 256;

// There are two variants of strerror_r. The XSI version stores the message to the passed-in
// buffer and returns an int (0 on success). The GNU version returns a pointer to the message,
// which might have been stored in the passed-in buffer or might be a static string.
//
// This function might change the errno value.
//
// References:
// https://www.gnu.org/software/gnulib/manual/html_node/strerror_005fr.html
// https://pubs.opengroup.org/onlinepubs/9699919799/functions/strerror_r.html
// https://refspecs.linuxbase.org/LSB_5.0.0/LSB-Core-generic/LSB-Core-generic/baselib-strerror-r.html
const char* StrErrorWrapper(int error, char* buffer, std::size_t length)
{
  // We check defines in order to figure out which variant is in use.
#if (defined(__GLIBC__) || __ANDROID_API__ >= 23) &&                                               \
    (_GNU_SOURCE || (_POSIX_C_SOURCE < 200112L && _XOPEN_SOURCE < 600))
  return strerror_r(error, buffer, length);
#else
  const int error_code = strerror_r(error, buffer, length);
  return error_code == 0 ? buffer : "";
#endif
}

// Wrapper function to get last strerror(errno) string.
// This function might change the error code.
std::string LastStrerrorString()
{
  char error_message[BUFFER_SIZE];

  return StrErrorWrapper(errno, error_message, BUFFER_SIZE);
}

#ifdef _WIN32
// Wrapper function to get GetLastError() string.
// This function might change the error code.
std::string GetLastErrorString()
{
  return GetWin32ErrorString(GetLastError());
}

// Like GetLastErrorString() but if you have already queried the error code.
std::string GetWin32ErrorString(DWORD error_code)
{
  wchar_t error_message[BUFFER_SIZE];

  FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error_code,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error_message, BUFFER_SIZE, nullptr);
  return WStringToUTF8(error_message);
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
