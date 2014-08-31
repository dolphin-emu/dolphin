// --------------------------------------------------------------------------------------
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
// --------------------------------------------------------------------------------------

#if defined(WIN32)

#include <cstdio>
#include <windows.h>

#include "Common/ExtendedTrace.h"
#include "Common/StringUtil.h"

#include <tchar.h>
#include <ImageHlp.h>

#define BUFFERSIZE   0x200
#pragma warning(disable:4996)

// Unicode safe char* -> TCHAR* conversion
void PCSTR2LPTSTR( PCSTR lpszIn, LPTSTR lpszOut )
{
#if defined(UNICODE)||defined(_UNICODE)
	ULONG index = 0;
	PCSTR lpAct = lpszIn;

	for ( ; ; lpAct++ )
	{
		lpszOut[index++] = (TCHAR)(*lpAct);
		if ( *lpAct == 0 )
			break;
	}
#else
	// This is trivial :)
	strcpy( lpszOut, lpszIn );
#endif
}

// Let's figure out the path for the symbol files
// Search path= ".;%_NT_SYMBOL_PATH%;%_NT_ALTERNATE_SYMBOL_PATH%;%SYSTEMROOT%;%SYSTEMROOT%\System32;" + lpszIniPath
// Note: There is no size check for lpszSymbolPath!
static void InitSymbolPath( PSTR lpszSymbolPath, PCSTR lpszIniPath )
{
	CHAR lpszPath[BUFFERSIZE];

	// Creating the default path
	// ".;%_NT_SYMBOL_PATH%;%_NT_ALTERNATE_SYMBOL_PATH%;%SYSTEMROOT%;%SYSTEMROOT%\System32;"
	strcpy( lpszSymbolPath, "." );

	// environment variable _NT_SYMBOL_PATH
	if ( GetEnvironmentVariableA( "_NT_SYMBOL_PATH", lpszPath, BUFFERSIZE ) )
	{
		strcat( lpszSymbolPath, ";" );
		strcat( lpszSymbolPath, lpszPath );
	}

	// environment variable _NT_ALTERNATE_SYMBOL_PATH
	if ( GetEnvironmentVariableA( "_NT_ALTERNATE_SYMBOL_PATH", lpszPath, BUFFERSIZE ) )
	{
		strcat( lpszSymbolPath, ";" );
		strcat( lpszSymbolPath, lpszPath );
	}

	// environment variable SYSTEMROOT
	if ( GetEnvironmentVariableA( "SYSTEMROOT", lpszPath, BUFFERSIZE ) )
	{
		strcat( lpszSymbolPath, ";" );
		strcat( lpszSymbolPath, lpszPath );
		strcat( lpszSymbolPath, ";" );

		// SYSTEMROOT\System32
		strcat( lpszSymbolPath, lpszPath );
		strcat( lpszSymbolPath, "\\System32" );
	}

	// Add user defined path
	if ( lpszIniPath != nullptr )
		if ( lpszIniPath[0] != '\0' )
		{
			strcat( lpszSymbolPath, ";" );
			strcat( lpszSymbolPath, lpszIniPath );
		}
}

// Uninitialize the loaded symbol files
BOOL UninitSymInfo()
{
	return SymCleanup( GetCurrentProcess() );
}

// Initializes the symbol files
BOOL InitSymInfo( PCSTR lpszInitialSymbolPath )
{
	CHAR     lpszSymbolPath[BUFFERSIZE];
	DWORD    symOptions = SymGetOptions();

	symOptions |= SYMOPT_LOAD_LINES;
	symOptions &= ~SYMOPT_UNDNAME;
	SymSetOptions( symOptions );
	InitSymbolPath( lpszSymbolPath, lpszInitialSymbolPath );

	return SymInitialize( GetCurrentProcess(), lpszSymbolPath, TRUE);
}

