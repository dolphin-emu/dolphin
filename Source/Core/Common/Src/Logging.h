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
#if 0

// THESE WILL BE REPLACED WITH A CLEANED UP LOGGING SYSTEM

#ifndef _LOGGING_H
#define _LOGGING_H

class IniFile;

// should be inside the LogManager ...
struct CDebugger_Log
{
	char m_szName[128];
	char m_szShortName[10];
	char m_szFilename[256];
	bool m_bLogToFile;
	bool m_bShowInLog;
	bool m_bEnable;
	FILE* m_pFile;

	void Init(void);


	// constructor
	CDebugger_Log(const char* _szShortName, const char* _szName);

	// destructor
	~CDebugger_Log(void);
};


#ifdef LOGGING

#define LOG(_t_, ...)                       LogManager::Log(LogManager::_t_, __VA_ARGS__);

#define _dbg_assert_(_t_, _a_)\
	if (!(_a_)){\
		char szError[512]; \
		sprintf_s(szError, 512, "Error localized at...\n\n  Line: %d\n  File: %s\n  Time: %s\n\nIgnore and continue?", __LINE__, __FILE__, __TIMESTAMP__); \
		LOG(_t_, szError); \
		if (MessageBox(NULL, szError, "*** Assertion Report ***", MB_YESNO | MB_ICONERROR) == IDNO){Crash();} \
	}
#define _dbg_assert_msg_(_t_, _a_, _fmt_, ...)\
	if (!(_a_)){\
		char szError[582], szError2[512]; \
		sprintf_s(szError2, 512, _fmt_, __VA_ARGS__); \
		sprintf_s(szError, 582, "Desc.: %s\n\nIgnore and continue?", szError2);	\
		LOG(_t_, szError); \
		if (MessageBox(NULL, szError, "*** Fatal Error ***", MB_YESNO | MB_ICONERROR) == IDNO){Crash();} \
	}

#else

#define LOG(_t_, ...)
#define _dbg_clear_()
#define _dbg_assert_(_t_, _a_) ;
#define _dbg_assert_msg_(_t_, _a_, _desc_, ...) ;
#define _dbg_update_() ;

#endif

#define _assert_msg_(_t_, _a_, _fmt_, ...)\
	if (!(_a_)){\
		char szError[582], szError2[512]; \
		sprintf_s(szError2, 512, _fmt_, __VA_ARGS__); \
		sprintf_s(szError, 582, "Desc.: %s\n\nIgnore and continue?", szError2);	\
		LOG(_t_, szError); \
		if (MessageBox(NULL, szError, "*** Fatal Error ***", MB_YESNO | MB_ICONERROR) == IDNO){Crash();} \
	}

#endif


#endif

#endif
