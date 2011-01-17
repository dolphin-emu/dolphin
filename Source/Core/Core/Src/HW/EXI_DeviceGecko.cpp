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

THREAD_RETURN ClientThreadFunc(void *arg)
{
	((GeckoSockServer*)arg)->ClientThread();
	return 0;
}

int							GeckoSockServer::client_count;
Common::Thread				*GeckoSockServer::connectionThread = NULL;
volatile bool				GeckoSockServer::server_running;
std::queue<sf::SocketTCP>	GeckoSockServer::waiting_socks;
Common::CriticalSection		GeckoSockServer::connection_lock;

GeckoSockServer::GeckoSockServer()
	: clientThread(NULL)
	, client_running(false)
{
	if (!connectionThread)
		connectionThread = new Common::Thread(&GeckoConnectionWaiter, (void*)0);
}

GeckoSockServer::~GeckoSockServer()
{
	if (clientThread)
		--client_count;

	client_running = false;
	delete clientThread;
	clientThread = NULL;

	if (client_count <= 0)
	{
		server_running = false;
		delete connectionThread;
		connectionThread = NULL;
	}
}

THREAD_RETURN GeckoSockServer::GeckoConnectionWaiter(void*)
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
			connection_lock.Enter();
			waiting_socks.push(new_client);
			connection_lock.Leave();
		}
		SLEEP(1);
	}
	server.Close();
	return 0;
}

bool GeckoSockServer::GetAvailableSock(sf::SocketTCP &sock_to_fill)
{
	bool sock_filled = false;

	connection_lock.Enter();
	if (waiting_socks.size())
	{
		sock_to_fill = waiting_socks.front();
		if (clientThread)
		{
			client_running = false;
			delete clientThread;
			clientThread = NULL;
		}
		clientThread = new Common::Thread(ClientThreadFunc, this);
		client_count++;
		waiting_socks.pop();
		sock_filled = true;
	}
	connection_lock.Leave();

	return sock_filled;
}

void GeckoSockServer::ClientThread()
{
	client_running = true;

	Common::SetCurrentThreadName("Gecko Client");

	client.SetBlocking(false);

	while (client_running)
	{
		u8 data;
		std::size_t	got = 0;

		transfer_lock.Enter();
		if (client.Receive((char*)&data, sizeof(data), got)
			== sf::Socket::Disconnected)
			client_running = false;
		if (got)
			recv_fifo.push(data);

		if (send_fifo.size())
		{
			if (client.Send((char*)&send_fifo.front(), sizeof(u8))
				== sf::Socket::Disconnected)
				client_running = false;
			send_fifo.pop();
		}
		transfer_lock.Leave();
		SLEEP(1);
	}

	client.Close();
}

void CEXIGecko::ImmReadWrite(u32 &_uData, u32 _uSize)
{
	// We don't really care about _uSize
	(void)_uSize;

	if (!client.IsValid())
		if (!GetAvailableSock(client))
			return;

	// for debug
	u32 oldval = _uData;

	switch (_uData >> 28)
	{
		// maybe do something fun later
	case CMD_LED_OFF:
		break;
	case CMD_LED_ON:
		break;
	
	case CMD_INIT:
		_uData = ident;
		break;

		// PC -> Gecko
		// |= 0x08000000 if successful
	case CMD_RECV:
		transfer_lock.Enter();
		if (recv_fifo.size())
		{
			_uData = 0x08000000 | (recv_fifo.front() << 16);
			recv_fifo.pop();
		}
		transfer_lock.Leave();
		break;
		// Gecko -> PC
		// |= 0x04000000 if successful
	case CMD_SEND:
		transfer_lock.Enter();
		send_fifo.push(_uData >> 20);
		transfer_lock.Leave();
		_uData = 0x04000000;
		break;

		// Check if ok for Gecko -> PC, or FIFO full
		// |= 0x04000000 if FIFO is not full
	case CMD_CHK_TX:
		_uData = 0x04000000;
		break;
		// Check if data in FIFO for PC -> Gecko, or FIFO empty
		// |= 0x04000000 if data in recv FIFO
	case CMD_CHK_RX:
		transfer_lock.Enter();
		_uData = recv_fifo.size() ? 0x04000000 : 0;
		transfer_lock.Leave();
		break;

	default:
		ERROR_LOG(EXPANSIONINTERFACE, "Uknown USBGecko command %x", _uData);
		break;
	}
}
