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

// several global utilities not really tied to core emulation
#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/notebook.h>
#include <wx/dialog.h>
#include <wx/aboutdlg.h>

#define LOGGING

#include "Common.h"
#include "x64Emitter.h"

#ifdef _WIN32

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>

void OpenConsole();
void CloseConsole();

#define GLEW_STATIC

#include "GLew/glew.h"
#include "GLew/wglew.h"
#include "GLew/gl.h"
#include "GLew/glext.h"

#else // linux basic definitions

#define ARRAYSIZE(A) (sizeof(A)/sizeof((A)[0]))
#define I_NEED_OS2_H // HAXXOR
//#include <GL/glew.h>
#include <GL/glxew.h>

#ifdef MACOSX

#include <OpenGL/gl.h>

#else

#include <GL/gl.h>

#endif
//#include <GL/glx.h>
#define __inline inline

#include <sys/timeb.h>    // ftime(), struct timeb

inline unsigned long timeGetTime()
{
#ifdef _WIN32
    _timeb t;
    _ftime(&t);
#else
    timeb t;
    ftime(&t);
#endif

    return (unsigned long)(t.time*1000+t.millitm);
}

struct RECT
{
    int left, top;
    int right, bottom;
};

#endif // linux basic definitions

#include <Cg/cg.h>
#include <Cg/cgGL.h>

#ifndef GL_DEPTH24_STENCIL8_EXT // allows FBOs to support stencils
#define GL_DEPTH_STENCIL_EXT 0x84F9
#define GL_UNSIGNED_INT_24_8_EXT 0x84FA
#define GL_DEPTH24_STENCIL8_EXT 0x88F0
#define GL_TEXTURE_STENCIL_SIZE_EXT 0x88F1
#endif
extern float MValue;
// several macros
// PLEASE DO NOT USE THE FOLLOWING SAFE*
// They only encourage sloppy coding, and hide bugs. 
#define SAFE_RELEASE_CGPROG(x) { if( (x) != NULL ) { cgDestroyProgram(x); x = NULL; } }
#define SAFE_RELEASE_PROG(x) { if( (x) != 0 ) { glDeleteProgramsARB(1, &(x)); x = 0; } }
#define SAFE_RELEASE_TEX(x) { if( (x) != 0 ) { glDeleteTextures(1, &(x)); x = 0; } }
#define SAFE_RELEASE_BUF(x) { if( (x) != 0 ) { glDeleteBuffers(1, &(x)); x = 0; } }

#define FORIT(it, v) for(it = (v).begin(); it != (v).end(); ++(it))

#define ERROR_LOG __Log

#ifdef _DEBUG
#define INFO_LOG if( g_Config.iLog & 1 ) __Log
#define PRIM_LOG if( g_Config.iLog & 2 ) __Log
#define DEBUG_LOG __Log
#else
#define INFO_LOG(...)
#define PRIM_LOG(...)
#define DEBUG_LOG(...)
#endif

#define GL_REPORT_ERROR() { err = glGetError(); if( err != GL_NO_ERROR ) { ERROR_LOG("%s:%d: gl error 0x%x\n", __FILE__, (int)__LINE__, err); HandleGLError(); } }

#ifdef _DEBUG
#define GL_REPORT_ERRORD() { GLenum err = glGetError(); if( err != GL_NO_ERROR ) { ERROR_LOG("%s:%d: gl error 0x%x\n", __FILE__, (int)__LINE__, err); HandleGLError(); } }
#else
#define GL_REPORT_ERRORD()
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define BEGINSAVELOAD char *optr=ptr;
#define SAVELOAD(what,size) memcpy((void*)((save)?(void*)(ptr):(void*)(what)),(void*)((save)?(void*)(what):(void*)(ptr)),(size)); ptr+=(size);
#define ENDSAVELOAD return ptr-optr;

extern int frameCount;

#define CONF_LOG 1
#define CONF_PRIMLOG 2
#define CONF_SAVETEXTURES 4
#define CONF_SAVETARGETS 8

struct Config 
{
    Config();
    void Load();
    void Save();

    //video
	bool bFullscreen;
	bool renderToMainframe;
	char iFSResolution[16];
	char iWindowedRes[16];

    bool bForceFiltering;
    bool bForceMaxAniso;

    bool bTexFmtOverlayEnable;
	bool bTexFmtOverlayCenter;
	bool bOverlayStats;
    bool bDumpTextures;
	char texDumpPath[280];

