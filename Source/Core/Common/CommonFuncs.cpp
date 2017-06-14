// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// The code in GetErrorMessage can't handle some systems having the
// GNU version of strerror_r and other systems having the XSI version,
// so we undefine _GNU_SOURCE here in an attempt to always get the XSI version.
// We include cstring before all other headers in case cstring is included
// indirectly (without undefining _GNU_SOURCE) by some other header.
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#include <cstring>
#define _GNU_SOURCE
#else
#include <cstring>
#endif

#include <cstddef>

#include "Common/CommonFuncs.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#endif

int GetLastErrorCode()
{
#ifdef _WIN32
  return GetLastError();
#else
  return errno;
#endif
}

std::string GetErrorMessage(int error_code)
{
  const size_t buff_size = 256;
  char err_str[buff_size];

#ifdef _WIN32
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error_code,
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err_str, buff_size, nullptr);
#else
  // We assume that the XSI-compliant version of strerror_r (returns int) is used
  // rather than the GNU version (returns char*). The returned value is stored to
  // an int variable to get a compile-time check that the return type is not char*.
  const int result = strerror_r(error_code, err_str, buff_size);
  if (result != 0)
    return "";
#endif

  return std::string(err_str);
}

std::string GetLastErrorMessage()
{
  return GetErrorMessage(GetLastErrorCode());
}
