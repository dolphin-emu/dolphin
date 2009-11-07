//__________________________________________________________________________________________________
// Common dsp plugin spec, version #1.0 maintained by F|RES
//

#ifndef _DSP_H_INCLUDED__
#define _DSP_H_INCLUDED__

#include "PluginSpecs.h"
#include "ExportProlog.h"

typedef unsigned char   (*TARAM_Read_U8)(const unsigned int _uAddress);
typedef void            (*TARAM_Write_U8)(const unsigned char _uValue, const unsigned int _uAddress);
typedef unsigned char*  (*TGetMemoryPointer)(const unsigned int  _uAddress);
typedef unsigned char* 	(*TGetARAMPointer)(void);
typedef void            (*TLogv)(const char* _szMessage, int _v);
typedef const char*     (*TName)(void);
typedef void            (*TDebuggerBreak)(void);
typedef void            (*TGenerateDSPInt)(void);
typedef unsigned int    (*TAudioGetStreaming)(short* _pDestBuffer, unsigned int _numSamples);

typedef struct
{
	void                   *hWnd;
	TARAM_Read_U8			pARAM_Read_U8;
	TARAM_Write_U8			pARAM_Write_U8;
	TGetMemoryPointer		pGetMemoryPointer;
	TGetARAMPointer			pGetARAMPointer;
	TLogv					pLog;	
	TName					pName;
	TDebuggerBreak			pDebuggerBreak;
	TGenerateDSPInt			pGenerateDSPInterrupt;
	TAudioGetStreaming		pGetAudioStreaming;
	int                    *pEmulatorState;
	bool					bWii;
	bool                    bOnThread;
} DSPInitialize;


// __________________________________________________________________________________________________
// Function: DSP_ReadMailboxHigh
// Purpose:  Send mail to high DSP Mailbox
// input:    none
// output:   none
//
EXPORT unsigned short CALL DSP_ReadMailboxHigh(bool _CPUMailbox);

// __________________________________________________________________________________________________
// Function: DSP_ReadMailboxLow
// Purpose:  Send mail to low DSP Mailbox
// input:    none
// output:   none
//
EXPORT unsigned short CALL DSP_ReadMailboxLow(bool _CPUMailbox);

// __________________________________________________________________________________________________
// Function: DSP_WriteMailboxHigh
// Purpose:  Send mail to high CPU Mailbox
// input:    none
// output:   none
//
EXPORT void CALL DSP_WriteMailboxHigh(bool _CPUMailbox, unsigned short _uHighMail);

// __________________________________________________________________________________________________
// Function: DSP_WriteMailboxLow
// Purpose:  Send mail to low CPU Mailbox
// input:    none
// output:   none
//
EXPORT void CALL DSP_WriteMailboxLow(bool _CPUMailbox, unsigned short _uLowMail);

// __________________________________________________________________________________________________
// Function: DSP_WriteControlRegister
// Purpose:  This function is called if the core reads from the DSP control register
// input:    Value to be written
// output:   value of the control register
// 
EXPORT unsigned short CALL DSP_WriteControlRegister(unsigned short _Value);

// __________________________________________________________________________________________________
// Function: DSP_ReadControlRegister
// Purpose:  This function is called if the core reads from the DSP control register
// output:   value of the control register
// 
EXPORT unsigned short CALL DSP_ReadControlRegister(void);

// __________________________________________________________________________________________________
// Function: DSP_Update
// Purpose:  This function is called from time to time from the core. 
// input:    cycles - run this number of DSP clock cycles.
// output:   TRUE if the flag is set, else FALSE
// 
EXPORT void CALL DSP_Update(int cycles);

// __________________________________________________________________________________________________
// Function: DSP_SendAIBuffer
// Purpose:  This function sends the current AI Buffer to the DSP plugin
// input:    _Address : Memory-Address
// input:    _Size : Size of the Buffer (always 32)
// 
EXPORT void CALL DSP_SendAIBuffer(unsigned int address, int sample_rate);

// __________________________________________________________________________________________________
// Function: DSP_StopSoundStream
// Purpose:  Stops audio playback. Must be called before Shutdown().
EXPORT void CALL DSP_StopSoundStream();

// __________________________________________________________________________________________________
// Function: DSP_ClearAudioBuffer
// Purpose:  Stops audio. Called while pausing to stop the annoying noises.
EXPORT void CALL DSP_ClearAudioBuffer();

#include "ExportEpilog.h"
#endif
