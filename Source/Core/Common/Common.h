// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef __has_feature
#define __has_feature(x) 0
#endif

// Git version number
extern const char *scm_desc_str;
extern const char *scm_branch_str;
extern const char *scm_rev_str;
extern const char *scm_rev_git_str;
extern const char *netplay_dolphin_ver;

// Force enable logging in the right modes. For some reason, something had changed
// so that debugfast no longer logged.
#if defined(_DEBUG) || defined(DEBUGFAST)
#undef LOGGING
#define LOGGING 1
#endif

#if defined(__GNUC__) || __clang__
// Disable "unused function" warnings for the ones manually marked as such.
#define UNUSED __attribute__((unused))
#else
// Not sure MSVC even checks this...
#define UNUSED
#endif

#if defined(__GNUC__) || __clang__
	#define EXPECT(x, y) __builtin_expect(x, y)
	#define LIKELY(x)    __builtin_expect(!!(x), 1)
	#define UNLIKELY(x)  __builtin_expect(!!(x), 0)
	// Careful, wrong assumptions result in undefined behavior!
	#define UNREACHABLE  __builtin_unreachable()
	// Careful, wrong assumptions result in undefined behavior!
	#define ASSUME(x)    do { if (!x) __builtin_unreachable(); } while (0)
#else
	#define EXPECT(x, y) (x)
	#define LIKELY(x)    (x)
	#define UNLIKELY(x)  (x)
	// Careful, wrong assumptions result in undefined behavior!
	#define UNREACHABLE  ASSUME(0)
	#if defined(_MSC_VER)
		// Careful, wrong assumptions result in undefined behavior!
		#define ASSUME(x) __assume(x)
	#else
		#define ASSUME(x) do { void(x); } while (0)
	#endif
#endif

// An inheritable class to disallow the copy constructor and operator= functions
class NonCopyable
{
protected:
	constexpr NonCopyable() = default;
	~NonCopyable() = default;

private:
	NonCopyable(NonCopyable&) = delete;
	NonCopyable& operator=(NonCopyable&) = delete;
};

#if defined _WIN32

// Memory leak checks
	#define CHECK_HEAP_INTEGRITY()

// Since they are always around on Windows
	#define HAVE_WX 1
	#define HAVE_OPENAL 1

	#define HAVE_PORTAUDIO 1

// Debug definitions
	#if defined(_DEBUG)
		#include <crtdbg.h>
		#undef CHECK_HEAP_INTEGRITY
		#define CHECK_HEAP_INTEGRITY() {if (!_CrtCheckMemory()) PanicAlert("memory corruption detected. see log.");}
		// If you want to see how much a pain in the ass singletons are, for example:
		// {614} normal block at 0x030C5310, 188 bytes long.
		// Data: <Master Log      > 4D 61 73 74 65 72 20 4C 6F 67 00 00 00 00 00 00
		struct CrtDebugBreak { CrtDebugBreak(int spot) { _CrtSetBreakAlloc(spot); } };
		//CrtDebugBreak breakAt(614);
	#endif // end DEBUG/FAST

#endif

// Windows compatibility
#ifndef _WIN32
#include <limits.h>
#define MAX_PATH PATH_MAX

#define __forceinline inline __attribute__((always_inline))
#endif

#ifdef _MSC_VER
#define __strdup _strdup
#define __getcwd _getcwd
#define __chdir _chdir
#else
#define __strdup strdup
#define __getcwd getcwd
#define __chdir chdir
#endif

// Dummy macro for marking translatable strings that can not be immediately translated.
// wxWidgets does not have a true dummy macro for this.
#define _trans(a) a

// Host communication.
enum HOST_COMM
{
	// Begin at 10 in case there is already messages with wParam = 0, 1, 2 and so on
	WM_USER_STOP = 10,
	WM_USER_CREATE,
	WM_USER_SETCURSOR,
};

// Used for notification on emulation state
enum EMUSTATE_CHANGE
{
	EMUSTATE_CHANGE_PLAY = 1,
	EMUSTATE_CHANGE_PAUSE,
	EMUSTATE_CHANGE_STOP
};

#include "Common/CommonTypes.h" // IWYU pragma: export
#include "Common/CommonFuncs.h" // IWYU pragma: export // NOLINT
#include "Common/MsgHandler.h" // IWYU pragma: export
#include "Common/Logging/Log.h" // IWYU pragma: export
