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

#include "Common.h"

#include "Profiler.h"


////////////////////
// Small profiler //
////////////////////
#include <list>
#include <string>
#include <map>
using namespace std;

int g_bWriteProfile=0;

#ifdef _WIN32

#if defined (_MSC_VER) && _MSC_VER >= 1400
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#endif

static u64 luPerfFreq=0;
inline u64 GET_PROFILE_TIME()
{
#if defined (_MSC_VER) && _MSC_VER >= 1400
    return __rdtsc();
#else
    LARGE_INTEGER lu;
    QueryPerformanceCounter(&lu);
    return lu.QuadPart;
#endif
}
#else
static u64 luPerfFreq=1000000;
#define GET_PROFILE_TIME() //GetCpuTick()
#endif


struct DVPROFSTRUCT;

struct DVPROFSTRUCT
{
    struct DATA
    {
        DATA(u64 time, u32 user = 0) : dwTime(time), dwUserData(user) {}
        DATA() : dwTime(0), dwUserData(0) {}
        
        u64 dwTime;
        u32 dwUserData;
    };

    ~DVPROFSTRUCT() {
        list<DVPROFSTRUCT*>::iterator it = listpChild.begin();
        while(it != listpChild.end() ) {
            delete *it; *it = NULL;
            ++it;
        }
    }

    list<DATA> listTimes;       // before DVProfEnd is called, contains the global time it started
                                // after DVProfEnd is called, contains the time it lasted
                                // the list contains all the tracked times 
    char pname[256];

    list<DVPROFSTRUCT*> listpChild;     // other profilers called during this profiler period
};

struct DVPROFTRACK
{
    u32 dwUserData;
    DVPROFSTRUCT::DATA* pdwTime;
    DVPROFSTRUCT* pprof;
};

list<DVPROFTRACK> g_listCurTracking;    // the current profiling functions, the back element is the
                                        // one that will first get popped off the list when DVProfEnd is called
                                        // the pointer is an element in DVPROFSTRUCT::listTimes
list<DVPROFSTRUCT> g_listProfilers;         // the current profilers, note that these are the parents
                                            // any profiler started during the time of another is held in
                                            // DVPROFSTRUCT::listpChild
list<DVPROFSTRUCT*> g_listAllProfilers;     // ignores the hierarchy, pointer to elements in g_listProfilers

void DVProfRegister(const char *pname)
{
    if (!g_bWriteProfile)
        return;

#ifdef _WIN32
    if (luPerfFreq <= 1) {
#if defined (_MSC_VER) && _MSC_VER >= 1400
        luPerfFreq = 1000000;
#else
        LARGE_INTEGER temp;
        QueryPerformanceFrequency(&temp);
        luPerfFreq = temp.QuadPart;
#endif
    }
#endif

    list<DVPROFSTRUCT*>::iterator it = g_listAllProfilers.begin();
    
//  while(it != g_listAllProfilers.end() ) {
//
//      if( _tcscmp(pname, (*it)->pname) == 0 ) {
//          (*it)->listTimes.push_back(timeGetTime());
//          DVPROFTRACK dvtrack;
//          dvtrack.pdwTime = &(*it)->listTimes.back();
//          dvtrack.pprof = *it;
//          g_listCurTracking.push_back(dvtrack);
//          return;
//      }
//
//      ++it;
//  }

    // else add in a new profiler to the appropriate parent profiler
    DVPROFSTRUCT* pprof = NULL;
    
    if (g_listCurTracking.size() > 0) {
        _assert_( g_listCurTracking.back().pprof != NULL );
        g_listCurTracking.back().pprof->listpChild.push_back(new DVPROFSTRUCT());
        pprof = g_listCurTracking.back().pprof->listpChild.back();
    }
    else {
        g_listProfilers.push_back(DVPROFSTRUCT());
        pprof = &g_listProfilers.back();
    }

    strncpy(pprof->pname, pname, 256);

    // setup the profiler for tracking
    pprof->listTimes.push_back(DVPROFSTRUCT::DATA(GET_PROFILE_TIME()));

    DVPROFTRACK dvtrack;
    dvtrack.pdwTime = &pprof->listTimes.back();
    dvtrack.pprof = pprof;
    dvtrack.dwUserData = 0;

    g_listCurTracking.push_back(dvtrack);

    // add to all profiler list
    g_listAllProfilers.push_back(pprof);
}

