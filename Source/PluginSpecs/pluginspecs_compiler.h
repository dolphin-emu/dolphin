//__________________________________________________________________________________________________
// Common compiler plugin spec, version #1.0 maintained by F|RES
//

#ifndef _COMPILER_H_INCLUDED__
#define _COMPILER_H_INCLUDED__

#include "PluginSpecs.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define EXPORT						__declspec(dllexport)
#define CALL						_cdecl

typedef void (*TWriteBigEData)(const unsigned __int8* _pData, const unsigned __int32 _iAddress, const unsigned __int32 _iSize);
typedef void (*TPatchFunction)(DWORD _uAddress, unsigned char* _pMachineCode);
typedef void (*TLog)(char* _pMessage);

typedef struct
{
	HWND hWnd;
	TPatchFunction		pPatchFunction;
	TLog				pLog;	
} SCompilerInitialize;


// __________________________________________________________________________________________________
// Function: GetDllInfo
// Purpose:  This function allows the emulator to gather information
//           about the DLL by filling in the PluginInfo structure.
// input:    a pointer to a PLUGIN_INFO structure that needs to be
//           filled by the function. (see def above)
// output:   none
//
EXPORT void CALL GetDllInfo(PLUGIN_INFO* _PluginInfo);

// __________________________________________________________________________________________________
// Function: CloseDLL
// Purpose:  This function is called when the emulator is closing
//           down allowing the DLL to de-initialise.
// input:    none
// output:   none
//
EXPORT void CALL CloseDLL(void);

// __________________________________________________________________________________________________
// Function: DllAbout
// Purpose:  This function is optional function that is provided
//           to give further information about the DLL.
// input:    a handle to the window that calls this function
// output:   none
//
EXPORT void CALL DllAbout(HWND _hParent);

// __________________________________________________________________________________________________
// Function: DllConfig
// Purpose:  This function is optional function that is provided
//           to allow the user to configure the dll
// input:    a handle to the window that calls this function
// output:   none
//
EXPORT void CALL DllConfig(HWND _hParent);

// __________________________________________________________________________________________________
// Function: 
// Purpose:  
// input:    SCompilerInitialize
// output:   none
//
EXPORT void CALL CompilerInitialize(SCompilerInitialize _CompilerInitialize);

// __________________________________________________________________________________________________
// Function: SetupHeader 
// Purpose: 
// input:    
// output:   
// 
EXPORT void CALL SetupHeader(char* _szSourceCode);

// __________________________________________________________________________________________________
// Function: CompileSourceCode 
// Purpose:  This function will compile a NULL-terminated string of C
//           source code..
// input:    pDest:         Pointer to address for the new machine code
//           strSourceCode: The NULL-terminated C Source Code string
// output:   Size of the Compiled Source, 0 == ERROR
//
EXPORT DWORD CALL CompileSourceCode(char* _szSourceCode, BYTE* _pMachineCode, DWORD _dwMaxSize);

// __________________________________________________________________________________________________
// Function: InsertEventString
// Purpose:  This optional function will insert the code in 
//           EventString[] immediately before every block of compiled code 
//           returns. Used to check for Interrupts and cyclic tasks.
// input:    The Event's C code
// output:   TRUE/FALSE for pass or fail
// 
EXPORT DWORD CALL InsertEventString(char* _szEventSourceCode);

#if defined(__cplusplus)
}
#endif
#endif