// Get the module name from a given address
static BOOL GetModuleNameFromAddress( UINT address, LPTSTR lpszModule )
{
	BOOL              ret = FALSE;
	IMAGEHLP_MODULE   moduleInfo;

	::ZeroMemory( &moduleInfo, sizeof(moduleInfo) );
	moduleInfo.SizeOfStruct = sizeof(moduleInfo);

	if ( SymGetModuleInfo( GetCurrentProcess(), (DWORD)address, &moduleInfo ) )
	{
		// Got it!
		PCSTR2LPTSTR( moduleInfo.ModuleName, lpszModule );
		ret = TRUE;
	}
	else
		// Not found :(
		_tcscpy( lpszModule, _T("?") );

	return ret;
}

// Get function prototype and parameter info from ip address and stack address
static BOOL GetFunctionInfoFromAddresses( ULONG fnAddress, ULONG stackAddress, LPTSTR lpszSymbol )
{
	BOOL              ret = FALSE;
	DWORD             dwSymSize = 10000;
	TCHAR             lpszUnDSymbol[BUFFERSIZE]=_T("?");
	CHAR              lpszNonUnicodeUnDSymbol[BUFFERSIZE]="?";
	LPTSTR            lpszParamSep = nullptr;
	LPTSTR            lpszParsed = lpszUnDSymbol;
	PIMAGEHLP_SYMBOL  pSym = (PIMAGEHLP_SYMBOL)GlobalAlloc( GMEM_FIXED, dwSymSize );

	::ZeroMemory( pSym, dwSymSize );
	pSym->SizeOfStruct = dwSymSize;
	pSym->MaxNameLength = dwSymSize - sizeof(IMAGEHLP_SYMBOL);

	// Set the default to unknown
	_tcscpy( lpszSymbol, _T("?") );

	// Get symbol info for IP
	DWORD64           dwDisp = 0;
	if ( SymGetSymFromAddr( GetCurrentProcess(), (ULONG)fnAddress, (PDWORD64)&dwDisp, pSym ) )
	{
		// Make the symbol readable for humans
		UnDecorateSymbolName( pSym->Name, lpszNonUnicodeUnDSymbol, BUFFERSIZE,
			UNDNAME_COMPLETE |
			UNDNAME_NO_THISTYPE |
			UNDNAME_NO_SPECIAL_SYMS |
			UNDNAME_NO_MEMBER_TYPE |
			UNDNAME_NO_MS_KEYWORDS |
			UNDNAME_NO_ACCESS_SPECIFIERS );

		// Symbol information is ANSI string
		PCSTR2LPTSTR( lpszNonUnicodeUnDSymbol, lpszUnDSymbol );

		// I am just smarter than the symbol file :)
		if ( _tcscmp(lpszUnDSymbol, _T("_WinMain@16")) == 0 )
			_tcscpy(lpszUnDSymbol, _T("WinMain(HINSTANCE,HINSTANCE,LPCTSTR,int)"));
		else
			if ( _tcscmp(lpszUnDSymbol, _T("_main")) == 0 )
				_tcscpy(lpszUnDSymbol, _T("main(int,TCHAR * *)"));
			else
				if ( _tcscmp(lpszUnDSymbol, _T("_mainCRTStartup")) == 0 )
					_tcscpy(lpszUnDSymbol, _T("mainCRTStartup()"));
				else
					if ( _tcscmp(lpszUnDSymbol, _T("_wmain")) == 0 )
						_tcscpy(lpszUnDSymbol, _T("wmain(int,TCHAR * *,TCHAR * *)"));
					else
						if ( _tcscmp(lpszUnDSymbol, _T("_wmainCRTStartup")) == 0 )
							_tcscpy(lpszUnDSymbol, _T("wmainCRTStartup()"));

		lpszSymbol[0] = _T('\0');

		// Let's go through the stack, and modify the function prototype, and insert the actual
		// parameter values from the stack
		if ( _tcsstr( lpszUnDSymbol, _T("(void)") ) == nullptr && _tcsstr( lpszUnDSymbol, _T("()") ) == nullptr)
		{
			ULONG index = 0;
			for ( ; ; index++ )
			{
				lpszParamSep = _tcschr( lpszParsed, _T(',') );
				if ( lpszParamSep == nullptr )
					break;

				*lpszParamSep = _T('\0');

				_tcscat( lpszSymbol, lpszParsed );
				_stprintf( lpszSymbol + _tcslen(lpszSymbol), _T("=0x%08X,"), *((ULONG*)(stackAddress) + 2 + index) );

				lpszParsed = lpszParamSep + 1;
			}

			lpszParamSep = _tcschr( lpszParsed, _T(')') );
			if ( lpszParamSep != nullptr )
			{
				*lpszParamSep = _T('\0');

				_tcscat( lpszSymbol, lpszParsed );
				_stprintf( lpszSymbol + _tcslen(lpszSymbol), _T("=0x%08X)"), *((ULONG*)(stackAddress) + 2 + index) );

				lpszParsed = lpszParamSep + 1;
			}
		}

		_tcscat( lpszSymbol, lpszParsed );

		ret = TRUE;
	}
	GlobalFree( pSym );

	return ret;
}

