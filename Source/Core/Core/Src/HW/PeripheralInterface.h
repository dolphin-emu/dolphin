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

#ifndef _PERIPHERALINTERFACE_H
#define _PERIPHERALINTERFACE_H

#include "Common.h"
//
// PERIPHERALINTERFACE
// Handles communication with cpu services like the write gatherer used for fifos, and interrupts
//

class CPeripheralInterface
{
friend class CRegisters;

public:
    enum InterruptCause
    {
        INT_CAUSE_ERROR       =    0x1, // ?
        INT_CAUSE_RSW         =    0x2, // Reset Switch    
        INT_CAUSE_DI          =    0x4, // DVD interrupt 
        INT_CAUSE_SI          =    0x8, // Serial interface    
        INT_CAUSE_EXI         =   0x10, // Expansion interface
        INT_CAUSE_AUDIO       =   0x20, // Audio Interface Streaming
        INT_CAUSE_DSP         =   0x40, // DSP interface
        INT_CAUSE_MEMORY      =   0x80, // Memory interface
        INT_CAUSE_VI          =  0x100, // Video interface
        INT_CAUSE_PE_TOKEN    =  0x200, // GP Token
        INT_CAUSE_PE_FINISH   =  0x400, // GP Finished
        INT_CAUSE_CP          =  0x800, // Command Fifo
        INT_CAUSE_DEBUG       = 0x1000, // ?    
        INT_CAUSE_HSP         = 0x2000, // High Speed Port
        INT_CAUSE_WII_IPC     = 0x4000, // Wii IPC
        INT_CAUSE_RST_BUTTON  = 0x10000 // ResetButtonState (1 = unpressed, 0 = pressed)
    };

private:

    // internal hardware addresses
    enum
    {
        PI_INTERRUPT_CAUSE		= 0x000,
        PI_INTERRUPT_MASK       = 0x004,
        PI_FIFO_BASE            = 0x00C,
        PI_FIFO_END             = 0x010,
        PI_FIFO_WPTR            = 0x014,
        PI_FIFO_RESET           = 0x018,    // ??? - GXAbortFrameWrites to it
        PI_RESET_CODE           = 0x024,
        PI_MB_REV               = 0x02C,
    };

    //This must always be byteswapped :(  (UNUSED ATM)
    struct PIRegMem
    {
        u32 rInterruptCause; //00
        u32 rInterruptMask;  //04
        u32 unused0;        //08
        u32 FifoBase;       //0C
        u32 FifoEnd;        //10
        u32 FifoWptr;       //14
        u32 FifoReset;      //18
        u32 unused1;        //1C
        u32 unused2;        //20
        u32 ResetCode;      //24
        u32 unused3;        //28
        u32 MBRev;          //2C
    };

    static volatile u32 m_InterruptMask;
    static volatile u32 m_InterruptCause;

    static void UpdateException();

    // Debug_GetInterruptName
    static const char* Debug_GetInterruptName(InterruptCause _causemask);

public:

    static u32 Fifo_CPUBase;
    static u32 Fifo_CPUEnd;
    static u32 Fifo_CPUWritePointer;

    static void Init();
    
    static void SetInterrupt(InterruptCause _causemask, bool _bSet=true);
    
    // Read32
    static void HWCALL Read32(u32& _uReturnValue, const u32 _iAddress);

    // Write32
    static void HWCALL Write32(const u32 _iValue, const u32 _iAddress);

    // SetResetButton (you have to release the button again to make a reset)
    static void SetResetButton(bool _bSet);
};

#endif

