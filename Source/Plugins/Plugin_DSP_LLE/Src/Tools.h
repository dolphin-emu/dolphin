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

#pragma once

// UDSPControl
union UDSPControl
{
	uint16 Hex;
	struct
	{
		unsigned DSPReset       : 1; // Write 1 to reset and waits for 0
		unsigned DSPAssertInt   : 1;
		unsigned DSPHalt        : 1;

		unsigned AI             : 1;
		unsigned AI_mask        : 1;
		unsigned ARAM           : 1;
		unsigned ARAM_mask      : 1;
		unsigned DSP            : 1;
		unsigned DSP_mask       : 1;

		unsigned ARAM_DMAState  : 1; // DSPGetDMAStatus() uses this flag
		unsigned unk3           : 1;
		unsigned DSPInit        : 1; // DSPInit() writes to this flag
		unsigned pad            : 4;
	};

	UDSPControl(uint16 _Hex = 0)
		: Hex(_Hex) {}
};


bool DumpDSPCode(uint32 _Address, uint32 _Length, uint32 crc);
uint32 GenerateCRC(const unsigned char* _pBuffer, int _pLength);
bool DumpCWCode(uint32 _Address, uint32 _Length);
