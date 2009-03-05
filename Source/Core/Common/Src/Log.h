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

#ifndef _LOG_H
#define _LOG_H

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
	DSPHLE,
	DSPINTERFACE,
	DVDINTERFACE,
	DYNA_REC,
	EXPANSIONINTERFACE,
	GEKKO,
	GPFIFO,
	HLE,
	MASTER_LOG,
	MEMMAP,
	OSREPORT,
	PERIPHERALINTERFACE,
	PIXELENGINE,
	SERIALINTERFACE,
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
	WII_IPC_WIIMOTE,
	
	NUMBER_OF_LOGS // Must be last
};

enum LOG_LEVELS {
	LERROR = 1,   // Bad errors - that still don't deserve a PanicAlert.
	LWARNING,     // Something is suspicious.
	LINFO,        // General information.
	LDEBUG,      // Strictly for detailed debugging - might make things slow.
};

}  // namespace


/* 
   FIXME: 
   - Debug_run() - run only in debug time
   - Compile the log functions according to LOGLEVEL 
*/
#ifdef LOGGING
#define LOGLEVEL 4 //LogTypes::LDEBUG
#else
#ifndef LOGLEVEL
#define LOGLEVEL 2 //LogTypes::LWARNING
#endif // loglevel
#endif // logging

#define ERROR_LOG(...) {}
#define WARN_LOG(...) {}
#define INFO_LOG(...) {}
#define DEBUG_LOG(...) {}

extern void __Log(int logNumber, const char* text, ...);

// Let the compiler optimize this out
#define GENERIC_LOG(t,v, ...) {if (v <= LOGLEVEL) __Log(t + (v)*100, __VA_ARGS__);}

#if LOGLEVEL >= 1 //LogTypes::LERROR
#undef ERROR_LOG
#define ERROR_LOG(t,...) {GENERIC_LOG(LogTypes::t, LogTypes::LERROR, __VA_ARGS__)}
#endif // loglevel LERROR+

#if LOGLEVEL >= 2 //LogTypes::LWARNING
#undef WARN_LOG
#define WARN_LOG(t,...)  {GENERIC_LOG(LogTypes::t, LogTypes::LWARNING, __VA_ARGS__)}
#endif // loglevel LWARNING+

#if LOGLEVEL >= 3 //LogTypes::LINFO
#undef INFO_LOG
#define INFO_LOG(t,...)  {GENERIC_LOG(LogTypes::t, LogTypes::LINFO, __VA_ARGS__)}
#endif // loglevel LINFO+

#if LOGLEVEL >= 4 //LogTypes::LDEBUG
#undef DEBUG_LOG
#define DEBUG_LOG(t,...) {GENERIC_LOG(LogTypes::t, LogTypes::LDEBUG, __VA_ARGS__)}
#endif // loglevel LDEBUG+

#if LOGLEVEL >= 4 //LogTypes::LDEBUG
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
#endif // LOGLEVEL LDEBUG

#define _assert_(_a_) _dbg_assert_(MASTER_LOG, _a_)
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

#endif // LOG_H
