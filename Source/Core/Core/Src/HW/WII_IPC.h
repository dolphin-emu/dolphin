// Copyright (C) 2003 Dolphin Project.

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
#ifndef _WII_IPC_H_
#define _WII_IPC_H_

#include "Common.h"
class PointerWrap;

namespace WII_IPCInterface
{

enum StarletInterruptCause
{
	INT_CAUSE_TIMER			=    0x1,
	INT_CAUSE_NAND			=    0x2,
	INT_CAUSE_AES			=    0x4,
	INT_CAUSE_SHA1			=    0x8,
	INT_CAUSE_EHCI			=   0x10,
	INT_CAUSE_OHCI0			=   0x20,
	INT_CAUSE_OHCI1			=   0x40,
	INT_CAUSE_SD			=   0x80,
	INT_CAUSE_WIFI			=  0x100,

	INT_CAUSE_GPIO_BROADWAY	=  0x400,
	INT_CAUSE_GPIO_STARLET	=  0x800,

	INT_CAUSE_RST_BUTTON	= 0x40000,

	INT_CAUSE_IPC_BROADWAY	= 0x40000000,
	INT_CAUSE_IPC_STARLET	= 0x80000000
};

void Init();
void Reset();
void Shutdown();	
void DoState(PointerWrap &p);

void Read32(u32& _rReturnValue, const u32 _Address);
void Write32(const u32 _Value, const u32 _Address);

void UpdateInterrupts();
void GenerateAck(u32 _Address);
void GenerateReply(u32 _Address);

bool IsReady();

}

#endif
