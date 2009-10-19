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

#ifndef _EXIDEVICE_AMBASEBOARD_H
#define _EXIDEVICE_AMBASEBOARD_H

class CEXIAMBaseboard : public IEXIDevice
{
public:
	CEXIAMBaseboard();

	virtual void SetCS(int _iCS);
	virtual bool IsPresent();
	virtual bool IsInterruptSet();

private:
	virtual void TransferByte(u8& _uByte);
	int m_position;
	bool m_have_irq;
	unsigned char m_command[4];
};

#endif
