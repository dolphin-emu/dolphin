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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Force enable logging in the right modes. For some reason, something had changed
// so that debugfast no longer logged.
#ifdef _DEBUG
#undef LOGGING
#define LOGGING 1
#endif

#ifdef DEBUGFAST
#undef LOGGING
#define LOGGING 1
#endif

#include "Log.h"
#include "CommonTypes.h"
#include "MsgHandler.h"
#include "CommonPaths.h"
#include "CommonFuncs.h"

#ifdef _MSC_VER
#define __strdup _strdup
#define __getcwd _getcwd
#define __chdir _chdir
#else
#define __strdup strdup
#define __getcwd getcwd
#define __chdir chdir
#endif

// Darwin ABI requires that stack frames be aligned to 16-byte boundaries.
// This probably wouldn't break anyone either, but hey
#ifdef __APPLE__
	#define STACKALIGN __attribute__((__force_align_arg_pointer__))
#else
	#define STACKALIGN
#endif

#ifdef _WIN32
// Check MSC ver
	#if !defined _MSC_VER || _MSC_VER <= 1000
		#error needs at least version 1000 of MSC
	#endif

// Memory leak check defines
	#define _CRTDBG_MAP_ALLOC
	#define _CRTDBG_MAP_ALLOC_NEW
	#define CHECK_HEAP_INTEGRITY()

	#define POSIX 0
	#define NOMINMAX

// Alignment
	#define GC_ALIGNED16(x) __declspec(align(16)) x
	#define GC_ALIGNED32(x) __declspec(align(32)) x
	#define GC_ALIGNED64(x) __declspec(align(64)) x
	#define GC_ALIGNED16_DECL(x) __declspec(align(16)) x
	#define GC_ALIGNED64_DECL(x) __declspec(align(64)) x

// Since they are always around on windows
	#define HAVE_WIIUSE 1
	#define HAVE_WX 1
	#define HAVE_SFML 1
	#define HAVE_OPENAL 1

// it is VERY DANGEROUS to mix _SECURE_SCL=0 and _SECURE_SCL=1 compiled libraries.
// You will get bizarre crash bugs whenever you use STL.
	namespace
	{
		#ifndef _SECURE_SCL
			#error Please define _SECURE_SCL=0 in the project settings
		#else
			CompileTimeAssert<_SECURE_SCL==0> EnsureNoSecureSCL;
		#endif
	}

// Debug definitions
	#if defined(_DEBUG)
		#include <crtdbg.h>
		#undef CHECK_HEAP_INTEGRITY
		#define CHECK_HEAP_INTEGRITY() {if (!_CrtCheckMemory()) PanicAlert("memory corruption detected. see log.");}
	#endif // end DEBUG/FAST

#else // Not windows

#include "Config.h" // Scons defines
// General defines
	#define POSIX 1
	#define MAX_PATH 260

// Windows compatibility
	#define __forceinline inline __attribute__((always_inline))

	#ifdef _LP64
		#define _M_X64 1
	#else
		#define _M_IX86 1
	#endif
// Alignment
	#define GC_ALIGNED16(x)  __attribute__((aligned(16))) x
	#define GC_ALIGNED32(x)  __attribute__((aligned(32))) x
	#define GC_ALIGNED64(x)  __attribute__((aligned(64))) x
	#define GC_ALIGNED16_DECL(x) __attribute__((aligned(16))) x
	#define GC_ALIGNED64_DECL(x) __attribute__((aligned(64))) x

#endif // WIN32

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#endif // _COMMON_H_
