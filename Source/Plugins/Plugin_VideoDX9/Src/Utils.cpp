#ifdef _WIN32
#include "W32Util/Misc.h"
#endif

#include "Common.h"
#include <assert.h>

int frameCount;
int lut3to8[8];
int lut4to8[16];
int lut5to8[32];
int lut6to8[64];
float lutu8tosfloat[256];
float lutu8toufloat[256];
float luts8tosfloat[256];

void InitLUTs()
{
	for (int i = 0; i < 8; i++)
		lut3to8[i] = (i * 255) / 7;
	for (int i = 0; i < 16; i++)
		lut4to8[i] = (i * 255) / 15;
	for (int i = 0; i < 32; i++)
		lut5to8[i] = (i * 255) / 31;	
	for (int i = 0; i < 64; i++)
		lut6to8[i] = (i * 255) / 63;
	for (int i = 0; i < 256; i++)
	{
		lutu8tosfloat[i] = (float)(i-128) / 127.0f;
		lutu8toufloat[i] = (float)(i) / 255.0f;
		luts8tosfloat[i] = ((float)(signed char)(char)i) / 127.0f;
	}
}

// Message handler for about box.
LRESULT CALLBACK AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
	switch (message)
	{
	case WM_INITDIALOG:
		W32Util::CenterWindow(hDlg);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
		{
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	}
	return FALSE;
}


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
static u64 luPerfFreq = 1000000;
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

void DVProfRegister(char* pname)
{
    if( !g_bWriteProfile )
        return;

#ifdef _WIN32
    if( luPerfFreq <= 1 ) {
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
    
    if( g_listCurTracking.size() > 0 ) {
        assert( g_listCurTracking.back().pprof != NULL );
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
    if( !g_bWriteProfile )
        return;
    if( g_listCurTracking.size() == 0 )
        return;

    DVPROFTRACK dvtrack = g_listCurTracking.back();

    assert( dvtrack.pdwTime != NULL && dvtrack.pprof != NULL );

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

        if( ittime->dwUserData )
            fprintf(f, "time: %d, user: 0x%8.8x", (u32)ittime->dwTime, ittime->dwUserData);
        else
            fprintf(f, "time: %d", (u32)ittime->dwTime);
        ++ittime;
    }

    // yes this is necessary, maps have problems with constructors on their type
    map<string, DVTIMEINFO>::iterator ittimes = mapAggregateTimes.find(p->pname);
    if( ittimes == mapAggregateTimes.end() ) {
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

    if( uex > utime ) {
        uex = 0;
    }

    ittimes->second.uExclusive += uex;
    return utime;
}

void DVProfWrite(char* pfilename, u32 frames)
{
    assert( pfilename != NULL );
    FILE* f = fopen(pfilename, "w");

    // pop back any unused
    mapAggregateTimes.clear();
    list<DVPROFSTRUCT>::iterator it = g_listProfilers.begin();

    while(it != g_listProfilers.end() ) {
        DVProfWriteStruct(f, &(*it), 0);
        ++it;
    }

    {
        map<string, DVTIMEINFO>::iterator it;
        fprintf(f, "\n\n-------------------------------------------------------------------\n\n");

        u64 uTotal[2] = {0};
        double fiTotalTime[2];

        for(it = mapAggregateTimes.begin(); it != mapAggregateTimes.end(); ++it) {
            uTotal[0] += it->second.uExclusive;
            uTotal[1] += it->second.uInclusive;
        }

        fprintf(f, "total times (%d): ex: %Lu ", frames, 1000000*uTotal[0]/(luPerfFreq*(u64)frames));
        fprintf(f, "inc: %Lu\n", 1000000 * uTotal[1]/(luPerfFreq*(u64)frames));

        fiTotalTime[0] = 1.0 / (double)uTotal[0];
        fiTotalTime[1] = 1.0 / (double)uTotal[1];

        // output the combined times
        for(it = mapAggregateTimes.begin(); it != mapAggregateTimes.end(); ++it) {
            fprintf(f, "%s - ex: %f inc: %f\n", it->first.c_str(), (float)((double)it->second.uExclusive * fiTotalTime[0]),
                (float)((double)it->second.uInclusive * fiTotalTime[1]));
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
