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

class GBASockServer : public sf::SocketTCP
{
public:
	GBASockServer();
	~GBASockServer();

	void Transfer(char* si_buffer);

	char current_data[5];

private:
	enum EBufferCommands
	{
		CMD_RESET	= 0xff,
		CMD_STATUS	= 0x00,
		CMD_READ	= 0x14,
		CMD_WRITE	= 0x15		
	};

	sf::SocketTCP server;
	sf::SocketTCP client;
	sf::IPAddress client_addr;
};

class CSIDevice_GBA : public ISIDevice, private GBASockServer
{
public:
	CSIDevice_GBA(int _iDeviceNumber);
	~CSIDevice_GBA();

	// Run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength);

	// Return true on new data
	virtual bool GetData(u32& _Hi, u32& _Low);

	// Send a command directly
	virtual void SendCommand(u32 _Cmd, u8 _Poll);
};

#endif
