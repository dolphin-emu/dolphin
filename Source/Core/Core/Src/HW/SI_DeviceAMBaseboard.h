// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _SIDEVICE_AMBASEBOARD_H
#define _SIDEVICE_AMBASEBOARD_H

// triforce (GC-AM) baseboard
class CSIDevice_AMBaseboard : public ISIDevice
{
private:
	enum EBufferCommands
	{
		CMD_RESET		= 0x00,
		CMD_GCAM		= 0x70,
	};

public:
	// constructor
	CSIDevice_AMBaseboard(SIDevices device, int _iDeviceNumber);

	// run the SI Buffer
	virtual int RunBuffer(u8* _pBuffer, int _iLength);

	// return true on new data
	virtual bool GetData(u32& _Hi, u32& _Low);

	// send a command directly
	virtual void SendCommand(u32 _Cmd, u8 _Poll);
};

#endif // _SIDEVICE_AMBASEBOARD_H
