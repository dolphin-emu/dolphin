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

#ifndef _COMMON_H
#define _COMMON_H

#ifdef _WIN32
#include "../../../PluginSpecs/CommonTypes.h"
#else
#include "CommonTypes.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Function Cross-Compatibility
#ifdef _WIN32
#define strcasecmp _stricmp
#define unlink _unlink
#else
#define _stricmp strcasecmp
#define _unlink unlink
#endif

#ifdef _WIN32
#define POSIX 0
#define NOMINMAX

#if _M_IX86
#define Crash() {__asm int 3}
#else
#if _MSC_VER > 1000
extern "C" {
	__declspec(dllimport) void __stdcall DebugBreak(void);
}
#define Crash() {DebugBreak();}
#else
#error fixme
#endif
#endif

#elif __GNUC__
#define POSIX 1
#define MAX_PATH 260
#define stricmp strcasecmp
#define Crash() {asm ("int $3");}
#ifdef _LP64
#define _M_X64 1
#else
#define _M_IX86 1
#endif
#endif

// Alignment

#if defined(_MSC_VER)

#define GC_ALIGNED16(x) __declspec(align(16)) x
#define GC_ALIGNED32(x) __declspec(align(32)) x
#define GC_ALIGNED64(x) __declspec(align(64)) x
#define GC_ALIGNED16_DECL(x) __declspec(align(16)) x
#define GC_ALIGNED64_DECL(x) __declspec(align(64)) x

#else

#define GC_ALIGNED16(x)  __attribute((aligned(16))) x
#define GC_ALIGNED32(x)  __attribute((aligned(16))) x
#define GC_ALIGNED64(x)  __attribute((aligned(64))) x
#define GC_ALIGNED16_DECL(x) __attribute((aligned(16))) x
#define GC_ALIGNED64_DECL(x) __attribute((aligned(64))) x

#endif

// Various Windows compatibility

#if !defined(_WIN32)

#ifdef __LINUX__
typedef union _LARGE_INTEGER
{
	long long QuadPart;
} LARGE_INTEGER;
#endif

#ifndef __forceinline
#define __forceinline inline
#endif

#ifndef _T
#define _T(a) a
#endif

#endif

#if defined (_M_IX86) && defined (_WIN32)
#define HWCALL __cdecl
#else
#define HWCALL
#endif

#undef min
#undef max

template<class T>
inline T min(const T& a, const T& b) {return(a > b ? b : a);}


template<class T>
inline T max(const T& a, const T& b) {return(a > b ? a : b);}

// Byte ordering

namespace Common
{
inline u8 swap8(u8 _data) {return(_data);}


#ifdef _WIN32
inline u16 swap16(u16 _data) {return(_byteswap_ushort(_data));}
inline u32 swap32(u32 _data) {return(_byteswap_ulong(_data));}
inline u64 swap64(u64 _data) {return(_byteswap_uint64(_data));}

#elif __linux__
}
#include <byteswap.h>
namespace Common
{
inline u16 swap16(u16 _data) {return(bswap_16(_data));}
inline u32 swap32(u32 _data) {return(bswap_32(_data));}
inline u64 swap64(u64 _data) {return(bswap_64(_data));}


#else
inline u16 swap16(u16 data) {return((data >> 8) | (data << 8));}
inline u32 swap32(u32 data) {return((swap16(data) << 16) | swap16(data >> 16));}
inline u64 swap64(u64 data) {return(((u64)swap32(data) << 32) | swap32(data >> 32));}
#endif

} // end of namespace Common

// Utility functions

void PanicAlert(const char* text, ...);
bool PanicYesNo(const char* text, ...);
bool AskYesNo(const char* text, ...);

extern void __Log(int logNumber, const char* text, ...);


// dummy class
class LogTypes
{
	public:
	enum LOG_TYPE
	{
		MASTER_LOG,
		BOOT,
		PIXELENGINE,
		COMMANDPROCESSOR,
		VIDEOINTERFACE,
		SERIALINTERFACE,
		PERIPHERALINTERFACE,
		MEMMAP,
		DSPINTERFACE,
		STREAMINGINTERFACE,
		DVDINTERFACE,
		GPFIFO,
		EXPANSIONINTERFACE,
		AUDIO_INTERFACE,
		GEKKO,
		HLE,
		DSPHLE,
		VIDEO,
		AUDIO,
		DYNA_REC,
		OSREPORT,
		CONSOLE,
		WII_IOB,
		WII_IPC,
		WII_IPC_HLE,
		NUMBER_OF_LOGS
	};
};

typedef bool (*PanicAlertHandler)(const char* text, bool yes_no);
void RegisterPanicAlertHandler(PanicAlertHandler handler);

void Host_UpdateLogDisplay();

// Logging macros
#ifdef LOGGING

#define LOG(t, ...) __Log(LogTypes::t, __VA_ARGS__);

#define _dbg_assert_(_t_, _a_) \
	if (!(_a_)){\
		LOG(_t_, "Error...\n\n  Line: %d\n  File: %s\n  Time: %s\n\nIgnore and continue?", \
				__LINE__, __FILE__, __TIME__); \
		if (!PanicYesNo("*** Assertion (see log)***\n")){Crash();} \
	}
#define _dbg_assert_msg_(_t_, _a_, ...)\
	if (!(_a_)){\
		LOG(_t_, __VA_ARGS__); \
		if (!PanicYesNo(__VA_ARGS__)){Crash();}	\
	}
#define _dbg_update_() Host_UpdateLogDisplay();

#else

#define LOG(_t_, ...)
#define _dbg_clear_()
#define _dbg_assert_(_t_, _a_) ;
#define _dbg_assert_msg_(_t_, _a_, _desc_, ...) ;
#define _dbg_update_() ;

#endif

#ifdef _WIN32
#define _assert_(_a_) _dbg_assert_(MASTER_LOG, _a_)
#define _assert_msg_(_t_, _a_, _fmt_, ...)\
	if (!(_a_)){\
		if (!PanicYesNo(_fmt_, __VA_ARGS__)){Crash();} \
	}
#else
#define _assert_(a)
#define _assert_msg_(...)
#endif
#endif
