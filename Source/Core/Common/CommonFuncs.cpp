// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <errno.h>
#include <type_traits>

#include "Common/CommonFuncs.h"

#ifdef _WIN32
#include <windows.h>
#define strerror_r(err, buf, len) strerror_s(buf, len, err)
#endif

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

#if defined(__GLIBC__) && (_GNU_SOURCE || (_POSIX_C_SOURCE < 200112L && _XOPEN_SOURCE < 600)) || defined(ANDROID)
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
#endif
