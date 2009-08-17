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

#include "dsp_interface.h"

void IDSP::SendTask(void *addr, u16 iram_addr, u16 len, u16 start)
{
	// addr			main ram addr			4byte aligned (1 Gekko word)
	// iram_addr	dsp addr				4byte aligned (2 DSP words)
	// len			block length in bytes	multiple of 4 (wtf? if you want to fill whole iram, you need 8191)
	//											(8191 % 4 = 3) wtffff
	// start		dsp iram entry point	
	while (CheckMailTo());
	SendMailTo(0x80F3A001);
	while (CheckMailTo());
	SendMailTo((u32)addr);
	while (CheckMailTo());
	SendMailTo(0x80F3C002);
	while (CheckMailTo());
	SendMailTo(iram_addr);
	while (CheckMailTo());
	SendMailTo(0x80F3A002);
	while (CheckMailTo());
	SendMailTo(len);
	while (CheckMailTo());
	SendMailTo(0x80F3B002);
	while (CheckMailTo());
	SendMailTo(0);
	while (CheckMailTo());
	SendMailTo(0x80F3D001);
	while (CheckMailTo());
	SendMailTo(start);
	while (CheckMailTo());
}
