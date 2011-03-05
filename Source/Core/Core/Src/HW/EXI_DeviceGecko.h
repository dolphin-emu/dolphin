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

#ifndef _EXIDEVICE_GECKO_H
#define _EXIDEVICE_GECKO_H

#include "SFML/Network.hpp"
#include "Thread.h"
#include <queue>

class GeckoSockServer
	: public sf::SocketTCP
{
public:
	GeckoSockServer();
	~GeckoSockServer();
	bool GetAvailableSock(sf::SocketTCP &sock_to_fill);

	// Client for this server object
	sf::SocketTCP client;
	void ClientThread();
	std::thread clientThread;
	std::mutex transfer_lock;

	std::queue<u8> send_fifo;
	std::queue<u8> recv_fifo;

private:
	static int		client_count;
	volatile bool	client_running;

	// Only ever one server thread
	static void GeckoConnectionWaiter();

	static u16							server_port;
	static volatile bool				server_running;
	static std::thread					connectionThread;
	static std::queue<sf::SocketTCP>	waiting_socks;
	static std::mutex					connection_lock;
};

class CEXIGecko
	: public IEXIDevice
	, private GeckoSockServer
{
public:
	CEXIGecko() {}
	bool IsPresent() { return true; }
	void ImmReadWrite(u32 &_uData, u32 _uSize);

private:
	enum
	{
		CMD_LED_OFF	= 0x7,
		CMD_LED_ON	= 0x8,
		CMD_INIT	= 0x9,
		CMD_RECV	= 0xa,
		CMD_SEND	= 0xb,
		CMD_CHK_TX	= 0xc,
		CMD_CHK_RX	= 0xd,
	};

	static const u32 ident = 0x04700000;
};

#endif

