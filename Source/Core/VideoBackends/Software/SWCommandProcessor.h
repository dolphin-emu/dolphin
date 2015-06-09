// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include "VideoCommon/CommandProcessor.h"

class PointerWrap;
namespace MMIO { class Mapping; }

namespace SWCommandProcessor
{
	using UCPStatusReg = CommandProcessor::UCPStatusReg;
	using UCPCtrlReg = CommandProcessor::UCPCtrlReg;
	using UCPClearReg = CommandProcessor::UCPClearReg;

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
