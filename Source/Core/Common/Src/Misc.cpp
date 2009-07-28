// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
const char *GetLastErrorMsg()
{
	// FIXME : not thread safe.
	// Caused by sloppy use in logging in FileUtil.cpp, primarily. What to do, what to do ...
	static char errStr[255] = {0};
#ifdef _WIN32
	DWORD dw = GetLastError();
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, dw,
				  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				  (LPTSTR) errStr, 254, NULL );
#else
	// Thread safe (XSI-compliant)
	strerror_r(errno, errStr, 255);
#endif
	return errStr;
}

#ifdef __APPLE__
// strlen with cropping after size n
size_t strnlen(const char *s, size_t n)
{
  const char *p = (const char *)memchr(s, 0, n);
  return(p ? p-s : n);
}
#endif

// strdup with cropping after size n
char* strndup(char const *s, size_t n)
{
	size_t len = strnlen(s, n);
	char *dup = (char *)malloc(len + 1);

	if (dup == NULL)
		return NULL;
	
	dup[len] = '\0';
	return (char *)memcpy(dup, s, len);
}
