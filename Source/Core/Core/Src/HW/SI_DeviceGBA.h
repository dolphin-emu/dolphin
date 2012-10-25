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

#ifndef _SI_DEVICEGBA_H
#define _SI_DEVICEGBA_H

#include "SFML/Network.hpp"

// GameBoy Advance "Link Cable"

void GBAConnectionWaiter_Shutdown();

class GBASockServer : public sf::SocketTCP
{
public:
	GBASockServer();
	~GBASockServer();

	void Transfer(char* si_buffer);

private:
	enum EJoybusCmds
	{
		CMD_RESET	= 0xff,
		CMD_STATUS	= 0x00,
		CMD_READ	= 0x14,
		CMD_WRITE	= 0x15		
	};

	sf::SocketTCP client;
	char current_data[5];
};

class CSIDevice_GBA : public ISIDevice, private GBASockServer
{
public:
	CSIDevice_GBA(SIDevices device, int _iDeviceNumber);
	~CSIDevice_GBA() {}

	// Run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength);

	virtual bool GetData(u32& _Hi, u32& _Low) { return true; }
	virtual void SendCommand(u32 _Cmd, u8 _Poll) {}
};

#endif
