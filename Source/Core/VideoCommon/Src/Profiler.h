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


// Zerofrog's Mini Internal Profiler

#ifndef _PROFILER_H
#define _PROFILER_H

// #define DVPROFILE // comment out to disable profiling

extern int g_bWriteProfile; // global variable to enable/disable profiling (if DVPROFILE is defined)

// IMPORTANT: For every Register there must be an End
void DVProfRegister(const char* pname);			// first checks if this profiler exists in g_listProfilers
void DVProfEnd(u32 dwUserData);
void DVProfWrite(const char* pfilename, u32 frames = 0);
void DVProfClear();						// clears all the profi	lers

#if defined(DVPROFILE) && (defined(_WIN32)||defined(WIN32))

#ifdef _MSC_VER

#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

#endif

#define DVSTARTPROFILE() DVProfileFunc _pf(__PRETTY_FUNCTION__);

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

class DVProfileFunc
{
public:
    u32 dwUserData;
    __forceinline DVProfileFunc(const char* pname) {}
    __forceinline DVProfileFunc(const char* pname, u32 _dwUserData) { }
    ~DVProfileFunc() {}
};

#endif

#endif
