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

#ifndef _EXIDEVICE_ETHERNET_H
#define _EXIDEVICE_ETHERNET_H

class CEXIETHERNET : public IEXIDevice
{
public:
	CEXIETHERNET();
	void SetCS(int _iCS);
	bool IsPresent();
	void Update();
	bool IsInterruptSet();
	void ImmWrite(u32 _uData,  u32 _uSize);
	u32  ImmRead(u32 _uSize);
	void DMAWrite(u32 _uAddr, u32 _uSize);
	void DMARead(u32 _uAddr, u32 _uSize);

private:
	// STATE_TO_SAVE
	u32 m_uPosition;
	u32 m_uCommand;
	bool mReadyToSend;
	unsigned int ID;
	u8 RegisterBlock[0x1000];
	enum {
		CMD_ID = 0x00,
		CMD_READ_REG = 0x01,
	};

	void Transfer(u8& _uByte);
};
#endif
