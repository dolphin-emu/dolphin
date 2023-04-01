// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <errno.h>

#include "Common/CommonFuncs.h"

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
std::string GetLastErrorMsg()
{
	const size_t buff_size = 256;
	char err_str[buff_size];

#ifdef _WIN32
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		err_str, buff_size, nullptr);
#else
	// Thread safe (XSI-compliant)
	if (strerror_r(errno, err_str, buff_size))
		return "";
#endif

	return std::string(err_str);
}
