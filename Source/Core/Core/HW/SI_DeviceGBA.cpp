// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <queue>

#include "Common/CommonFuncs.h"
#include "Common/Thread.h"
#include "Common/Logging/Log.h"
#include "Core/HW/SI_Device.h"
#include "Core/HW/SI_DeviceGBA.h"

#include "SFML/Network.hpp"

static std::thread connectionThread;
static std::queue<sf::SocketTCP> waiting_socks;
static std::mutex cs_gba;
namespace { volatile bool server_running; }

// --- GameBoy Advance "Link Cable" ---

static void GBAConnectionWaiter()
{
	server_running = true;

	Common::SetCurrentThreadName("GBA Connection Waiter");

	sf::SocketTCP server;
	// "dolphin gba"
	if (!server.Listen(0xd6ba))
		return;

	server.SetBlocking(false);

	sf::SocketTCP new_client;
	while (server_running)
	{
		if (server.Accept(new_client) == sf::Socket::Done)
		{
			std::lock_guard<std::mutex> lk(cs_gba);
			waiting_socks.push(new_client);
		}
		SLEEP(1);
	}
	server.Close();
	return;
}

void GBAConnectionWaiter_Shutdown()
{
	server_running = false;
	if (connectionThread.joinable())
		connectionThread.join();
}

static bool GetAvailableSock(sf::SocketTCP& sock_to_fill)
{
	bool sock_filled = false;

	std::lock_guard<std::mutex> lk(cs_gba);

	if (!waiting_socks.empty())
	{
		sock_to_fill = waiting_socks.front();
		waiting_socks.pop();
		sock_filled = true;
	}

	return sock_filled;
}

GBASockServer::GBASockServer()
{
	if (!connectionThread.joinable())
		connectionThread = std::thread(GBAConnectionWaiter);
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

	for (int i = 0; i < 5; i++)
		current_data[i] = si_buffer[i ^ 3];

	u8 cmd = *current_data;

	if (cmd == CMD_WRITE)
		client.Send(current_data, sizeof(current_data));
	else
		client.Send(current_data, 1);

	DEBUG_LOG(SERIALINTERFACE, "> command %02x %02x%02x%02x%02x",
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
		ERROR_LOG(SERIALINTERFACE, "%x:%x:%x", (u8)cmd,
				(unsigned int)num_received, (unsigned int)num_expecting);
#endif

	for (int i = 0; i < 5; i++)
		si_buffer[i ^ 3] = current_data[i];
}

CSIDevice_GBA::CSIDevice_GBA(SIDevices _device, int _iDeviceNumber)
	: ISIDevice(_device, _iDeviceNumber)
	, GBASockServer()
{
}

int CSIDevice_GBA::RunBuffer(u8* _pBuffer, int _iLength)
{
	Transfer((char*)_pBuffer);
	return _iLength;
}
