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

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include "pluginspecs_dsp.h"
#include <stdio.h>

#define WITH_DSP_ON_THREAD 1
#define DUMP_DSP_IMEM   0

extern DSPInitialize g_dspInitialize;
void DebugLog(const char* _fmt, ...);
void ErrorLog(const char* _fmt, ...);
void DSP_DebugBreak();


#ifndef _dbg_assert_
	#if defined(_DEBUG) || defined(DEBUGFAST)

		#undef _dbg_assert_
		#undef _dbg_assert_msg_
		#define _dbg_assert_(_a_)                   if (!(_a_)){DebugBreak();}
		#define _dbg_assert_msg_(_a_, _desc_, ...)\
			if (!(_a_)){\
				if (MessageBox(NULL, _desc_, "*** Fatal Error ***", MB_YESNO | MB_ICONERROR) == IDNO){DebugBreak();}}

	#else

	#define _dbg_assert_(_a_);
	#define _dbg_assert_msg_(_a_, _desc_, ...);

	#endif
#endif

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;
typedef unsigned int uint;

typedef signed char sint8;
typedef signed short sint16;
typedef signed int sint32;
typedef signed long long sint64;

typedef const uint32 cuint32;

#endif

