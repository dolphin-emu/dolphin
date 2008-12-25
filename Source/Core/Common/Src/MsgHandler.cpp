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

#include "Common.h"
#include "StringUtil.h"

bool DefaultMsgHandler(const char* caption, const char* text, bool yes_no);

static MsgAlertHandler msg_handler = DefaultMsgHandler;

void RegisterMsgAlertHandler(MsgAlertHandler handler)
{
	msg_handler = handler;
}

bool MsgAlert(const char* caption, bool yes_no, const char* format, ...)
{  
	char buffer[2048];
	va_list args;
	bool ret = false;

	va_start(args, format);
	CharArrayFromFormatV(buffer, 2048, format, args);

	LOG(MASTER_LOG, "%s: %s", caption, buffer);

	if (msg_handler)
	{
		ret = msg_handler(caption, buffer, yes_no);
	}

	va_end(args);
	return ret;
}

bool DefaultMsgHandler(const char* caption, const char* text, bool yes_no)
{
#ifdef _WIN32
   if (yes_no)
	   return IDYES == MessageBox(0, text, caption, 
	   MB_ICONQUESTION | MB_YESNO);
   else {
	   MessageBox(0, text, caption, MB_ICONWARNING);
	   return true;
   }
#else
	printf("%s\n", text);
	return true;
#endif
}

