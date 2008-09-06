// Copyright (C) 2003-2008 Dolphin Project.

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

#include <stdio.h>

#define LOGGING

#include "Common.h"
#include "StringUtil.h"

namespace
{
	static PanicAlertHandler panic_handler = 0;
}

void RegisterPanicAlertHandler(PanicAlertHandler handler)
{
	panic_handler = handler;
}


void PanicAlert(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	if (panic_handler)
	{
		std::string msg;
		StringFromFormatV(&msg, format, args);
		LOG(MASTER_LOG, "PANIC: %s", msg.c_str());
		panic_handler(msg.c_str(), false);
	}
	else
	{
#ifdef _WIN32
		std::string msg;
		StringFromFormatV(&msg, format, args);
		LOG(MASTER_LOG, "PANIC: %s", msg.c_str());
		MessageBox(0, msg.c_str(), "PANIC!", MB_ICONWARNING);
#elif __GNUC__
		//#error Do a messagebox!
		vprintf(format, args);
		printf("\n");
//        asm ("int $3") ;
#endif
	}

	va_end(args);
}


bool PanicYesNo(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	bool retval;
#ifdef _WIN32
	std::string msg;
	StringFromFormatV(&msg, format, args);
	LOG(MASTER_LOG, "PANIC: %s", msg.c_str());
	retval = IDYES == MessageBox(0, msg.c_str(), "PANIC! Continue?", MB_ICONQUESTION | MB_YESNO);
#elif __GNUC__
	//vprintf(format, args);
	return(true); //#error Do a messagebox!
#endif

	va_end(args);
	return(retval);
}


bool AskYesNo(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	bool retval;
#ifdef _WIN32
	std::string msg;
	StringFromFormatV(&msg, format, args);
	LOG(MASTER_LOG, "ASK: %s", msg.c_str());
	retval = IDYES == MessageBox(0, msg.c_str(), "Dolphin", MB_ICONQUESTION | MB_YESNO);
#elif __GNUC__
	//vprintf(format, args);
	return(true); //#error Do a messagebox!
#endif

	va_end(args);
	return(retval);
}
// Standard implementation of logging - simply print to standard output.
// Programs are welcome to override this.
/*
   void __Log(int logNumber, const char *text, ...)
   {
    va_list args;
    va_start(args, text);
    vprintf(text, args);
    va_end(args);
   }*/
