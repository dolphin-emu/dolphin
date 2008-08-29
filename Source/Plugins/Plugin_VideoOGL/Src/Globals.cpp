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


#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/notebook.h>
#include <wx/dialog.h>
#include <wx/aboutdlg.h>

#include "Globals.h"

#include "pluginspecs_video.h"
#include "main.h"

#include "IniFile.h"
#include <assert.h>
float MValueX, MValueY; // Since it can Stretch to fit Window, we need two different multiplication values//
int frameCount;

Config g_Config;
Statistics stats;

void Statistics::ResetFrame()
{
	memset(&thisFrame,0,sizeof(ThisFrame));
}

Config::Config()
{
    memset(this, 0, sizeof(Config));
}

void Config::Load()
{
    std::string temp;
    IniFile iniFile;
    iniFile.Load("gfx_opengl.ini");
    iniFile.Get("Hardware", "Adapter", &iAdapter, 0);
    iniFile.Get("Hardware", "WindowedRes", &temp, 0);
    strcpy(iWindowedRes, temp.c_str());
    iniFile.Get("Hardware", "FullscreenRes", &temp, 0);
    strcpy(iFSResolution, temp.c_str());
    iniFile.Get("Hardware", "Fullscreen", &bFullscreen, 0);
	iniFile.Get("Hardware", "RenderToMainframe", &renderToMainframe, 0);
    if (iAdapter == -1) 
        iAdapter = 0;

    iniFile.Get("Settings", "OverlayStats", &bOverlayStats, false);
    iniFile.Get("Settings", "Postprocess", &iPostprocessEffect, 0);
    iniFile.Get("Settings", "DLOptimize", &iCompileDLsLevel, 0);
    iniFile.Get("Settings", "DumpTextures", &bDumpTextures, 0);
    iniFile.Get("Settings", "ShowShaderErrors", &bShowShaderErrors, 0);
    iniFile.Get("Settings", "Multisample", &iMultisampleMode, 0);
    if(iMultisampleMode == 0)
        iMultisampleMode = 1;
	std::string s;
    iniFile.Get("Settings", "TexDumpPath", &s, 0);
    if( s.size() < sizeof(texDumpPath) )
        strcpy(texDumpPath, s.c_str());
    else {
        strncpy(texDumpPath, s.c_str(), sizeof(texDumpPath)-1);
        texDumpPath[sizeof(texDumpPath)-1] = 0;
    }

	iniFile.Get("Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
	iniFile.Get("Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);
    
    iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, 0);
    iniFile.Get("Enhancements", "ForceMaxAniso", &bForceMaxAniso, 0);
    iniFile.Get("Enhancements", "StretchToFit", &bStretchToFit, false);
    iniFile.Get("Enhancements", "ShowFPS", &bShowFPS, false);
}

void Config::Save()
{
    IniFile iniFile;
    iniFile.Load("gfx_opengl.ini");
    iniFile.Set("Hardware", "Adapter", iAdapter);
    iniFile.Set("Hardware", "WindowedRes", iWindowedRes);
    iniFile.Set("Hardware", "FullscreenRes", iFSResolution);
    iniFile.Set("Hardware", "Fullscreen", bFullscreen);
	iniFile.Set("Hardware", "RenderToMainframe", renderToMainframe);

    iniFile.Set("Settings", "OverlayStats", bOverlayStats);
    iniFile.Set("Settings", "Postprocess", iPostprocessEffect);
    iniFile.Set("Settings", "DLOptimize", iCompileDLsLevel);
    iniFile.Set("Settings", "DumpTextures", bDumpTextures);
    iniFile.Set("Settings", "ShowShaderErrors", bShowShaderErrors);
    iniFile.Set("Settings", "Multisample", iMultisampleMode);
    iniFile.Set("Settings", "TexDumpPath", texDumpPath);

	iniFile.Set("Settings", "TexFmtOverlayEnable", bTexFmtOverlayEnable);
	iniFile.Set("Settings", "TexFmtOverlayCenter", bTexFmtOverlayCenter);

    iniFile.Set("Enhancements", "ForceFiltering", bForceFiltering);
    iniFile.Set("Enhancements", "ForceMaxAniso", bForceMaxAniso);
    iniFile.Set("Enhancements", "StretchToFit", bStretchToFit);
    iniFile.Set("Enhancements", "ShowFPS", bShowFPS);
    iniFile.Save("gfx_opengl.ini");
}

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

struct TGA_HEADER
{
    u8  identsize;          // size of ID field that follows 18 u8 header (0 usually)
    u8  colourmaptype;      // type of colour map 0=none, 1=has palette
    u8  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

    s16 colourmapstart;     // first colour map entry in palette
    s16 colourmaplength;    // number of colours in palette
    u8  colourmapbits;      // number of bits per palette entry 15,16,24,32

    s16 xstart;             // image x origin
    s16 ystart;             // image y origin
    s16 width;              // image width in pixels
    s16 height;             // image height in pixels
    u8  bits;               // image bits per pixel 8,16,24,32
    u8  descriptor;         // image descriptor bits (vh flip bits)
    
    // pixel data follows header
    
#if defined(_MSC_VER)
};
#pragma pack(pop)
#else
} __attribute__((packed));
#endif