	//currently unused:
	int aa; // anti-aliasing level
	int iLog; // CONF_ bits
	int iSaveTargetId;
	int iAdapter;
	int iMultisampleMode;
	char psProfile[16];
    char vsProfile[16];
	int iPostprocessEffect;
    int iCompileDLsLevel;
	bool bPreUpscale;
    int iPreUpscaleFilter;
	bool bTruform;
    int iTruformLevel;
	bool bOldCard;
	bool bWireFrame;
    bool bShowShaderErrors;
};

extern Config g_Config;

struct Statistics
{
    int numPrimitives;

    int numPixelShadersCreated;
    int numPixelShadersAlive;
    int numVertexShadersCreated;
    int numVertexShadersAlive;

    int numTexturesCreated;
    int numTexturesAlive;

    int numRenderTargetsCreated;
    int numRenderTargetsAlive;
    
    int numDListsCalled;
    int numDListsCreated;
    int numDListsAlive;

    int numJoins;

    struct ThisFrame
    {
        int numBPLoads;
        int numCPLoads;
        int numXFLoads;
        
        int numBPLoadsInDL;
        int numCPLoadsInDL;
        int numXFLoadsInDL;
        
        int numDLs;
        int numDLPrims;
        int numPrims;
        int numShaderChanges;
        int numBadCommands; //hope this always is zero ;)

		int numDListsCalled;
    };
    ThisFrame thisFrame;
    void ResetFrame();
};

extern Statistics stats;

#define STATISTICS

#ifdef STATISTICS
#define INCSTAT(a) (a)++;
#define ADDSTAT(a,b) (a)+=(b);
#define SETSTAT(a,x) (a)=(int)(x);
#else
#define INCSTAT(a) ;
#define ADDSTAT(a,b) ;
#define SETSTAT(a,x) ;
#endif

void DebugLog(const char* _fmt, ...);
void __Log(const char *format, ...);
void __Log(int type, const char *format, ...);
void SysMessage(const char *fmt, ...);
void HandleGLError();

void InitLUTs();
bool SaveJPEG(const char* filename, int image_width, int image_height, const void* pdata, int quality);
bool SaveTGA(const char* filename, int width, int height, void* pdata);
bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height);

#if defined(_MSC_VER) && !defined(__x86_64__) && !defined(_M_X64)
void * memcpy_amd(void *dest, const void *src, size_t n);
unsigned char memcmp_mmx(const void* src1, const void* src2, int cmpsize);
#define memcpy_gc memcpy_amd
#define memcmp_gc memcmp_mmx
#else
#define memcpy_gc memcpy
#define memcmp_gc memcmp
#endif

#include "main.h"

inline u8 *Memory_GetPtr(u32 _uAddress)
{
    return g_VideoInitialize.pGetMemoryPointer(_uAddress);//&g_pMemory[_uAddress & RAM_MASK];
}

inline u8 Memory_Read_U8(u32 _uAddress)
{
    return *(u8*)g_VideoInitialize.pGetMemoryPointer(_uAddress);//g_pMemory[_uAddress & RAM_MASK];
}

inline u16 Memory_Read_U16(u32 _uAddress)
{
    return Common::swap16(*(u16*)g_VideoInitialize.pGetMemoryPointer(_uAddress));
    //return _byteswap_ushort(*(u16*)&g_pMemory[_uAddress & RAM_MASK]);
}

inline u32 Memory_Read_U32(u32 _uAddress)
{
    return Common::swap32(*(u32*)g_VideoInitialize.pGetMemoryPointer(_uAddress));
    //return _byteswap_ulong(*(u32*)&g_pMemory[_uAddress & RAM_MASK]);
}

inline float Memory_Read_Float(u32 _uAddress)
{
    union {u32 i; float f;} temp;
	temp.i = Memory_Read_U32(_uAddress);
    return temp.f;
}

////
// profiling
///

extern int g_bWriteProfile; // global variable to enable/disable profiling (if DVPROFILE is defined)

// IMPORTANT: For every Reigster there must be an End
void DVProfRegister(const char* pname);			// first checks if this profiler exists in g_listProfilers
void DVProfEnd(u32 dwUserData);
void DVProfWrite(const char* pfilename, u32 frames = 0);
void DVProfClear();						// clears all the profilers

//#define DVPROFILE // comment out to disable profiling

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
    __forceinline DVProfileFunc(const char* pname, u32 dwUserData) { }
    ~DVProfileFunc() {}
};

#endif

#endif
