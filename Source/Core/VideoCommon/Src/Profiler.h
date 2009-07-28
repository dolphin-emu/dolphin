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


// Zerofrog's Mini Internal Profiler

#ifndef _PROFILER_H
#define _PROFILER_H

#include <string>

#include "Common.h"

// #define DVPROFILE // comment out to disable profiling

extern int g_bWriteProfile; // global variable to enable/disable profiling (if DVPROFILE is defined)

// IMPORTANT: For every Register there must be an End. Use the below DVProfileFunc utility class for safety.
void DVProfRegister(const char* pname);			// first checks if this profiler exists in g_listProfilers
void DVProfEnd(u32 dwUserData);

void DVProfWrite(const char* pfilename, u32 frames = 0);
void DVProfGenReport(std::string *report);
void DVProfClear();						// clears all the profilers

#if defined(DVPROFILE) && defined(_WIN32)

#ifdef _MSC_VER

#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#endif

#define DVSTARTPROFILE() DVProfileFunc _pf(__PRETTY_FUNCTION__);
#define DVSTARTSUBPROFILE(name) DVProfileFunc _pf(name);

class DVProfileFunc
{
public:
    u32 dwUserData;
    DVProfileFunc(const char* pname) { DVProfRegister(pname); dwUserData = 0; }
    DVProfileFunc(const char* pname, u32 dwUserData) : dwUserData(dwUserData) { DVProfRegister(pname); }
    ~DVProfileFunc() { DVProfEnd(dwUserData); }
};

#else

#define DVSTARTPROFILE()
#define DVSTARTSUBPROFILE(name)

class DVProfileFunc
{
public:
    u32 dwUserData;
    __forceinline DVProfileFunc(const char* pname) {}
    __forceinline DVProfileFunc(const char* pname, u32 _dwUserData) { }
    ~DVProfileFunc() {}
};

#endif // DVPROFILE && WIN32

#endif // _PROFILER_H
