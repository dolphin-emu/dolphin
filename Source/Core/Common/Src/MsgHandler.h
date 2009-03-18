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

#ifndef MSGHANDLER_H
#define MSGHANDLER_H
// Message alerts
enum MSG_TYPE
{
	INFORMATION,
	QUESTION,
	WARNING,
};

typedef bool (*MsgAlertHandler)(const char* caption, const char* text, 
                                bool yes_no, int Style);
void RegisterMsgAlertHandler(MsgAlertHandler handler);
extern bool MsgAlert(const char* caption, bool yes_no, int Style, const char* format, ...);
void SetEnableAlert(bool enable);

#ifdef _WIN32
	#define SuccessAlert(format, ...) MsgAlert("Information", false, INFORMATION, format, __VA_ARGS__) 
	#define PanicAlert(format, ...) MsgAlert("Warning", false, WARNING, format, __VA_ARGS__) 
	#define PanicYesNo(format, ...) MsgAlert("Warning", true, WARNING, format, __VA_ARGS__) 
	#define AskYesNo(format, ...) MsgAlert("Question", true, QUESTION, format, __VA_ARGS__) 
#else
	#define SuccessAlert(format, ...) MsgAlert("Information", false, INFORMATION, format, ##__VA_ARGS__) 
	#define PanicAlert(format, ...) MsgAlert("Warning", false, WARNING, format, ##__VA_ARGS__) 
	#define PanicYesNo(format, ...) MsgAlert("Warning", true, WARNING, format, ##__VA_ARGS__) 
	#define AskYesNo(format, ...) MsgAlert("Question", true, QUESTION, format, ##__VA_ARGS__) 
#endif

#endif //MSGHANDLER
