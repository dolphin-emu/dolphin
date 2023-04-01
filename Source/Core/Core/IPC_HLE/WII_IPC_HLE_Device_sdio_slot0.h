// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// PRELIMINARY - seems to fully work with libogc, writing has yet to be tested

#pragma once

#include <string>
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class PointerWrap;
namespace File { class IOFile; }

class CWII_IPC_HLE_Device_sdio_slot0 : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_sdio_slot0(u32 _DeviceID, const std::string& _rDeviceName);

	void DoState(PointerWrap& p) override;

	IPCCommandResult Open(u32 _CommandAddress, u32 _Mode) override;
	IPCCommandResult Close(u32 _CommandAddress, bool _bForce) override;

	IPCCommandResult IOCtl(u32 _CommandAddress) override;
	IPCCommandResult IOCtlV(u32 _CommandAddress) override;

	static void EnqueueReply(u32 CommandAddress, u32 ReturnValue);
	void EventNotify();

private:

	// SD Host Controller Registers
	enum
	{
		HCR_CLOCKCONTROL  = 0x2C,
		HCR_SOFTWARERESET = 0x2F,
	};

	// IOCtl
	enum
	{
		IOCTL_WRITEHCR  = 0x01,
		IOCTL_READHCR   = 0x02,
		IOCTL_RESETCARD = 0x04,
		IOCTL_SETCLK    = 0x06,
		IOCTL_SENDCMD   = 0x07,
		IOCTL_GETSTATUS = 0x0B,
		IOCTL_GETOCR    = 0x0C,
	};

	// IOCtlV
	enum
	{
		IOCTLV_SENDCMD = 0x07,
	};

	// ExecuteCommand
	enum
	{
		RET_OK,
		RET_FAIL,
		RET_EVENT_REGISTER, // internal state only - not actually returned
		RET_EVENT_UNREGISTER
	};

	// Status
	enum
	{
		CARD_NOT_EXIST   = 0,
		CARD_INSERTED    = 1,
		CARD_INITIALIZED = 0x10000,
	};

	// Commands
	enum
	{
		GO_IDLE_STATE        = 0x00,
		ALL_SEND_CID         = 0x02,
		SEND_RELATIVE_ADDR   = 0x03,
		SELECT_CARD          = 0x07,
		SEND_IF_COND         = 0x08,
		SEND_CSD             = 0x09,
		SEND_CID             = 0x0A,
		SEND_STATUS          = 0x0D,
		SET_BLOCKLEN         = 0x10,
		READ_MULTIPLE_BLOCK  = 0x12,
		WRITE_MULTIPLE_BLOCK = 0x19,
		APP_CMD_NEXT         = 0x37,

		ACMD_SETBUSWIDTH     = 0x06,
		ACMD_SENDOPCOND      = 0x29,
		ACMD_SENDSCR         = 0x33,

		EVENT_REGISTER       = 0x40,
		EVENT_UNREGISTER     = 0x41,
	};

	enum EventType
	{
		EVENT_NONE = 0,
		EVENT_INSERT,
		EVENT_REMOVE,
		// from unregister, i think it is just meant to be invalid
		EVENT_INVALID = 0xc210000
	};

	// TODO do we need more than one?
	struct Event
	{
		EventType type;
		u32 addr;
		Event()
			: type(EVENT_NONE)
			, addr()
		{}
	} m_event;

	u32 m_Status;
	u32 m_BlockLength;
	u32 m_BusWidth;

	u32 m_Registers[0x200/4];

	File::IOFile m_Card;

	u32 ExecuteCommand(u32 BufferIn, u32 BufferInSize,
		u32 BufferIn2, u32 BufferInSize2,
		u32 _BufferOut, u32 BufferOutSize);
	void OpenInternal();
};
