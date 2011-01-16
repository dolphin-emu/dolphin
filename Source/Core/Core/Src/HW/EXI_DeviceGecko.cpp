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

#include "EXI_Device.h"
#include "EXI_DeviceGecko.h"

#include "SFML/Network.hpp"
#include "Thread.h"
#include <queue>

static Common::Thread *connectionThread = NULL;
static std::queue<sf::SocketTCP> waiting_socks;
static Common::CriticalSection cs_gecko;
namespace { volatile bool server_running; }

static THREAD_RETURN GeckoConnectionWaiter(void*)
{
	server_running = true;

	Common::SetCurrentThreadName("Gecko Connection Waiter");

	sf::SocketTCP server;
	// "dolphin gecko"
	if (!server.Listen(0xd6ec))
		return 0;

	server.SetBlocking(false);

	sf::SocketTCP new_client;
	while (server_running)
	{
		if (server.Accept(new_client) == sf::Socket::Done)
		{
			cs_gecko.Enter();
			waiting_socks.push(new_client);
			cs_gecko.Leave();
		}
		SLEEP(1);
	}
	server.Close();
	return 0;
}

void GeckoConnectionWaiter_Shutdown()
{
	server_running = false;
	if (connectionThread)
	{
		connectionThread->WaitForDeath();
		delete connectionThread;
		connectionThread = NULL;
	}
}

static bool GetAvailableSock(sf::SocketTCP& sock_to_fill)
{
	bool sock_filled = false;

	cs_gecko.Enter();
	if (waiting_socks.size())
	{
		sock_to_fill = waiting_socks.front();
		waiting_socks.pop();
		sock_filled = true;
	}
	cs_gecko.Leave();

	return sock_filled;
}

GeckoSockServer::GeckoSockServer()
{
	if (!connectionThread)
		connectionThread = new Common::Thread(GeckoConnectionWaiter, (void*)0);
}

GeckoSockServer::~GeckoSockServer()
{
	client.Close();
}

CEXIGecko::CEXIGecko()
	: m_uPosition(0)
	, recv_fifo(false)
{
}

void CEXIGecko::SetCS(int cs)
{
	if (cs)
		m_uPosition = 0;
}

bool CEXIGecko::IsPresent()
{
	return true;
}

void CEXIGecko::ImmReadWrite(u32 &_uData, u32 _uSize)
{
	if (!client.IsValid())
		if (!GetAvailableSock(client))
			;// TODO nothing for now

	// for debug
	u32 oldval = _uData;
	// TODO do we really care about _uSize?

	u8 data = 0;
	std::size_t	got;

	switch (_uData >> 28)
	{
		// maybe do something fun later
	case CMD_LED_OFF:
		break;
	case CMD_LED_ON:
		break;
		// maybe should only | 2bytes?
	case CMD_INIT:
		_uData = ident;
		break;
		// PC -> Gecko
	case CMD_RECV:
		// TODO recv
		client.Receive((char*)&data, sizeof(data), got);
		recv_fifo = !!got;
		if (recv_fifo)
			_uData = 0x08000000 | (data << 16);
		break;
		// Gecko -> PC
	case CMD_SEND:
		data = (_uData >> 20) & 0xff;
		// TODO send
		client.Send((char*)&data, sizeof(data));
		// If successful
		_uData |= 0x04000000;
		break;
		// Check if ok for Gecko -> PC, or FIFO full
		// |= 0x04000000 if FIFO is not full
	case CMD_CHK_TX:
		_uData = 0x04000000;
		break;
		// Check if data in FIFO for PC -> Gecko, or FIFO empty
		// |= 0x04000000 if data in recv FIFO
	case CMD_CHK_RX:
		//_uData = recv_fifo ? 0x04000000 : 0;
		_uData = 0x04000000;
		break;
	default:
		ERROR_LOG(EXPANSIONINTERFACE, "Uknown USBGecko command %x", _uData);
		break;
	}

	if (_uData) { ERROR_LOG(EXPANSIONINTERFACE, "rw %x %08x %08x", oldval, _uData, _uSize); }
}
