// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <deque>
#include <queue>

#include <SFML/Network.hpp>

#include "Common/Thread.h"

#include "Core/HW/EXI_Device.h"

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

	std::deque<u8> send_fifo;
	std::deque<u8> recv_fifo;

private:
	static int    client_count;
	volatile bool client_running;

	// Only ever one server thread
	static void GeckoConnectionWaiter();

	static u16                       server_port;
	static volatile bool             server_running;
	static std::thread               connectionThread;
	static std::queue<sf::SocketTCP> waiting_socks;
	static std::mutex                connection_lock;
};

class CEXIGecko
	: public IEXIDevice
	, private GeckoSockServer
{
public:
	CEXIGecko() {}
	bool IsPresent() override { return true; }
	void ImmReadWrite(u32 &_uData, u32 _uSize) override;

private:
	enum
	{
		CMD_LED_OFF = 0x7,
		CMD_LED_ON  = 0x8,
		CMD_INIT    = 0x9,
		CMD_RECV    = 0xa,
		CMD_SEND    = 0xb,
		CMD_CHK_TX  = 0xc,
		CMD_CHK_RX  = 0xd,
	};

	static const u32 ident = 0x04700000;
};
