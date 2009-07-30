// -----------------------------------------------------------------------------------------
//
// Written by Zoltan Csizmadia, zoltan_csizmadia@yahoo.com
// For companies(Austin,TX): If you would like to get my resume, send an email.
//
// The source is free, but if you want to use it, mention my name and e-mail address
//
// History:
//    1.0      Initial version                  Zoltan Csizmadia
//    1.1      WhineCube version                Masken
//    1.2      Dolphin version                  Masken
//
// ----------------------------------------------------------------------------------------

#ifndef _EXTENDEDTRACE_H_INCLUDED_
#define _EXTENDEDTRACE_H_INCLUDED_

#if defined(WIN32)

#include <windows.h>
#include <tchar.h>

#include <string>

#pragma comment( lib, "imagehlp.lib" )

#define EXTENDEDTRACEINITIALIZE( IniSymbolPath )	InitSymInfo( IniSymbolPath )
#define EXTENDEDTRACEUNINITIALIZE()			        UninitSymInfo()
#define STACKTRACE(file)							StackTrace( GetCurrentThread(), _T(""), file)
#define STACKTRACE2(file, eip, esp, ebp) StackTrace(GetCurrentThread(), _T(""), file, eip, esp, ebp)
// class File;

BOOL InitSymInfo( PCSTR );
BOOL UninitSymInfo();
void StackTrace( HANDLE, LPCTSTR, FILE *file);
void StackTrace( HANDLE, LPCTSTR, FILE *file, DWORD eip, DWORD esp, DWORD ebp);
void StackTrace( HANDLE hThread, wchar_t const* lpszMessage, FILE *file, DWORD eip, DWORD esp, DWORD ebp);

// functions by Masken
void etfprintf(FILE *file, const char *format, ...);
void etfprint(FILE *file, const std::string &text);
#define UEFBUFSIZE 2048
extern char g_uefbuf[UEFBUFSIZE];

#else	// not WIN32

#define EXTENDEDTRACEINITIALIZE( IniSymbolPath )   ((void)0)
#define EXTENDEDTRACEUNINITIALIZE()			         ((void)0)
#define STACKTRACE(file)						         	   ((void)0)
#define STACKTRACE2(file, eip, esp, ebp) ((void)0)

#endif	// WIN32

#endif	// _EXTENDEDTRACE_H_INCLUDED_
