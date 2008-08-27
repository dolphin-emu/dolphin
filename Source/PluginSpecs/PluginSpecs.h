//__________________________________________________________________________________________________
// Common plugin spec, version #1.0 maintained by F|RES
//

#ifdef _WIN32
#define EXPORT	__declspec(dllexport)
#define CALL	__cdecl
#else
#define EXPORT	__attribute__ ((visibility("default")))
#define CALL
#endif

#ifndef _PLUGINS_H_INCLUDED__
#define _PLUGINS_H_INCLUDED__

#ifdef _WIN32

#include <windows.h>

#else

#ifndef TRUE
#define TRUE 1
#define FALSE 0 
#endif

#define __cdecl

// simulate something that looks like win32
// long term, kill these

// glxew defines BOOL and BYTE. evil.
#ifdef BOOL
#undef BOOL
#undef BYTE
#endif

#define BOOL unsigned int
#define BYTE unsigned char
#define WORD unsigned short
#define DWORD unsigned int
#define HWND  void*
#define HINSTANCE void*
#define INT int
#define CHAR char
#define TCHAR char
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// plugin types
#define PLUGIN_TYPE_VIDEO			1
#define PLUGIN_TYPE_DVD				2
#define PLUGIN_TYPE_PAD             3
#define PLUGIN_TYPE_AUDIO           4
#define PLUGIN_TYPE_COMPILER        5
#define PLUGIN_TYPE_DSP             6

typedef struct 
{
	WORD Version;        // Set to 0x0100
	WORD Type;           // Set to PLUGIN_TYPE_DVD
	char Name[100];      // Name of the DLL
} PLUGIN_INFO;

typedef struct
{
	HWND mainWindow;
	HWND displayWindow;
	HINSTANCE hInstance;
} OSData;

#if defined(__cplusplus)
}
#endif
#endif
