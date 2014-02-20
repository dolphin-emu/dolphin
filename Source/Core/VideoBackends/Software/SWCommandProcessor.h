// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"

class PointerWrap;
namespace MMIO { class Mapping; }

extern volatile bool g_bSkipCurrentFrame;
extern u8* g_pVideoData;

namespace SWCommandProcessor
{
	// internal hardware addresses
	enum
	{
		STATUS_REGISTER          = 0x00,
		CTRL_REGISTER            = 0x02,
		CLEAR_REGISTER           = 0x04,
		FIFO_TOKEN_REGISTER      = 0x0E,
		FIFO_BOUNDING_BOX_LEFT   = 0x10,
		FIFO_BOUNDING_BOX_RIGHT  = 0x12,
		FIFO_BOUNDING_BOX_TOP    = 0x14,
		FIFO_BOUNDING_BOX_BOTTOM = 0x16,
		FIFO_BASE_LO             = 0x20,
		FIFO_BASE_HI             = 0x22,
		FIFO_END_LO              = 0x24,
		FIFO_END_HI              = 0x26,
		FIFO_HI_WATERMARK_LO     = 0x28,
		FIFO_HI_WATERMARK_HI     = 0x2a,
		FIFO_LO_WATERMARK_LO     = 0x2c,
		FIFO_LO_WATERMARK_HI     = 0x2e,
		FIFO_RW_DISTANCE_LO      = 0x30,
		FIFO_RW_DISTANCE_HI      = 0x32,
		FIFO_WRITE_POINTER_LO    = 0x34,
		FIFO_WRITE_POINTER_HI    = 0x36,
		FIFO_READ_POINTER_LO     = 0x38,
		FIFO_READ_POINTER_HI     = 0x3A,
		FIFO_BP_LO               = 0x3C,
		FIFO_BP_HI               = 0x3E
	};

	// Fifo Status Register
	union UCPStatusReg
	{
		struct
		{
			u16 OverflowHiWatermark  : 1;
			u16 UnderflowLoWatermark : 1;
			u16 ReadIdle             : 1; // done reading
			u16 CommandIdle          : 1; // done processing commands
			u16 Breakpoint           : 1;
			u16                      : 11;
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
			u16 GPReadEnable           : 1;
			u16 BPEnable               : 1;
			u16 FifoOverflowIntEnable  : 1;
			u16 FifoUnderflowIntEnable : 1;
			u16 GPLinkEnable           : 1;
			u16 BreakPointIntEnable    : 1;
			u16                        : 10;
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
			u16 ClearFifoOverflow  : 1;
			u16 ClearFifoUnderflow : 1;
			u16 ClearMetrices      : 1;
			u16                    : 13;
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
		u32 unk0;               // 0x08
		u16 unk1;               // 0x0c
		u16 token;              // 0x0e
		u16 bboxleft;           // 0x10
		u16 bboxtop;            // 0x12
		u16 bboxright;          // 0x14
		u16 bboxbottom;         // 0x16
		u32 unk2;               // 0x18
		u32 unk3;               // 0x1c
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

	void RegisterMMIO(MMIO::Mapping* mmio, u32 base);

	bool RunBuffer();
	void RunGpu();

	// for CGPFIFO
	void GatherPipeBursted();
	void UpdateInterrupts(u64 userdata);
	void UpdateInterruptsFromVideoBackend(u64 userdata);

	void SetRendering(bool enabled);

} // end of namespace SWCommandProcessor
