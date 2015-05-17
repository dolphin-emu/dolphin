// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <queue>

#include "Common/CommonFuncs.h"
#include "Common/Flag.h"
#include "Common/StdMakeUnique.h"
#include "Common/Thread.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/SI_Device.h"
#include "Core/HW/SI_DeviceGBA.h"
#include "Core/HW/SystemTimers.h"
#include "Core/HW/VideoInterface.h"

#include "SFML/Network.hpp"

static std::thread connectionThread;
static std::queue<std::unique_ptr<sf::TcpSocket>> waiting_socks;
static std::queue<std::unique_ptr<sf::TcpSocket>> waiting_clocks;
static std::mutex cs_gba;
static std::mutex cs_gba_clk;
static u8 num_connected;

namespace { Common::Flag server_running; }

enum EJoybusCmds
{
	CMD_RESET  = 0xff,
	CMD_STATUS = 0x00,
	CMD_READ   = 0x14,
	CMD_WRITE  = 0x15
};

const u64 BITS_PER_SECOND = 115200;
const u64 BYTES_PER_SECOND = BITS_PER_SECOND / 8;

u8 GetNumConnected()
{
	int num_ports_connected = num_connected;
	if (num_ports_connected == 0)
		num_ports_connected = 1;

	return num_ports_connected;
}

// --- GameBoy Advance "Link Cable" ---

int GetTransferTime(u8 cmd)
{
	u64 bytes_transferred = 0;

	switch (cmd)
	{
	case CMD_RESET:
	case CMD_STATUS:
	{
		bytes_transferred = 4;
		break;
	}
	case CMD_READ:
	{
		bytes_transferred = 6;
		break;
	}
	case CMD_WRITE:
	{
		bytes_transferred = 1;
		break;
	}
	default:
	{
		bytes_transferred = 1;
		break;
	}
	}
	return (int)(bytes_transferred * SystemTimers::GetTicksPerSecond() / (GetNumConnected() * BYTES_PER_SECOND));
}

static void GBAConnectionWaiter()
{
	server_running.Set();

	Common::SetCurrentThreadName("GBA Connection Waiter");

	sf::TcpListener server;
	sf::TcpListener clock_server;

	// "dolphin gba"
	if (server.listen(0xd6ba) != sf::Socket::Done)
		return;

	// "clock"
	if (clock_server.listen(0xc10c) != sf::Socket::Done)
		return;

	server.setBlocking(false);
	clock_server.setBlocking(false);

	auto new_client = std::make_unique<sf::TcpSocket>();
	while (server_running.IsSet())
	{
		if (server.accept(*new_client) == sf::Socket::Done)
		{
			std::lock_guard<std::mutex> lk(cs_gba);
			waiting_socks.push(std::move(new_client));

			new_client = std::make_unique<sf::TcpSocket>();
		}
		if (clock_server.accept(*new_client) == sf::Socket::Done)
		{
			std::lock_guard<std::mutex> lk(cs_gba_clk);
			waiting_clocks.push(std::move(new_client));

			new_client = std::make_unique<sf::TcpSocket>();
		}

		Common::SleepCurrentThread(1);
	}
}

void GBAConnectionWaiter_Shutdown()
{
	server_running.Clear();
	if (connectionThread.joinable())
		connectionThread.join();
}

static bool GetAvailableSock(std::unique_ptr<sf::TcpSocket>& sock_to_fill)
{
	bool sock_filled = false;

	std::lock_guard<std::mutex> lk(cs_gba);

	if (!waiting_socks.empty())
	{
		sock_to_fill = std::move(waiting_socks.front());
		waiting_socks.pop();
		sock_filled = true;
	}

	return sock_filled;
}

static bool GetNextClock(std::unique_ptr<sf::TcpSocket>& sock_to_fill)
{
	bool sock_filled = false;

	std::lock_guard<std::mutex> lk(cs_gba_clk);

	if (!waiting_clocks.empty())
	{
		sock_to_fill = std::move(waiting_clocks.front());
		waiting_clocks.pop();
		sock_filled = true;
	}

	return sock_filled;
}

GBASockServer::GBASockServer(int _iDeviceNumber)
{
	if (!connectionThread.joinable())
		connectionThread = std::thread(GBAConnectionWaiter);

	cmd = 0;
	num_connected = 0;
	last_time_slice = 0;
	booted = false;
	device_number = _iDeviceNumber;
}

GBASockServer::~GBASockServer()
{
	Disconnect();
}

void GBASockServer::Disconnect()
{
	if (client)
	{
		num_connected--;
		client->disconnect();
		client = nullptr;
	}
	if (clock_sync)
	{
		clock_sync->disconnect();
		clock_sync = nullptr;
	}
	last_time_slice = 0;
	booted = false;
}