// Get source file name and line number from IP address
// The output format is: "sourcefile(linenumber)" or
//                       "modulename!address" or
//                       "address"
static BOOL GetSourceInfoFromAddress( UINT address, LPTSTR lpszSourceInfo )
{
	BOOL           ret = FALSE;
	IMAGEHLP_LINE  lineInfo;
	DWORD          dwDisp;
	TCHAR          lpszFileName[BUFFERSIZE] = _T("");
	TCHAR          lpModuleInfo[BUFFERSIZE] = _T("");

	_tcscpy( lpszSourceInfo, _T("?(?)") );

	::ZeroMemory( &lineInfo, sizeof( lineInfo ) );
	lineInfo.SizeOfStruct = sizeof( lineInfo );

	if ( SymGetLineFromAddr( GetCurrentProcess(), address, &dwDisp, &lineInfo ) )
	{
		// Got it. Let's use "sourcefile(linenumber)" format
		PCSTR2LPTSTR( lineInfo.FileName, lpszFileName );
		TCHAR fname[_MAX_FNAME];
		TCHAR ext[_MAX_EXT];
		_tsplitpath(lpszFileName, nullptr, nullptr, fname, ext);
		_stprintf( lpszSourceInfo, _T("%s%s(%d)"), fname, ext, lineInfo.LineNumber );
		ret = TRUE;
	}
	else
	{
		// There is no source file information. :(
		// Let's use the "modulename!address" format
		GetModuleNameFromAddress( address, lpModuleInfo );

		if ( lpModuleInfo[0] == _T('?') || lpModuleInfo[0] == _T('\0'))
			// There is no modulename information. :((
			// Let's use the "address" format
			_stprintf( lpszSourceInfo, _T("0x%08X"), address );
		else
			_stprintf( lpszSourceInfo, _T("%s!0x%08X"), lpModuleInfo, address );

		ret = FALSE;
	}

	return ret;
}

void PrintFunctionAndSourceInfo(FILE* file, const STACKFRAME& callstack)
{
	TCHAR symInfo[BUFFERSIZE] = _T("?");
	TCHAR srcInfo[BUFFERSIZE] = _T("?");

	GetFunctionInfoFromAddresses((ULONG)callstack.AddrPC.Offset, (ULONG)callstack.AddrFrame.Offset, symInfo);
	GetSourceInfoFromAddress((ULONG)callstack.AddrPC.Offset, srcInfo);
	etfprint(file, "     " + TStrToUTF8(srcInfo) + " : " + TStrToUTF8(symInfo) + "\n");
}

