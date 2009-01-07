//__________________________________________________________________________________________________
// Common video plugin spec, version #1.0 maintained by F|RES
//

#ifndef _VIDEO_H_INCLUDED__
#define _VIDEO_H_INCLUDED__

#include "Thread.h"
#include "PluginSpecs.h"

#include "ExportProlog.h"


typedef void			(*TSetPEToken)(const unsigned short _token, const int _bSetTokenAcknowledge);
typedef void			(*TSetPEFinish)(void);
typedef unsigned char*	(*TGetMemoryPointer)(const unsigned int  _iAddress);
typedef void			(*TVideoLog)(const char* _pMessage, int _bBreak);
typedef void			(*TSysMessage)(const char *fmt, ...);
typedef void			(*TRequestWindowSize)(int _iWidth, int _iHeight, bool _bFullscreen);
typedef void			(*TCopiedToXFB)(void);
typedef unsigned int	(*TPeekMessages)(void);
typedef void			(*TUpdateInterrupts)(void);
typedef void			(*TUpdateFPSDisplay)(const char* text); // sets the window title
typedef void			(*TKeyPressed)(int keycode, bool shift, bool control); // sets the window title

typedef struct
{
	// fifo registers
	volatile u32 CPBase;
	volatile u32 CPEnd;
	u32 CPHiWatermark;
	u32 CPLoWatermark;
	volatile u32 CPReadWriteDistance;
	volatile u32 CPWritePointer;
	volatile u32 CPReadPointer;
	volatile u32 CPBreakpoint;

	// Super Monkey Ball Adventure require this.
	// Because the read&check-PEToken-loop stays in its JITed block I suppose.
	// So no possiblity to ack the Token irq by the scheduler until some sort of PPC watchdog do its mess.
	volatile u16 PEToken;

	volatile u32 bFF_GPReadEnable;
	volatile u32 bFF_BPEnable;
	volatile u32 bFF_GPLinkEnable;
	volatile u32 bFF_Breakpoint;

	volatile u32 CPCmdIdle;
	volatile u32 CPReadIdle;	

	// for GP watchdog hack
	volatile u32 Fake_GPWDToken; // cicular incrementer
} SCPFifoStruct;

typedef struct
{
	void *pWindowHandle;

	TSetPEToken						pSetPEToken;
	TSetPEFinish					pSetPEFinish;
	TGetMemoryPointer				pGetMemoryPointer;
	TVideoLog						pLog;
	TSysMessage						pSysMessage;
	TRequestWindowSize              pRequestWindowSize;
	TCopiedToXFB					pCopiedToXFB;
	TPeekMessages					pPeekMessages;
	TUpdateInterrupts               pUpdateInterrupts;
    TUpdateFPSDisplay               pUpdateFPSDisplay;
	TKeyPressed                     pKeyPress;

	SCPFifoStruct                   *pCPFifo;
	unsigned char                   *pVIRegs;
	void *pMemoryBase;
	bool bWii;
} SVideoInitialize;

/////////////////////////////////////////////////////////////////////////////////////////////////////
// I N T E R F A C E ////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

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
EXPORT void CALL Video_SendFifoData(u8* _uData, u32 len);

// __________________________________________________________________________________________________
// Function: Video_UpdateXFB
// Purpose:  This fucntion is called when you have to flip the yuv2
//		     video-buffer. You should ignore this function after you
//		     got the first EFB to XFB copy.
// input:    pointer to the XFB, width and height of the XFB
// output:   none
//
EXPORT void CALL Video_UpdateXFB(u8* _pXFB, u32 _dwWidth, u32 _dwHeight, s32 _dwYOffset);

// __________________________________________________________________________________________________
// Function: Video_Screenshot
// Purpose:  This fucntion is called when you want to do a screenshot
// input:    Filename
// output:   TRUE if all was okay
//
EXPORT unsigned int CALL Video_Screenshot(TCHAR* _szFilename);

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
// Function: Video_DoState
// Purpose:  Saves/Loads the current video data state (depends on mode parameter)
// input/output: ptr
// input: mode
//
EXPORT void CALL Video_DoState(unsigned char **ptr, int mode);

// __________________________________________________________________________________________________
// Function: Video_Stop
// Purpose:  Stop the video plugin before shutdown
// input/output: 
// input: 
//
EXPORT void CALL Video_Stop();

#include "ExportEpilog.h"
#endif
