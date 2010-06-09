// stdafx.cpp : source file that includes just the standard includes
// DSP_InterC.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file

#include <stdarg.h>

void ErrorLog(const char* _fmt, ...)
{
	char Msg[512];
	va_list ap;

	va_start(ap, _fmt);
	vsprintf(Msg, _fmt, ap);
	va_end(ap);

	printf("Error");

}