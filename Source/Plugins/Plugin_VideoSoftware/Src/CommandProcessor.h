// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _COMMANDPROCESSOR_H_
#define _COMMANDPROCESSOR_H_
   
#include "Common.h"
#include "pluginspecs_video.h"
class PointerWrap;

extern volatile bool g_bSkipCurrentFrame;

// for compatibility with video common
void Fifo_Init();
void Fifo_Shutdown();
void Fifo_DoState(PointerWrap &p);

void Fifo_EnterLoop(const SVideoInitialize &video_initialize);
void Fifo_ExitLoop();
void Fifo_SetRendering(bool bEnabled);

// Implemented by the Video Plugin
void VideoFifo_CheckSwapRequest();
void VideoFifo_CheckSwapRequestAt(u32 xfbAddr, u32 fbWidth, u32 fbHeight);
void VideoFifo_CheckEFBAccess();

namespace CommandProcessor
{
    // internal hardware addresses
    enum
    {
	    STATUS_REGISTER				= 0x00,
	    CTRL_REGISTER				= 0x02,
	    CLEAR_REGISTER				= 0x04,
	    FIFO_TOKEN_REGISTER			= 0x0E,
	    FIFO_BOUNDING_BOX_LEFT		= 0x10,
	    FIFO_BOUNDING_BOX_RIGHT		= 0x12,
	    FIFO_BOUNDING_BOX_TOP		= 0x14,
	    FIFO_BOUNDING_BOX_BOTTOM	= 0x16,
	    FIFO_BASE_LO				= 0x20,
	    FIFO_BASE_HI				= 0x22,
	    FIFO_END_LO					= 0x24,
	    FIFO_END_HI					= 0x26,
	    FIFO_HI_WATERMARK_LO		= 0x28,
	    FIFO_HI_WATERMARK_HI		= 0x2a,
	    FIFO_LO_WATERMARK_LO		= 0x2c,
	    FIFO_LO_WATERMARK_HI		= 0x2e,
	    FIFO_RW_DISTANCE_LO			= 0x30,
	    FIFO_RW_DISTANCE_HI			= 0x32,
	    FIFO_WRITE_POINTER_LO		= 0x34,
	    FIFO_WRITE_POINTER_HI		= 0x36,
	    FIFO_READ_POINTER_LO		= 0x38,
	    FIFO_READ_POINTER_HI		= 0x3A,
	    FIFO_BP_LO					= 0x3C,
	    FIFO_BP_HI					= 0x3E
    };

    // Fifo Status Register
    union UCPStatusReg
    {
	    struct
	    {
		    unsigned OverflowHiWatermark	:	1;
		    unsigned UnderflowLoWatermark	:	1;
		    unsigned ReadIdle				:	1; // done reading
		    unsigned CommandIdle			:	1; // done processing commands
		    unsigned Breakpoint				:	1;
		    unsigned						:	11;
	    };
	    u16 Hex;
	    UCPStatusReg() {Hex = 0; }
	    UCPStatusReg(u16 _hex) {Hex = _hex; }
    };

    // Fifo Control Register
    union UCPCtrlReg
    {
	    struct
	    {
		    unsigned GPReadEnable			:	1;
		    unsigned BPEnable           	:	1;
		    unsigned FifoOverflowIntEnable	:	1;
		    unsigned FifoUnderflowIntEnable	:	1;
		    unsigned GPLinkEnable			:	1;
		    unsigned BreakPointIntEnable	:	1;
		    unsigned						:	10;
	    };
	    u16 Hex;
	    UCPCtrlReg() {Hex = 0; }
	    UCPCtrlReg(u16 _hex) {Hex = _hex; }
    };

    // Fifo Control Register
    union UCPClearReg
    {
	    struct
	    {
		    unsigned ClearFifoOverflow		:	1;
		    unsigned ClearFifoUnderflow		:	1;
		    unsigned ClearMetrices			:	1;
		    unsigned						:	13;
	    };
	    u16 Hex;
	    UCPClearReg() {Hex = 0; }
	    UCPClearReg(u16 _hex) {Hex = _hex; }
    };

    struct CPReg
    {
        UCPStatusReg status;    // 0x00
        UCPCtrlReg ctrl;        // 0x02
        UCPClearReg clear;      // 0x04
        u32 unk0;               // 0x06
        u32 unk1;               // 0x0a
        u16 token;              // 0x0e
        u16 bboxleft;           // 0x10
        u16 bboxtop;            // 0x12
        u16 bboxright;          // 0x14
        u16 bboxbottom;         // 0x16
        u16 unk2;               // 0x18
        u32 fifobase;           // 0x20
        u32 fifoend;            // 0x24
        u32 hiwatermark;        // 0x28
        u32 lowatermark;        // 0x2c
        u32 rwdistance;         // 0x30
        u32 writeptr;           // 0x34
        u32 readptr;            // 0x38
        u32 breakpt;            // 0x3c
    };

    extern CPReg cpreg;

    // Init
    void Init();
    void Shutdown();
    void DoState(PointerWrap &p);

    bool RunBuffer();
    void RunGpu();

    // Read
    void Read16(u16& _rReturnValue, const u32 _Address);
    void Write16(const u16 _Data, const u32 _Address);
    void Read32(u32& _rReturnValue, const u32 _Address);
    void Write32(const u32 _Data, const u32 _Address);

    // for CGPFIFO
    void GatherPipeBursted();
    void UpdateInterrupts(u64 userdata);
    void UpdateInterruptsFromVideoPlugin(u64 userdata);


} // end of namespace CommandProcessor


#endif
