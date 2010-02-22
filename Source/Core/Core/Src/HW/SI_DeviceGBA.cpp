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

// --- GameBoy Advance "Link Cable" ---

THREAD_RETURN ConnectionWaiter(void*)
{
	Common::SetCurrentThreadName("GBA Connection Waiter");

	sf::SocketTCP server;
	if (!server.Listen(0xd6ba))
		return 0;

	server.SetBlocking(false);

	for (;;)
	{
		sf::SocketTCP new_client;
		if (server.Accept(new_client) == sf::Socket::Done)
		{
			waiting_socks.push(new_client);
			PanicAlert("Connected");
		}
	}
	server.Close();
	return 0;
}

bool GetAvailableSock(sf::SocketTCP& sock_to_fill)
{
	if (waiting_socks.size() > 0)
	{
		sock_to_fill = waiting_socks.front();
		waiting_socks.pop();
		return true;
	}
	return false;
}

GBASockServer::GBASockServer()
{
	if (!connectionThread)
		connectionThread = new Common::Thread(ConnectionWaiter, (void*)0);
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
	client.Receive(current_data, sizeof(current_data), num_received);

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