void GBASockServer::ClockSync()
{
	if (!clock_sync)
		if (!GetNextClock(clock_sync))
			return;

	u32 time_slice = 0;

	if (last_time_slice == 0)
	{
		num_connected++;
		last_time_slice = CoreTiming::GetTicks();
		time_slice = (u32)(SystemTimers::GetTicksPerSecond() / 60);
	}
	else
	{
		time_slice = (u32)(CoreTiming::GetTicks() - last_time_slice);
	}

	time_slice = (u32)((u64)time_slice * 16777216 / SystemTimers::GetTicksPerSecond());
	last_time_slice = CoreTiming::GetTicks();
	char bytes[4] = { 0, 0, 0, 0 };
	bytes[0] = (time_slice >> 24) & 0xff;
	bytes[1] = (time_slice >> 16) & 0xff;
	bytes[2] = (time_slice >> 8) & 0xff;
	bytes[3] = time_slice & 0xff;

	sf::Socket::Status status = clock_sync->send(bytes, 4);
	if (status == sf::Socket::Disconnected)
	{
		clock_sync->disconnect();
		clock_sync = nullptr;
	}
}

void GBASockServer::Send(u8* si_buffer)
{
	if (!client)
		if (!GetAvailableSock(client))
			return;

	for (int i = 0; i < 5; i++)
		send_data[i] = si_buffer[i ^ 3];

	cmd = (u8)send_data[0];

#ifdef _DEBUG
	NOTICE_LOG(SERIALINTERFACE, "%01d cmd %02x [> %02x%02x%02x%02x]",
		device_number,
		(u8)send_data[0], (u8)send_data[1], (u8)send_data[2],
		(u8)send_data[3], (u8)send_data[4]);
#endif

	client->setBlocking(false);
	sf::Socket::Status status;
	if (cmd == CMD_WRITE)
		status = client->send(send_data, sizeof(send_data));
	else
		status = client->send(send_data, 1);

	if (cmd != CMD_STATUS)
		booted = true;

	if (status == sf::Socket::Disconnected)
		Disconnect();

	time_cmd_sent = CoreTiming::GetTicks();
}

int GBASockServer::Receive(u8* si_buffer)
{
	if (!client)
		if (!GetAvailableSock(client))
			return 5;

	size_t num_received = 0;
	u64 transferTime = GetTransferTime((u8)send_data[0]);
	bool block = (CoreTiming::GetTicks() - time_cmd_sent) > transferTime;
	if (cmd == CMD_STATUS && !booted)
		block = false;

	if (block)
	{
		sf::SocketSelector Selector;
		Selector.add(*client);
		Selector.wait(sf::milliseconds(1000));
	}

	sf::Socket::Status recv_stat = client->receive(recv_data, sizeof(recv_data), num_received);
	if (recv_stat == sf::Socket::Disconnected)
	{
		Disconnect();
		return 5;
	}

	if (recv_stat == sf::Socket::NotReady)
		num_received = 0;

	if (num_received > sizeof(recv_data))
		num_received = sizeof(recv_data);

	if (num_received > 0)
	{
#ifdef _DEBUG
		if ((u8)send_data[0] == 0x00 || (u8)send_data[0] == 0xff)
		{
			WARN_LOG(SERIALINTERFACE, "%01d                              [< %02x%02x%02x%02x%02x] (%lu)",
				device_number,
				(u8)recv_data[0], (u8)recv_data[1], (u8)recv_data[2],
				(u8)recv_data[3], (u8)recv_data[4],
				num_received);
		}
		else
		{
			ERROR_LOG(SERIALINTERFACE, "%01d                              [< %02x%02x%02x%02x%02x] (%lu)",
				device_number,
				(u8)recv_data[0], (u8)recv_data[1], (u8)recv_data[2],
				(u8)recv_data[3], (u8)recv_data[4],
				num_received);
		}
#endif

		for (int i = 0; i < 5; i++)
			si_buffer[i ^ 3] = recv_data[i];
	}

	 return (int)num_received;
}

CSIDevice_GBA::CSIDevice_GBA(SIDevices _device, int _iDeviceNumber)
	: ISIDevice(_device, _iDeviceNumber)
	, GBASockServer(_iDeviceNumber)
{
	waiting_for_response = false;
}

CSIDevice_GBA::~CSIDevice_GBA()
{
	GBASockServer::Disconnect();
}

int CSIDevice_GBA::RunBuffer(u8* _pBuffer, int _iLength)
{
	if (!waiting_for_response)
	{
		for (int i = 0; i < 5; i++)
			send_data[i] = _pBuffer[i ^ 3];

		num_data_received = 0;
		ClockSync();
		Send(_pBuffer);
		timestamp_sent = CoreTiming::GetTicks();
		waiting_for_response = true;
	}

	if (waiting_for_response && num_data_received == 0)
	{
		num_data_received = Receive(_pBuffer);
	}

	if ((GetTransferTime(send_data[0])) > (int)(CoreTiming::GetTicks() - timestamp_sent))
	{
		return 0;
	}
	else
	{
		if (num_data_received != 0)
			waiting_for_response = false;
		return num_data_received;
	}
}

int CSIDevice_GBA::TransferInterval()
{
	return GetTransferTime(send_data[0]);
}