void StackTrace( HANDLE hThread, const char* lpszMessage, FILE *file )
{
	STACKFRAME     callStack;
	BOOL           bResult;
	CONTEXT        context;
	HANDLE         hProcess = GetCurrentProcess();

	// If it's not this thread, let's suspend it, and resume it at the end
	if ( hThread != GetCurrentThread() )
		if ( SuspendThread( hThread ) == -1 )
		{
			// whaaat ?!
			etfprint(file, "Call stack info failed\n");
			return;
		}

		::ZeroMemory( &context, sizeof(context) );
		context.ContextFlags = CONTEXT_FULL;

		if ( !GetThreadContext( hThread, &context ) )
		{
			etfprint(file, "Call stack info failed\n");
			return;
		}

		::ZeroMemory( &callStack, sizeof(callStack) );
		callStack.AddrPC.Offset    = context.Rip;
		callStack.AddrStack.Offset = context.Rsp;
		callStack.AddrFrame.Offset = context.Rbp;
		callStack.AddrPC.Mode      = AddrModeFlat;
		callStack.AddrStack.Mode   = AddrModeFlat;
		callStack.AddrFrame.Mode   = AddrModeFlat;

		etfprint(file, "Call stack info: \n");
		etfprint(file, lpszMessage);

		PrintFunctionAndSourceInfo(file, callStack);

		for ( ULONG index = 0; ; index++ )
		{
			bResult = StackWalk(
				IMAGE_FILE_MACHINE_I386,
				hProcess,
				hThread,
				&callStack,
				nullptr,
				nullptr,
				SymFunctionTableAccess,
				SymGetModuleBase,
				nullptr);

			if ( index == 0 )
				continue;

			if (!bResult || callStack.AddrFrame.Offset == 0)
				break;

			PrintFunctionAndSourceInfo(file, callStack);

		}

		if (hThread != GetCurrentThread())
			ResumeThread( hThread );
}

void StackTrace(HANDLE hThread, const char* lpszMessage, FILE *file, DWORD eip, DWORD esp, DWORD ebp )
{
	STACKFRAME     callStack;
	BOOL           bResult;
	HANDLE         hProcess = GetCurrentProcess();

	// If it's not this thread, let's suspend it, and resume it at the end
	if ( hThread != GetCurrentThread() )
		if (SuspendThread( hThread ) == -1)
		{
			// whaaat ?!
			etfprint(file, "Call stack info failed\n");
			return;
		}

		::ZeroMemory( &callStack, sizeof(callStack) );
		callStack.AddrPC.Offset    = eip;
		callStack.AddrStack.Offset = esp;
		callStack.AddrFrame.Offset = ebp;
		callStack.AddrPC.Mode      = AddrModeFlat;
		callStack.AddrStack.Mode   = AddrModeFlat;
		callStack.AddrFrame.Mode   = AddrModeFlat;

		etfprint(file, "Call stack info: \n");
		etfprint(file, lpszMessage);

		PrintFunctionAndSourceInfo(file, callStack);

		for (ULONG index = 0; ; index++)
		{
			bResult = StackWalk(
				IMAGE_FILE_MACHINE_I386,
				hProcess,
				hThread,
				&callStack,
				nullptr,
				nullptr,
				SymFunctionTableAccess,
				SymGetModuleBase,
				nullptr);

			if (index == 0)
				continue;

			if (!bResult || callStack.AddrFrame.Offset == 0)
				break;

			PrintFunctionAndSourceInfo(file, callStack);
		}

		if (hThread != GetCurrentThread())
			ResumeThread(hThread);
}
#define UEFBUFSIZE 2048
static char g_uefbuf[UEFBUFSIZE];

void etfprintf(FILE *file, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int len = vsprintf(g_uefbuf, format, ap);
	fwrite(g_uefbuf, 1, len, file);
	va_end(ap);
}

void etfprint(FILE *file, const std::string &text)
{
	size_t len = text.length();
	fwrite(text.data(), 1, len, file);
}

#endif //WIN32
