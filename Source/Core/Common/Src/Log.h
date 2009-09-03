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

#ifndef _LOG_H_
#define _LOG_H_

#define	NOTICE_LEVEL  1  // VERY important information that is NOT errors. Like startup and OSReports.
#define	ERROR_LEVEL   2  // Critical errors 
#define	WARNING_LEVEL 3  // Something is suspicious.
#define	INFO_LEVEL    4  // General information.
#define	DEBUG_LEVEL   5  // Detailed debugging - might make things slow.

namespace LogTypes
{

enum LOG_TYPE {
	ACTIONREPLAY,
	AUDIO,
	AUDIO_INTERFACE,
	BOOT,
	COMMANDPROCESSOR,
	COMMON,
	CONSOLE,
	DISCIO,
	FILEMON,
	DSPHLE,
	DSPLLE,
	DSP_MAIL,
	DSPINTERFACE,
	DVDINTERFACE,
	DYNA_REC,
	EXPANSIONINTERFACE,
	POWERPC,
	GPFIFO,
	HLE,
	MASTER_LOG,
	MEMMAP,
	MEMCARD_MANAGER,
	OSREPORT,
	PAD, 
	PERIPHERALINTERFACE,
	PIXELENGINE,
	SERIALINTERFACE,
	SP1,
	STREAMINGINTERFACE,
	VIDEO,
	VIDEOINTERFACE,
	WII_IOB,
	WII_IPC,
	WII_IPC_DVD,
	WII_IPC_ES,
	WII_IPC_FILEIO,
	WII_IPC_HLE,
	WII_IPC_NET,
	WII_IPC_SD,
	WII_IPC_STM,
	WII_IPC_WIIMOTE,
	WIIMOTE,
	NETPLAY,

	NUMBER_OF_LOGS // Must be last
};

// FIXME: should this be removed?
enum LOG_LEVELS {
	LNOTICE = NOTICE_LEVEL,
	LERROR = ERROR_LEVEL,
	LWARNING = WARNING_LEVEL,
	LINFO = INFO_LEVEL,
	LDEBUG = DEBUG_LEVEL,
};

}  // namespace


/* 
   FIXME: 
   - Debug_run() - run only in debug time
*/
#if defined LOGGING || defined _DEBUG || defined DEBUGFAST
#define MAX_LOGLEVEL DEBUG_LEVEL
#else
#ifndef MAX_LOGLEVEL
#define MAX_LOGLEVEL WARNING_LEVEL
#endif // loglevel
#endif // logging

#define ERROR_LOG(...) {}
#define WARN_LOG(...) {}
#define NOTICE_LOG(...) {}
#define INFO_LOG(...) {}
#define DEBUG_LOG(...) {}

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char *fmt, ...);

#ifdef GEKKO
#define GENERIC_LOG(t, v, ...)
#else

// Let the compiler optimize this out
#define GENERIC_LOG(t, v, ...) {if (v <= MAX_LOGLEVEL) {GenericLog(v, t,  __VA_ARGS__);}}
#endif

#if MAX_LOGLEVEL >= ERROR_LEVEL
#undef ERROR_LOG
#define ERROR_LOG(t,...) {GENERIC_LOG(LogTypes::t, LogTypes::LERROR, __VA_ARGS__)}
#endif // loglevel ERROR+

#if MAX_LOGLEVEL >= WARNING_LEVEL
#undef WARN_LOG
#define WARN_LOG(t,...)  {GENERIC_LOG(LogTypes::t, LogTypes::LWARNING, __VA_ARGS__)}
#endif // loglevel WARNING+

#if MAX_LOGLEVEL >= NOTICE_LEVEL
#undef NOTICE_LOG
#define NOTICE_LOG(t,...)  {GENERIC_LOG(LogTypes::t, LogTypes::LNOTICE, __VA_ARGS__)}
#endif // loglevel NOTICE+
#if MAX_LOGLEVEL >= INFO_LEVEL
#undef INFO_LOG
#define INFO_LOG(t,...)  {GENERIC_LOG(LogTypes::t, LogTypes::LINFO, __VA_ARGS__)}
#endif // loglevel INFO+

#if MAX_LOGLEVEL >= DEBUG_LEVEL
#undef DEBUG_LOG
#define DEBUG_LOG(t,...) {GENERIC_LOG(LogTypes::t, LogTypes::LDEBUG, __VA_ARGS__)}
#endif // loglevel DEBUG+

#if MAX_LOGLEVEL >= DEBUG_LEVEL
#define _dbg_assert_(_t_, _a_) \
	if (!(_a_)) {\
		ERROR_LOG(_t_, "Error...\n\n  Line: %d\n  File: %s\n  Time: %s\n\nIgnore and continue?", \
					   __LINE__, __FILE__, __TIME__); \
		if (!PanicYesNo("*** Assertion (see log)***\n")) {Crash();} \
	}
#define _dbg_assert_msg_(_t_, _a_, ...)\
	if (!(_a_)) {\
		ERROR_LOG(_t_, __VA_ARGS__); \
		if (!PanicYesNo(__VA_ARGS__)) {Crash();} \
	}
#define _dbg_update_() Host_UpdateLogDisplay();

#else // not debug
#define _dbg_clear_()
#define _dbg_update_() ;

#ifndef _dbg_assert_
#define _dbg_assert_(_t_, _a_) ;
#define _dbg_assert_msg_(_t_, _a_, _desc_, ...) ;
#endif // dbg_assert
#endif // MAX_LOGLEVEL DEBUG

#define _assert_(_a_) _dbg_assert_(MASTER_LOG, _a_)

#ifndef GEKKO
#ifdef _WIN32
#define _assert_msg_(_t_, _a_, _fmt_, ...)		\
	if (!(_a_)) {\
		if (!PanicYesNo(_fmt_, __VA_ARGS__)) {Crash();} \
	}
#else // not win32
#define _assert_msg_(_t_, _a_, _fmt_, ...)		\
	if (!(_a_)) {\
		if (!PanicYesNo(_fmt_, ##__VA_ARGS__)) {Crash();} \
	}
#endif // WIN32
#else // GEKKO
#define _assert_msg_(_t_, _a_, _fmt_, ...)
#endif

#endif // _LOG_H_
