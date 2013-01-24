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

#include <functional>

#include "EXI_Device.h"
#include "EXI_DeviceGecko.h"
#include "../Core.h"

u16							GeckoSockServer::server_port;
int							GeckoSockServer::client_count;
std::thread					GeckoSockServer::connectionThread;
volatile bool				GeckoSockServer::server_running;
std::queue<sf::SocketTCP>	GeckoSockServer::waiting_socks;
std::mutex					GeckoSockServer::connection_lock;

GeckoSockServer::GeckoSockServer()
	: client_running(false)
{
	if (!connectionThread.joinable())
		connectionThread = std::thread(GeckoConnectionWaiter);
}

GeckoSockServer::~GeckoSockServer()
{
	if (clientThread.joinable())
		--client_count;

	client_running = false;
	clientThread.join();

	if (client_count <= 0)
	{
		server_running = false;
		connectionThread.join();
	}
}

void GeckoSockServer::GeckoConnectionWaiter()
{
	Common::SetCurrentThreadName("Gecko Connection Waiter");

	sf::SocketTCP server;
	server_port = 0xd6ec; // "dolphin gecko"
	for (int bind_tries = 0; bind_tries <= 10 && !server_running; bind_tries++)
	{
		if (!(server_running = server.Listen(server_port)))
			server_port++;
	}

	if (!server_running)
		return;

	Core::DisplayMessage(
		StringFromFormat("USBGecko: Listening on TCP port %u", server_port),
		5000);

	server.SetBlocking(false);

	sf::SocketTCP new_client;
	while (server_running)
	{
		if (server.Accept(new_client) == sf::Socket::Done)
		{
			std::lock_guard<std::mutex> lk(connection_lock);
			waiting_socks.push(new_client);
		}
		SLEEP(1);
	}
	server.Close();
}

bool GeckoSockServer::GetAvailableSock(sf::SocketTCP &sock_to_fill)
{
	bool sock_filled = false;

	std::lock_guard<std::mutex> lk(connection_lock);

	if (waiting_socks.size())
	{
		sock_to_fill = waiting_socks.front();
		if (clientThread.joinable())
		{
			client_running = false;
			clientThread.join();

			recv_fifo = std::queue<u8>();
			send_fifo = std::queue<u8>();
		}
		clientThread = std::thread(std::mem_fun(&GeckoSockServer::ClientThread), this);
		client_count++;
		waiting_socks.pop();
		sock_filled = true;
	}

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
		
		{
		std::lock_guard<std::mutex> lk(transfer_lock);

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
		}	// unlock transfer

		SLEEP(1);
	}

	client.Close();
}

void CEXIGecko::ImmReadWrite(u32 &_uData, u32 _uSize)
{
	// We don't really care about _uSize
	(void)_uSize;

	if (!client.IsValid())
		GetAvailableSock(client);

	switch (_uData >> 28)
	{
	case CMD_LED_OFF:
		Core::DisplayMessage(StringFromFormat(
			"USBGecko: No LEDs for you!"),
			3000);
		break;
	case CMD_LED_ON:
		Core::DisplayMessage(StringFromFormat(
			"USBGecko: A piercing blue light is now shining in your general direction"),
			3000);
		break;
	
	case CMD_INIT:
		_uData = ident;
		break;

		// PC -> Gecko
		// |= 0x08000000 if successful
	case CMD_RECV:
		{
		std::lock_guard<std::mutex> lk(transfer_lock);
		if (!recv_fifo.empty())
		{
			_uData = 0x08000000 | (recv_fifo.front() << 16);
			recv_fifo.pop();
		}
		break;
		}

		// Gecko -> PC
		// |= 0x04000000 if successful
	case CMD_SEND:
		{
		std::lock_guard<std::mutex> lk(transfer_lock);
		send_fifo.push(_uData >> 20);
		_uData = 0x04000000;
		break;
		}

		// Check if ok for Gecko -> PC, or FIFO full
		// |= 0x04000000 if FIFO is not full
	case CMD_CHK_TX:
		_uData = 0x04000000;
		break;

		// Check if data in FIFO for PC -> Gecko, or FIFO empty
		// |= 0x04000000 if data in recv FIFO
	case CMD_CHK_RX:
		{
		std::lock_guard<std::mutex> lk(transfer_lock);
		_uData = recv_fifo.empty() ? 0 : 0x04000000;
		break;
		}

	default:
		ERROR_LOG(EXPANSIONINTERFACE, "Uknown USBGecko command %x", _uData);
		break;
	}
}
