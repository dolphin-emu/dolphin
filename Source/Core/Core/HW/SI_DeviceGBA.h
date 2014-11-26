// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI_Device.h"

#include "SFML/Network.hpp"

// GameBoy Advance "Link Cable"

void GBAConnectionWaiter_Shutdown();

class GBASockServer
{
public:
	GBASockServer();
	~GBASockServer();

	void Transfer(char* si_buffer);

private:
	enum EJoybusCmds
	{
		CMD_RESET  = 0xff,
		CMD_STATUS = 0x00,
		CMD_READ   = 0x14,
		CMD_WRITE  = 0x15
	};

	std::unique_ptr<sf::TcpSocket> client;
	char current_data[5];
};

class CSIDevice_GBA : public ISIDevice, private GBASockServer
{
public:
	CSIDevice_GBA(SIDevices device, int _iDeviceNumber);
	~CSIDevice_GBA() {}

	// Run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength) override;

	virtual bool GetData(u32& _Hi, u32& _Low) override { return true; }
	virtual void SendCommand(u32 _Cmd, u8 _Poll) override {}
};
