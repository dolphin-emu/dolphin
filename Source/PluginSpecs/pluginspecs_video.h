//__________________________________________________________________________________________________
// Common video plugin spec, version #1.0 maintained by F|RES
//

#ifndef _VIDEO_H_INCLUDED__
#define _VIDEO_H_INCLUDED__

#include "PluginSpecs.h"

#include "ExportProlog.h"

typedef void			(*TSetPEToken)(const unsigned short _token, const int _bSetTokenAcknowledge);
typedef void			(*TSetPEFinish)(void);
typedef unsigned char*	(*TGetMemoryPointer)(const unsigned int  _iAddress);
typedef void			(*TVideoLog)(const char* _pMessage, BOOL _bBreak);
typedef void			(*TRequestWindowSize)(int _iWidth, int _iHeight, BOOL _bFullscreen);
typedef void			(*TCopiedToXFB)(void);
typedef BOOL			(*TPeekMessages)(void);
typedef void			(*TUpdateInterrupts)(void);
typedef void			(*TUpdateFPSDisplay)(const char* text); // sets the window title

typedef struct
{
	// fifo registers
	volatile DWORD CPBase;
	volatile DWORD CPEnd;
	DWORD CPHiWatermark;
	DWORD CPLoWatermark;
	volatile INT CPReadWriteDistance;
	volatile DWORD CPWritePointer;
	volatile DWORD CPReadPointer;
	volatile DWORD CPBreakpoint;

	volatile bool bFF_GPReadEnable;
	volatile bool bFF_BPEnable;
	volatile bool bFF_GPLinkEnable;
	volatile bool bFF_Breakpoint;
	volatile bool bPauseRead;
#ifdef _WIN32
	CRITICAL_SECTION sync;
#endif
} SCPFifoStruct;

typedef struct
{
	void *pWindowHandle;

	TSetPEToken						pSetPEToken;
	TSetPEFinish					pSetPEFinish;
	TGetMemoryPointer				pGetMemoryPointer;
	TVideoLog						pLog;
	TRequestWindowSize              pRequestWindowSize;
	TCopiedToXFB					pCopiedToXFB;
	TPeekMessages					pPeekMessages;
	TUpdateInterrupts               pUpdateInterrupts;
    TUpdateFPSDisplay               pUpdateFPSDisplay;
	SCPFifoStruct                   *pCPFifo;

	unsigned char                   *pVIRegs;
	void *pMemoryBase;
} SVideoInitialize;

/////////////////////////////////////////////////////////////////////////////////////////////////////
// I N T E R F A C E ////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

// __________________________________________________________________________________________________
// Function: GetDllInfo
// Purpose:  This function allows the emulator to gather information
//           about the DLL by filling in the PluginInfo structure.
// input:    a pointer to a PLUGIN_INFO structure that needs to be
//           filled by the function. (see def above)
// output:   none
//
//
#ifndef _WIN32 
#define _CDECLCALL 
#endif

EXPORT void CALL GetDllInfo(PLUGIN_INFO* _pPluginInfo);

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
//           to allow the user to configure the DLL
// input:    a handle to the window that calls this function
// output:   none
//
EXPORT void CALL DllConfig(HWND _hParent);

// __________________________________________________________________________________________________
// Function: Video_Initialize
// Purpose:
// input:    SVideoInitialize* - pointer because window data will be passed back
// output:   none
//
EXPORT void CALL Video_Initialize(SVideoInitialize* _pvideoInitialize);

// __________________________________________________________________________________________________
// Function: Video_Prepare
// Purpose:  This function is called from the EmuThread before the
// 		     emulation has started. It is just for threadsensitive 
//   	     APIs like OpenGL.
// input:    none
// output:   none
//
EXPORT void CALL Video_Prepare(void);

// __________________________________________________________________________________________________
// Function: Video_Shutdown
// Purpose:  This function is called when the emulator is shutting down
//  		 a game allowing the dll to de-initialise.
// input:    none
// output:   none
//
EXPORT void CALL Video_Shutdown(void);

// __________________________________________________________________________________________________
// Function: Video_ExecuteFifoBuffer
// Purpose:  This function is called if data is inside the fifo-buffer
// input:    a data-byte (i know we have to optimize this ;-))
// output:   none
//
EXPORT void CALL Video_SendFifoData(BYTE *_uData);

// __________________________________________________________________________________________________
// Function: Video_UpdateXFB
// Purpose:  This fucntion is called when you have to flip the yuv2
//		     video-buffer. You should ignore this function after you
//		     got the first EFB to XFB copy.
// input:    pointer to the XFB, width and height of the XFB
// output:   none
//
EXPORT void CALL Video_UpdateXFB(BYTE* _pXFB, DWORD _dwWidth, DWORD _dwHeight);

// __________________________________________________________________________________________________
// Function: Video_Screenshot
// Purpose:  This fucntion is called when you want to do a screenshot
// input:    Filename
// output:   TRUE if all was okay
//
EXPORT BOOL CALL Video_Screenshot(TCHAR* _szFilename);


// __________________________________________________________________________________________________
// Function: Video_EnterLoop
// Purpose:  FIXME!
// input:    none
// output:   none
//
EXPORT void CALL Video_EnterLoop(void);

// __________________________________________________________________________________________________
// Function: Video_AddMessage
// Purpose:  Adds a message to the display queue, to be shown forthe specified time
// input:    pointer to the null-terminated string, time in milliseconds
// output:   none
//
EXPORT void CALL Video_AddMessage(const char* pstr, unsigned int milliseconds);

// __________________________________________________________________________________________________
// Function: Video_SaveState
// Purpose:  Saves the current video data state
// input:    The chunkfile to write to? FIXME
// output:   none
//
EXPORT void CALL Video_SaveState(void);

// __________________________________________________________________________________________________
// Function: Video_LoadState
// Purpose:  Loads the current video data state
// input:    The chunkfile to read from? FIXME
// output:   none
//
EXPORT void CALL Video_LoadState(void);

#include "ExportEpilog.h"
#endif
