//__________________________________________________________________________________________________
// Common video plugin spec, version #1.0 maintained by F|RES
//

#ifndef _VIDEO_H_INCLUDED__
#define _VIDEO_H_INCLUDED__

#include "PluginSpecs.h"

#include "ExportProlog.h"

typedef void (*TimedCallback)(u64 userdata, int cyclesLate);

typedef void            (*TSetInterrupt)(u32 _causemask, bool _bSet);
typedef int             (*TRegisterEvent)(const char *name, TimedCallback callback);
typedef void            (*TScheduleEvent_Threadsafe)(int cyclesIntoFuture, int event_type, u64 userdata);
typedef unsigned char*	(*TGetMemoryPointer)(const unsigned int  _iAddress);
typedef void			(*TVideoLog)(const char* _pMessage, int _bBreak);
typedef void			(*TSysMessage)(const char *fmt, ...);
typedef void			(*TRequestWindowSize)(int _iWidth, int _iHeight, bool _bFullscreen);
typedef void			(*TCopiedToXFB)(bool video_update);
typedef unsigned int	(*TPeekMessages)(void);
typedef void			(*TUpdateFPSDisplay)(const char* text); // sets the window title
typedef void			(*TKeyPressed)(int keycode, bool shift, bool control); // sets the window title

enum FieldType
{
	FIELD_PROGRESSIVE = 0,
	FIELD_UPPER,
	FIELD_LOWER
};

enum EFBAccessType
{
	PEEK_Z = 0,
	POKE_Z,
	PEEK_COLOR,
	POKE_COLOR
};

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

    TSetInterrupt                   pSetInterrupt;
    TRegisterEvent                  pRegisterEvent;
    TScheduleEvent_Threadsafe       pScheduleEvent_Threadsafe;
	TGetMemoryPointer				pGetMemoryPointer;
	TVideoLog						pLog;
	TSysMessage						pSysMessage;
	TRequestWindowSize              pRequestWindowSize;
	TCopiedToXFB					pCopiedToXFB;
	TPeekMessages					pPeekMessages;
    TUpdateFPSDisplay               pUpdateFPSDisplay;
	TKeyPressed                     pKeyPress;
	void *pMemoryBase;
	bool bWii;
	bool bOnThread;
    u32 *Fifo_CPUBase;
    u32 *Fifo_CPUEnd;
    u32 *Fifo_CPUWritePointer;

} SVideoInitialize;


// I N T E R F A C E 


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
// Function: Video_BeginField
// Purpose:  When a field begins in the VI emulator, this function tells the video plugin what the
//           parameters of the upcoming field are. The video plugin should make sure the previous
//           field is on the player's display before returning.
// input:    vi parameters of the upcoming field
// output:   none
//
EXPORT void CALL Video_BeginField(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight);

// __________________________________________________________________________________________________
// Function: Video_EndField
// Purpose:  When a field ends in the VI emulator, this function notifies the video plugin. The video
//           has permission to swap the field to the player's display.
// input:    none
// output:   none
//
EXPORT void CALL Video_EndField();

// __________________________________________________________________________________________________
// Function: Video_AccessEFB
// input:    type of access (r/w, z/color, ...), x coord, y coord
// output:   response to the access request (ex: peek z data at specified coord)
//
EXPORT u32 CALL Video_AccessEFB(EFBAccessType type, u32 x, u32 y);

// __________________________________________________________________________________________________
// Function: Video_Screenshot
// input:    Filename
// output:   TRUE if all was okay
//
EXPORT void CALL Video_Screenshot(const char *_szFilename);

// __________________________________________________________________________________________________
// Function: Video_EnterLoop
// Purpose:  Enters the video fifo dispatch loop. This is only used in Dual Core mode.
// input:    none
// output:   none
//
EXPORT void CALL Video_EnterLoop(void);

// __________________________________________________________________________________________________
// Function: Video_ExitLoop
// Purpose:  Exits the video dispatch loop. This is only used in Dual Core mode.
// input:    none
// output:   none
//
EXPORT void CALL Video_ExitLoop(void);

// __________________________________________________________________________________________________
// Function: Video_SetRendering
// Purpose:  Sets video rendering on and off. Currently used for frame skipping
// input:    Enabled toggle
// output:   none
//
EXPORT void CALL Video_SetRendering(bool bEnabled);

// __________________________________________________________________________________________________
// Function: Video_AddMessage
// Purpose:  Adds a message to the display queue, to be shown forthe specified time
// input:    pointer to the null-terminated string, time in milliseconds
// output:   none
//
EXPORT void CALL Video_AddMessage(const char* pstr, unsigned int milliseconds);

EXPORT void CALL Video_CommandProcessorRead16(u16& _rReturnValue, const u32 _Address);
EXPORT void CALL Video_CommandProcessorWrite16(const u16 _Data, const u32 _Address);
EXPORT void CALL Video_PixelEngineRead16(u16& _rReturnValue, const u32 _Address);
EXPORT void CALL Video_PixelEngineWrite16(const u16 _Data, const u32 _Address);
EXPORT void CALL Video_PixelEngineWrite32(const u32 _Data, const u32 _Address);
EXPORT void CALL Video_GatherPipeBursted(void);
EXPORT void CALL Video_WaitForFrameFinish(void);

#include "ExportEpilog.h"
#endif
