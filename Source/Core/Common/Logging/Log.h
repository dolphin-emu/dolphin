// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#define NOTICE_LEVEL  1  // VERY important information that is NOT errors. Like startup and OSReports.
#define ERROR_LEVEL   2  // Critical errors
#define WARNING_LEVEL 3  // Something is suspicious.
#define INFO_LEVEL    4  // General information.
#define DEBUG_LEVEL   5  // Detailed debugging - might make things slow.

namespace LogTypes
{

enum LOG_TYPE
{
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
	GDB_STUB,
	POWERPC,
	GPFIFO,
	OSHLE,
	MASTER_LOG,
	MEMMAP,
	MEMCARD_MANAGER,
	OSREPORT,
	PAD,
	PROCESSORINTERFACE,
	PIXELENGINE,
	SERIALINTERFACE,
	SP1,
	STREAMINGINTERFACE,
	VIDEO,
	VIDEOINTERFACE,
	WII_IPC,
	WII_IPC_DVD,
	WII_IPC_ES,
	WII_IPC_FILEIO,
	WII_IPC_HID,
	WII_IPC_HLE,
	WII_IPC_NET,
	WII_IPC_WC24,
	WII_IPC_SSL,
	WII_IPC_SD,
	WII_IPC_STM,
	WII_IPC_WIIMOTE,
	WIIMOTE,
	NETPLAY,

	NUMBER_OF_LOGS // Must be last
};

// FIXME: should this be removed?
enum LOG_LEVELS
{
	LNOTICE  = NOTICE_LEVEL,
	LERROR   = ERROR_LEVEL,
	LWARNING = WARNING_LEVEL,
	LINFO    = INFO_LEVEL,
	LDEBUG   = DEBUG_LEVEL,
};

static const char LOG_LEVEL_TO_CHAR[7] = "-NEWID";

}  // namespace

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type,
		const char *file, int line, const char *fmt, ...)
#ifdef __GNUC__
		__attribute__((format(printf, 5, 6)))
#endif
		;

#if defined LOGGING || defined _DEBUG || defined DEBUGFAST
#define MAX_LOGLEVEL DEBUG_LEVEL
#else
#ifndef MAX_LOGLEVEL
#define MAX_LOGLEVEL WARNING_LEVEL
#endif // loglevel
#endif // logging

#ifdef GEKKO
#define GENERIC_LOG(t, v, ...)
#else
// Let the compiler optimize this out
#define GENERIC_LOG(t, v, ...) { \
	if (v <= MAX_LOGLEVEL) \
		GenericLog(v, t, __FILE__, __LINE__, __VA_ARGS__); \
	}
#endif

#define ERROR_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LERROR, __VA_ARGS__) } while (0)
#define WARN_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LWARNING, __VA_ARGS__) } while (0)
#define NOTICE_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LNOTICE, __VA_ARGS__) } while (0)
#define INFO_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LINFO, __VA_ARGS__) } while (0)
#define DEBUG_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LDEBUG, __VA_ARGS__) } while (0)

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
#define _dbg_update_() ;

#ifndef _dbg_assert_
#define _dbg_assert_(_t_, _a_) {}
#define _dbg_assert_msg_(_t_, _a_, _desc_, ...) {}
#endif // dbg_assert
#endif // MAX_LOGLEVEL DEBUG

#define _assert_(_a_) _dbg_assert_(MASTER_LOG, _a_)

#ifndef GEKKO
#ifdef _WIN32
#define _assert_msg_(_t_, _a_, _fmt_, ...) \
	if (!(_a_)) {\
		if (!PanicYesNo(_fmt_, __VA_ARGS__)) {Crash();} \
	}
#else // not win32
#define _assert_msg_(_t_, _a_, _fmt_, ...) \
	if (!(_a_)) {\
		if (!PanicYesNo(_fmt_, ##__VA_ARGS__)) {Crash();} \
	}
#endif // WIN32
#else // GEKKO
#define _assert_msg_(_t_, _a_, _fmt_, ...)
#endif
