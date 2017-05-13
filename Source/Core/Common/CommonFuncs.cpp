// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <errno.h>

#include "Common/CommonFuncs.h"

#ifdef _WIN32
#include <windows.h>
#endif

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
std::string GetLastErrorMsg()
{
  const size_t buff_size = 256;
  char err_str[buff_size];

#ifdef _WIN32
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(),
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err_str, buff_size, nullptr);
#else
  // We assume that the XSI-compliant version of strerror_r (returns int) is used
  // rather than the GNU version (returns char*). The returned value is stored to
  // an int variable to get a compile-time check that the return type is not char*.
  const int result = strerror_r(errno, err_str, buff_size);
  if (result != 0)
    return "";
#endif

  return std::string(err_str);
}
