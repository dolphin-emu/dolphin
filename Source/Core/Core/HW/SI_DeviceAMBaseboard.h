// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// triforce (GC-AM) baseboard
class CSIDevice_AMBaseboard : public ISIDevice
{
private:
	enum EBufferCommands
	{
		CMD_RESET = 0x00,
		CMD_GCAM  = 0x70,
	};

public:
	// constructor
	CSIDevice_AMBaseboard(SIDevices device, int iDeviceNumber);

	// run the SI Buffer
	virtual int RunBuffer(u8* pBuffer, int iLength) override;

	// return true on new data
	virtual bool GetData(u32& Hi, u32& Low) override;

	// send a command directly
	virtual void SendCommand(u32 Cmd, u8 Poll) override;
};