void DVProfEnd(u32 dwUserData)
{
    if (!g_bWriteProfile)
        return;
    if (g_listCurTracking.size() == 0)
        return;

    DVPROFTRACK dvtrack = g_listCurTracking.back();

    _assert_( dvtrack.pdwTime != NULL && dvtrack.pprof != NULL );

    dvtrack.pdwTime->dwTime = GET_PROFILE_TIME()- dvtrack.pdwTime->dwTime;
    dvtrack.pdwTime->dwUserData= dwUserData;

    g_listCurTracking.pop_back();
}

struct DVTIMEINFO
{
    DVTIMEINFO() : uInclusive(0), uExclusive(0) {}
    u64 uInclusive, uExclusive;
};

map<string, DVTIMEINFO> mapAggregateTimes;

u64 DVProfWriteStruct(FILE* f, DVPROFSTRUCT* p, int ident)
{
    fprintf(f, "%*s%s - ", ident, "", p->pname);

    list<DVPROFSTRUCT::DATA>::iterator ittime = p->listTimes.begin();

    u64 utime = 0;

    while(ittime != p->listTimes.end() ) {
        utime += ittime->dwTime;
        
        if (ittime->dwUserData) 
            fprintf(f, "time: %d, user: 0x%8.8x", (u32)ittime->dwTime, ittime->dwUserData);
        else
            fprintf(f, "time: %d", (u32)ittime->dwTime);
        ++ittime;
    }

    // yes this is necessary, maps have problems with constructors on their type
    map<string, DVTIMEINFO>::iterator ittimes = mapAggregateTimes.find(p->pname);
    if (ittimes == mapAggregateTimes.end()) {
        ittimes = mapAggregateTimes.insert(map<string, DVTIMEINFO>::value_type(p->pname, DVTIMEINFO())).first;
        ittimes->second.uExclusive = 0;
        ittimes->second.uInclusive = 0;
    }

    ittimes->second.uInclusive += utime;

    fprintf(f, "\n");

    list<DVPROFSTRUCT*>::iterator itprof = p->listpChild.begin();

    u64 uex = utime;
    while(itprof != p->listpChild.end() ) {

        uex -= DVProfWriteStruct(f, *itprof, ident+4);
        ++itprof;
    }

    if (uex > utime) {
        uex = 0;
    }

    ittimes->second.uExclusive += uex;
    return utime;
}

void DVProfWrite(const char* pfilename, u32 frames)
{
    _assert_( pfilename != NULL );
    FILE* f = fopen(pfilename, "w");

    // pop back any unused
    mapAggregateTimes.clear();
    list<DVPROFSTRUCT>::iterator it = g_listProfilers.begin();

    while(it != g_listProfilers.end() ) {
        DVProfWriteStruct(f, &(*it), 0);
        ++it;
    }

    {
        map<string, DVTIMEINFO>::iterator iter;
        fprintf(f, "\n\n-------------------------------------------------------------------\n\n");

        u64 uTotal[2] = {0};
        double fiTotalTime[2];

        for(iter = mapAggregateTimes.begin(); iter != mapAggregateTimes.end(); ++iter) {
            uTotal[0] += iter->second.uExclusive;
            uTotal[1] += iter->second.uInclusive;
        }

        fprintf(f, "total times (%d): ex: %Lu ", frames, 1000000*uTotal[0]/(luPerfFreq*(u64)frames));
        fprintf(f, "inc: %Lu\n", 1000000 * uTotal[1]/(luPerfFreq*(u64)frames));

        fiTotalTime[0] = 1.0 / (double)uTotal[0];
        fiTotalTime[1] = 1.0 / (double)uTotal[1];

        // output the combined times
        for(iter = mapAggregateTimes.begin(); iter != mapAggregateTimes.end(); ++iter) {
            fprintf(f, "%s - ex: %f inc: %f\n", iter->first.c_str(), (float)((double)iter->second.uExclusive * fiTotalTime[0]),
                (float)((double)iter->second.uInclusive * fiTotalTime[1]));
        }
    }


    fclose(f);
}

void DVProfClear()
{
    g_listCurTracking.clear();
    g_listProfilers.clear();
    g_listAllProfilers.clear();
}
