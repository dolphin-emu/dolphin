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

#include "SI_Device.h"
#include "SI_DeviceGBA.h"

#include "SFML/Network.hpp"
#include "Thread.h"
#include <queue>

static Common::Thread *connectionThread = NULL;
static std::queue<sf::SocketTCP> waiting_socks;
static Common::CriticalSection cs_gba;
volatile bool server_running;

// --- GameBoy Advance "Link Cable" ---

THREAD_RETURN GBAConnectionWaiter(void*)
{
	server_running = true;

	Common::SetCurrentThreadName("GBA Connection Waiter");

	sf::SocketTCP server;
	if (!server.Listen(0xd6ba))
		return 0;
	
	server.SetBlocking(false);
	
	sf::SocketTCP new_client;
	while (server_running)
	{
		if (server.Accept(new_client) == sf::Socket::Done)
		{
			cs_gba.Enter();
			waiting_socks.push(new_client);
			cs_gba.Leave();
		}
		SLEEP(1);
	}
	server.Close();
	return 0;
}

void GBAConnectionWaiter_Shutdown()
{
	server_running = false;
	if (connectionThread)
	{
		connectionThread->WaitForDeath();
		delete connectionThread;
		connectionThread = NULL;
	}
}

bool GetAvailableSock(sf::SocketTCP& sock_to_fill)
{
	bool sock_filled = false;

	cs_gba.Enter();
	if (waiting_socks.size())
	{
		sock_to_fill = waiting_socks.front();
		waiting_socks.pop();
		sock_filled = true;
	}
	cs_gba.Leave();

	return sock_filled;
}

GBASockServer::GBASockServer()
{
	if (!connectionThread)
		connectionThread = new Common::Thread(GBAConnectionWaiter, (void*)0);
}

GBASockServer::~GBASockServer()
{
	client.Close();
}

// Blocking, since GBA must always send lower byte of REG_JOYSTAT
void GBASockServer::Transfer(char* si_buffer)
{
	if (!client.IsValid())
		if (!GetAvailableSock(client))
			return;

	current_data[0] = si_buffer[3];
	current_data[1] = si_buffer[2];
	current_data[2] = si_buffer[1];
	current_data[3] = si_buffer[0];
	current_data[4] = si_buffer[7];

	u8 cmd = *current_data;

	if (cmd == CMD_WRITE)
		client.Send(current_data, sizeof(current_data));
	else
		client.Send(current_data, 1);

	DEBUG_LOG(SERIALINTERFACE, "> cmd %02x %02x%02x%02x%02x",
		(u8)current_data[0], (u8)current_data[1], (u8)current_data[2],
		(u8)current_data[3], (u8)current_data[4]);

	memset(current_data, 0, sizeof(current_data));
	size_t num_received = 0;
	if (client.Receive(current_data, sizeof(current_data), num_received) == sf::Socket::Disconnected)
		client.Close();

	DEBUG_LOG(SERIALINTERFACE, "< %02x%02x%02x%02x%02x",
		(u8)current_data[0], (u8)current_data[1], (u8)current_data[2],
		(u8)current_data[3], (u8)current_data[4]);

#ifdef _DEBUG
	size_t num_expecting = 3;
	if (cmd == CMD_READ)
		num_expecting = 5;
	else if (cmd == CMD_WRITE)
		num_expecting = 1;
	if (num_received != num_expecting)
		ERROR_LOG(SERIALINTERFACE, "%x:%x:%x", (u8)cmd, num_received, num_expecting);
#endif

	si_buffer[0] = current_data[3];
	si_buffer[1] = current_data[2];
	si_buffer[2] = current_data[1];
	si_buffer[3] = current_data[0];
	si_buffer[7] = current_data[4];
}

CSIDevice_GBA::CSIDevice_GBA(int _iDeviceNumber)
	: ISIDevice(_iDeviceNumber)
	, GBASockServer()
{
}

int CSIDevice_GBA::RunBuffer(u8* _pBuffer, int _iLength)
{
	Transfer((char*)_pBuffer);
	return _iLength;
}
