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


#include <stdio.h> // System

#include "Common.h" // Local
#include "StringUtil.h"

bool DefaultMsgHandler(const char* caption, const char* text, bool yes_no, int Style);
static MsgAlertHandler msg_handler = DefaultMsgHandler;
static bool AlertEnabled = true;

std::string DefaultStringTranslator(const char* text);
static StringTranslator str_translator = DefaultStringTranslator;

// Select which of these functions that are used for message boxes. If
// wxWidgets is enabled we will use wxMsgAlert() that is defined in Main.cpp
void RegisterMsgAlertHandler(MsgAlertHandler handler)
{
	msg_handler = handler;
}

// Select translation function.  For wxWidgets use wxStringTranslator in Main.cpp
void RegisterStringTranslator(StringTranslator translator)
{
	str_translator = translator;
}

// enable/disable the alert handler
void SetEnableAlert(bool enable)
{
	AlertEnabled = enable;
}

// This is the first stop for gui alerts where the log is updated and the
// correct window is shown
bool MsgAlert(bool yes_no, int Style, const char* format, ...)
{
	// Read message and write it to the log
	std::string caption;
	char buffer[2048];

	static std::string info_caption;
	static std::string warn_caption;
	static std::string ques_caption;
	static std::string crit_caption;

	if (!info_caption.length())
	{
		info_caption = str_translator(_trans("Information"));
		ques_caption = str_translator(_trans("Question"));
		warn_caption = str_translator(_trans("Warning"));
		crit_caption = str_translator(_trans("Critical"));
	}

	switch(Style)
	{
		case INFORMATION:
			caption = info_caption;
			break;
		case QUESTION:
			caption = ques_caption;
			break;
		case WARNING:
			caption = warn_caption;
			break;
		case CRITICAL:
			caption = crit_caption;
			break;
	}

	va_list args;
	va_start(args, format);
	CharArrayFromFormatV(buffer, sizeof(buffer)-1, str_translator(format).c_str(), args);
	va_end(args);

	ERROR_LOG(MASTER_LOG, "%s: %s", caption.c_str(), buffer);

	// Don't ignore questions, especially AskYesNo, PanicYesNo could be ignored
	if (msg_handler && (AlertEnabled || Style == QUESTION || Style == CRITICAL))
		return msg_handler(caption.c_str(), buffer, yes_no, Style);

	return true;
}

// Default non library dependent panic alert
bool DefaultMsgHandler(const char* caption, const char* text, bool yes_no, int Style)
{
#ifdef _WIN32
    int STYLE = MB_ICONINFORMATION;
    if (Style == QUESTION) STYLE = MB_ICONQUESTION;
    if (Style == WARNING) STYLE = MB_ICONWARNING;
    
    return IDYES == MessageBox(0, text, caption, STYLE | (yes_no ? MB_YESNO : MB_OK));
    
#else
    printf("%s\n", text);
    return true;
#endif
}

// Default (non) translator
std::string DefaultStringTranslator(const char* text)
{
	return text;
}