bool SaveTGA(const char* filename, int width, int height, void* pdata)
{
    TGA_HEADER hdr;
    FILE* f = fopen(filename, "wb");
    if (f == NULL)
        return false;

    _assert_( sizeof(TGA_HEADER) == 18 && sizeof(hdr) == 18 );
    
    memset(&hdr, 0, sizeof(hdr));
    hdr.imagetype = 2;
    hdr.bits = 32;
    hdr.width = width;
    hdr.height = height;
    hdr.descriptor |= 8|(1<<5); // 8bit alpha, flip vertical

    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(pdata, width*height*4, 1, f);
    fclose(f);
    return true;
}

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height)
{
    GL_REPORT_ERRORD();
	std::vector<u32> data(width*height);
    glBindTexture(textarget, tex);
    glGetTexImage(textarget, 0, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
    GLenum err;
    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR) {
        return false;
    }

    return SaveTGA(filename, width, height, &data[0]);
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


#ifdef _WIN32
// The one for Linux is in Linux/Linux.cpp
static HANDLE hConsole = NULL;
void OpenConsole() {
	COORD csize;
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo; 
	SMALL_RECT srect;

	if (hConsole) return;
	AllocConsole();
	SetConsoleTitle("Opengl Plugin Output");
	csize.X = 80;
	csize.Y = 1024;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), csize);

	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo);
	srect = csbiInfo.srWindow;
	srect.Right = srect.Left + 79;
	srect.Bottom = srect.Top + 44;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &srect);
	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
}

void CloseConsole() {
	if (hConsole == NULL) return;
	FreeConsole(); hConsole = NULL;
}

#endif


static FILE* pfLog = NULL;
void __Log(const char *fmt, ...)
{
    char* Msg = (char*)alloca(strlen(fmt)+512);
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( Msg, strlen(fmt)+512, fmt, ap );
    va_end( ap );

    g_VideoInitialize.pLog(Msg, FALSE);

    if( pfLog == NULL ) pfLog = fopen("Logs/oglgfx.txt", "w");

    if( pfLog != NULL )
        fwrite(Msg, strlen(Msg), 1, pfLog);
#ifdef _WIN32
    DWORD tmp;
    WriteConsole(hConsole, Msg, (DWORD)strlen(Msg), &tmp, 0);
#else
	//printf("%s", Msg);
#endif
}

void __Log(int type, const char *fmt, ...)
{
    char* Msg = (char*)alloca(strlen(fmt)+512);
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( Msg, strlen(fmt)+512, fmt, ap );
    va_end( ap );

    g_VideoInitialize.pLog(Msg, FALSE);

#ifdef _WIN32
    DWORD tmp;
    WriteConsole(hConsole, Msg, (DWORD)strlen(Msg), &tmp, 0);
#endif
}

void SysMessage(const char *fmt, ...) 
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (msg[strlen(msg)-1] == '\n') msg[strlen(msg)-1] = 0;
	wxMessageBox(wxString::FromAscii(msg));
}
