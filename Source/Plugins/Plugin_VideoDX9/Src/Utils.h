#ifndef _UTILS_H
#define _UTILS_H

#include "Common.h"
#include "main.h"
#include "LookUpTables.h"

extern int frameCount;

LRESULT CALLBACK AboutProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

//#define RAM_MASK 0x1FFFFFF

inline u8 *Memory_GetPtr(u32 _uAddress)
{
    return g_VideoInitialize.pGetMemoryPointer(_uAddress);
}

inline u8 Memory_Read_U8(u32 _uAddress)
{    
	return *g_VideoInitialize.pGetMemoryPointer(_uAddress);
}

inline u16 Memory_Read_U16(u32 _uAddress)
{
    return _byteswap_ushort(*(u16*)g_VideoInitialize.pGetMemoryPointer(_uAddress));
//	return _byteswap_ushort(*(u16*)&g_pMemory[_uAddress & RAM_MASK]);
}

inline u32 Memory_Read_U32(u32 _uAddress)
{
    if (_uAddress == 0x020143a8)
    {
        int i =0;
    }
    return _byteswap_ulong(*(u32*)g_VideoInitialize.pGetMemoryPointer(_uAddress));
//	return _byteswap_ulong(*(u32*)&g_pMemory[_uAddress & RAM_MASK]);
}

inline float Memory_Read_Float(u32 _uAddress)
{
	u32 uTemp = Memory_Read_U32(_uAddress);
	return *(float*)&uTemp;
}

////
// profiling
///

extern int g_bWriteProfile; // global variable to enable/disable profiling (if DVPROFILE is defined)

// IMPORTANT: For every Register there must be an End
void DVProfRegister(char* pname);			// first checks if this profiler exists in g_listProfilers
void DVProfEnd(u32 dwUserData);
void DVProfWrite(char* pfilename, u32 frames = 0);
void DVProfClear();						// clears all the profilers

// #define DVPROFILE // comment out to disable profiling

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
	DVProfileFunc(char* pname) { DVProfRegister(pname); dwUserData = 0; }
	DVProfileFunc(char* pname, u32 dwUserData) : dwUserData(dwUserData) { DVProfRegister(pname); }
	~DVProfileFunc() { DVProfEnd(dwUserData); }
};

#else

#define DVSTARTPROFILE()

class DVProfileFunc
{
public:
	u32 dwUserData;
	__forceinline DVProfileFunc(char* pname) {}
	__forceinline DVProfileFunc(char* pname, u32 dwUserData) { }
	~DVProfileFunc() {}
};

#endif

#endif
