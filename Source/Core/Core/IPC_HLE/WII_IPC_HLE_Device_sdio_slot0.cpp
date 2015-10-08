// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/SDCardUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/CPU.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_sdio_slot0.h"

void CWII_IPC_HLE_Device_sdio_slot0::EnqueueReply(u32 CommandAddress, u32 ReturnValue)
{
	// IOS seems to write back the command that was responded to, this class does not
	// overwrite the command so it is safe to read.
	Memory::Write_U32(Memory::Read_U32(CommandAddress), CommandAddress + 8);
	// The original hardware overwrites the command type with the async reply type.
	Memory::Write_U32(IPC_REP_ASYNC, CommandAddress);

	Memory::Write_U32(ReturnValue, CommandAddress + 4);

	WII_IPC_HLE_Interface::EnqueueReply(CommandAddress);
}

CWII_IPC_HLE_Device_sdio_slot0::CWII_IPC_HLE_Device_sdio_slot0(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
	, m_Status(CARD_NOT_EXIST)
	, m_BlockLength(0)
	, m_BusWidth(0)
	, m_Card(nullptr)
{}

void CWII_IPC_HLE_Device_sdio_slot0::DoState(PointerWrap& p)
{
	DoStateShared(p);
	if (p.GetMode() == PointerWrap::MODE_READ)
	{
		OpenInternal();
	}
	p.Do(m_Status);
	p.Do(m_BlockLength);
	p.Do(m_BusWidth);
	p.Do(m_Registers);
}

void CWII_IPC_HLE_Device_sdio_slot0::EventNotify()
{
	if ((SConfig::GetInstance().m_WiiSDCard && m_event.type == EVENT_INSERT) ||
		(!SConfig::GetInstance().m_WiiSDCard && m_event.type == EVENT_REMOVE))
	{
		EnqueueReply(m_event.addr, m_event.type);
		m_event.addr = 0;
		m_event.type = EVENT_NONE;
	}
}

void CWII_IPC_HLE_Device_sdio_slot0::OpenInternal()
{
	const std::string filename = File::GetUserPath(D_WIIROOT_IDX) + "/sd.raw";
	m_Card.Open(filename, "r+b");
	if (m_Card)
		return;
	
	WARN_LOG(WII_IPC_SD, "Failed to open SD Card image, trying read-only access...");
	m_Card.Open(filename, "rb");
	if (m_Card)
	{
		SConfig::GetInstance().bEnableMemcardSdWriting = false;
		return;
	}
	
	WARN_LOG(WII_IPC_SD, "Failed to open SD Card image, trying to create a new 128MB image...");
	if (SDCardCreate(128, filename))
	{
		WARN_LOG(WII_IPC_SD, "Successfully created %s", filename.c_str());
		m_Card.Open(filename, "r+b");
	}
	if (!m_Card)
	{
		ERROR_LOG(WII_IPC_SD, "Could not open SD Card image or create a new one, are you running from a read-only directory?");
	}
	
}

IPCCommandResult CWII_IPC_HLE_Device_sdio_slot0::Open(u32 _CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_SD, "Open");

	OpenInternal();

	Memory::Write_U32(GetDeviceID(), _CommandAddress + 0x4);
	memset(m_Registers, 0, sizeof(m_Registers));
	m_Active = true;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_sdio_slot0::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_SD, "Close");

	m_Card.Close();
	m_BlockLength = 0;
	m_BusWidth = 0;

	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 0x4);
	m_Active = false;
	return GetDefaultReply();
}

// The front SD slot
IPCCommandResult CWII_IPC_HLE_Device_sdio_slot0::IOCtl(u32 _CommandAddress)
{
	u32 Cmd = Memory::Read_U32(_CommandAddress + 0xC);

	u32 BufferIn      = Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize  = Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut     = Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

	// As a safety precaution we fill the out buffer with zeros to avoid
	// returning nonsense values
	Memory::Memset(BufferOut, 0, BufferOutSize);

	u32 ReturnValue = 0;
	switch (Cmd)
	{
	case IOCTL_WRITEHCR:
		{
		u32 reg = Memory::Read_U32(BufferIn);
		u32 val = Memory::Read_U32(BufferIn + 16);

		DEBUG_LOG(WII_IPC_SD, "IOCTL_WRITEHCR 0x%08x - 0x%08x", reg, val);

		if (reg >= 0x200)
		{
			DEBUG_LOG(WII_IPC_SD, "IOCTL_WRITEHCR out of range");
			break;
		}

		if ((reg == HCR_CLOCKCONTROL) && (val & 1))
		{
			// Clock is set to oscillate, enable bit 1 to say it's stable
			m_Registers[reg] = val | 2;
		}
		else if ((reg == HCR_SOFTWARERESET) && val)
		{
			// When a reset is specified, the register gets cleared
			m_Registers[reg] = 0;
		}
		else
		{
			// Default to just storing the new value
			m_Registers[reg] = val;
		}
		}
		break;

	case IOCTL_READHCR:
		{
		u32 reg = Memory::Read_U32(BufferIn);

		if (reg >= 0x200)
		{
			DEBUG_LOG(WII_IPC_SD, "IOCTL_READHCR out of range");
			break;
		}

		u32 val = m_Registers[reg];
		DEBUG_LOG(WII_IPC_SD, "IOCTL_READHCR 0x%08x - 0x%08x", reg, val);

		// Just reading the register
		Memory::Write_U32(val, BufferOut);
		}
		break;

	case IOCTL_RESETCARD:
		DEBUG_LOG(WII_IPC_SD, "IOCTL_RESETCARD");
		if (m_Card)
			m_Status |= CARD_INITIALIZED;
		// Returns 16bit RCA and 16bit 0s (meaning success)
		Memory::Write_U32(0x9f620000, BufferOut);
		break;

	case IOCTL_SETCLK:
		{
		DEBUG_LOG(WII_IPC_SD, "IOCTL_SETCLK");
		// libogc only sets it to 1 and makes sure the return isn't negative...
		// one half of the sdclk divisor: a power of two or zero.
		u32 clock = Memory::Read_U32(BufferIn);
		if (clock != 1)
			INFO_LOG(WII_IPC_SD, "Setting to %i, interesting", clock);
		}
		break;

	case IOCTL_SENDCMD:
		INFO_LOG(WII_IPC_SD, "IOCTL_SENDCMD %x IPC:%08x",
			Memory::Read_U32(BufferIn), _CommandAddress);
		ReturnValue = ExecuteCommand(BufferIn, BufferInSize, 0, 0, BufferOut, BufferOutSize);
		break;

	case IOCTL_GETSTATUS:
		if (SConfig::GetInstance().m_WiiSDCard)
			m_Status |= CARD_INSERTED;
		else
			m_Status = CARD_NOT_EXIST;
		INFO_LOG(WII_IPC_SD, "IOCTL_GETSTATUS. Replying that SD card is %s%s",
			(m_Status & CARD_INSERTED) ? "inserted" : "not present",
			(m_Status & CARD_INITIALIZED) ? " and initialized" : "");
		Memory::Write_U32(m_Status, BufferOut);
		break;

	case IOCTL_GETOCR:
		DEBUG_LOG(WII_IPC_SD, "IOCTL_GETOCR");
		Memory::Write_U32(0x80ff8000, BufferOut);
		break;

	default:
		ERROR_LOG(WII_IPC_SD, "Unknown SD IOCtl command (0x%08x)", Cmd);
		break;
	}

	// INFO_LOG(WII_IPC_SD, "InBuffer");
	// DumpCommands(BufferIn, BufferInSize / 4, LogTypes::WII_IPC_SD);
	// INFO_LOG(WII_IPC_SD, "OutBuffer");
	// DumpCommands(BufferOut, BufferOutSize/4, LogTypes::WII_IPC_SD);

	if (ReturnValue == RET_EVENT_REGISTER)
	{
		// async
		m_event.addr = _CommandAddress;
		Memory::Write_U32(0, _CommandAddress + 0x4);
		// Check if the condition is already true
		EventNotify();
		return GetNoReply();
	}
	else if (ReturnValue == RET_EVENT_UNREGISTER)
	{
		// release returns 0
		// unknown sd int
		// technically we do it out of order, oh well
		EnqueueReply(m_event.addr, EVENT_INVALID);
		m_event.addr = 0;
		m_event.type = EVENT_NONE;
		Memory::Write_U32(0, _CommandAddress + 0x4);
		return GetDefaultReply();
	}
	else
	{
		Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);
		return GetDefaultReply();
	}
}

IPCCommandResult CWII_IPC_HLE_Device_sdio_slot0::IOCtlV(u32 _CommandAddress)
{
	// PPC sending commands

	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	// Prepare the out buffer(s) with zeros as a safety precaution
	// to avoid returning bad values
	for (u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; i++)
	{
		Memory::Memset(CommandBuffer.PayloadBuffer[i].m_Address, 0,
			CommandBuffer.PayloadBuffer[i].m_Size);
	}

	u32 ReturnValue = 0;
	switch (CommandBuffer.Parameter)
	{
	case IOCTLV_SENDCMD:
		INFO_LOG(WII_IPC_SD, "IOCTLV_SENDCMD 0x%08x", Memory::Read_U32(CommandBuffer.InBuffer[0].m_Address));
		ReturnValue = ExecuteCommand(
			CommandBuffer.InBuffer[0].m_Address, CommandBuffer.InBuffer[0].m_Size,
			CommandBuffer.InBuffer[1].m_Address, CommandBuffer.InBuffer[1].m_Size,
			CommandBuffer.PayloadBuffer[0].m_Address, CommandBuffer.PayloadBuffer[0].m_Size);
		break;

	default:
		ERROR_LOG(WII_IPC_SD, "Unknown SD IOCtlV command 0x%08x", CommandBuffer.Parameter);
		break;
	}

	//DumpAsync(CommandBuffer.BufferVector, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer, LogTypes::WII_IPC_SD);

	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return GetDefaultReply();
}

u32 CWII_IPC_HLE_Device_sdio_slot0::ExecuteCommand(u32 _BufferIn, u32 _BufferInSize,
                                                   u32 _rwBuffer, u32 _rwBufferSize,
                                                   u32 _BufferOut, u32 _BufferOutSize)
{
	// The game will send us a SendCMD with this information. To be able to read and write
	// to a file we need to prepare a 0x10 byte output buffer as response.
	struct Request
	{
		u32 command;
		u32 type;
		u32 resp;
		u32 arg;
		u32 blocks;
		u32 bsize;
		u32 addr;
		u32 isDMA;
		u32 pad0;
	} req;

	req.command = Memory::Read_U32(_BufferIn + 0);
	req.type    = Memory::Read_U32(_BufferIn + 4);
	req.resp    = Memory::Read_U32(_BufferIn + 8);
	req.arg     = Memory::Read_U32(_BufferIn + 12);
	req.blocks  = Memory::Read_U32(_BufferIn + 16);
	req.bsize   = Memory::Read_U32(_BufferIn + 20);
	req.addr    = Memory::Read_U32(_BufferIn + 24);
	req.isDMA   = Memory::Read_U32(_BufferIn + 28);
	req.pad0    = Memory::Read_U32(_BufferIn + 32);

	// Note: req.addr is the virtual address of _rwBuffer


	u32 ret = RET_OK;

	switch (req.command)
	{
	case GO_IDLE_STATE:
		// libogc can use it during init..
		break;

	case SEND_RELATIVE_ADDR:
		// Technically RCA should be generated when asked and at power on...w/e :p
		Memory::Write_U32(0x9f62, _BufferOut);
		break;

	case SELECT_CARD:
		// This covers both select and deselect
		// Differentiate by checking if rca is set in req.arg
		// If it is, it's a select and return 0x700
		Memory::Write_U32((req.arg>>16) ? 0x700 : 0x900, _BufferOut);
		break;

	case SEND_IF_COND:
		// If the card can operate on the supplied voltage, the response echoes back the supply
		// voltage and the check pattern that were set in the command argument.
		Memory::Write_U32(req.arg, _BufferOut);
		break;

	case SEND_CSD:
		DEBUG_LOG(WII_IPC_SD, "SEND_CSD");
		// <WntrMute> shuffle2_, OCR: 0x80ff8000 CID: 0x38a00000 0x480032d5 0x3c608030 0x8803d420
		// CSD: 0xff928040 0xc93efbcf 0x325f5a83 0x00002600

		// Values used currently are from lpfaint99
		Memory::Write_U32(0x80168000, _BufferOut);
		Memory::Write_U32(0xa9ffffff, _BufferOut + 4);
		Memory::Write_U32(0x325b5a83, _BufferOut + 8);
		Memory::Write_U32(0x00002e00, _BufferOut + 12);
		break;

	case ALL_SEND_CID:
	case SEND_CID:
		DEBUG_LOG(WII_IPC_SD, "(ALL_)SEND_CID");
		Memory::Write_U32(0x80114d1c, _BufferOut);
		Memory::Write_U32(0x80080000, _BufferOut + 4);
		Memory::Write_U32(0x8007b520, _BufferOut + 8);
		Memory::Write_U32(0x80080000, _BufferOut + 12);
		break;

	case SET_BLOCKLEN:
		m_BlockLength = req.arg;
		Memory::Write_U32(0x900, _BufferOut);
		break;

	case APP_CMD_NEXT:
		// Next cmd is going to be ACMD_*
		Memory::Write_U32(0x920, _BufferOut);
		break;

	case ACMD_SETBUSWIDTH:
		// 0 = 1bit, 2 = 4bit
		m_BusWidth = (req.arg & 3);
		Memory::Write_U32(0x920, _BufferOut);
		break;

	case ACMD_SENDOPCOND:
		// Sends host capacity support information (HCS) and asks the accessed card to send
		// its operating condition register (OCR) content
		Memory::Write_U32(0x80ff8000, _BufferOut);
		break;

	case READ_MULTIPLE_BLOCK:
		{
		// Data address (req.arg) is in byte units in a Standard Capacity SD Memory Card
		// and in block (512 Byte) units in a High Capacity SD Memory Card.
		DEBUG_LOG(WII_IPC_SD, "%sRead %i Block(s) from 0x%08x bsize %i into 0x%08x!",
			req.isDMA ? "DMA " : "", req.blocks, req.arg, req.bsize, req.addr);

		if (m_Card)
		{
			u32 size = req.bsize * req.blocks;

			if (!m_Card.Seek(req.arg, SEEK_SET))
				ERROR_LOG(WII_IPC_SD, "Seek failed WTF");


			if (m_Card.ReadBytes(Memory::GetPointer(req.addr), size))
			{
				DEBUG_LOG(WII_IPC_SD, "Outbuffer size %i got %i", _rwBufferSize, size);
			}
			else
			{
				ERROR_LOG(WII_IPC_SD, "Read Failed - error: %i, eof: %i",
					ferror(m_Card.GetHandle()), feof(m_Card.GetHandle()));
				ret = RET_FAIL;
			}
		}
		}
		Memory::Write_U32(0x900, _BufferOut);
		break;

	case WRITE_MULTIPLE_BLOCK:
		{
		// Data address (req.arg) is in byte units in a Standard Capacity SD Memory Card
		// and in block (512 Byte) units in a High Capacity SD Memory Card.
		DEBUG_LOG(WII_IPC_SD, "%sWrite %i Block(s) from 0x%08x bsize %i to offset 0x%08x!",
			req.isDMA ? "DMA " : "", req.blocks, req.addr, req.bsize, req.arg);

		if (m_Card && SConfig::GetInstance().bEnableMemcardSdWriting)
		{
			u32 size = req.bsize * req.blocks;

			if (!m_Card.Seek(req.arg, SEEK_SET))
				ERROR_LOG(WII_IPC_SD, "fseeko failed WTF");

			if (!m_Card.WriteBytes(Memory::GetPointer(req.addr), size))
			{
				ERROR_LOG(WII_IPC_SD, "Write Failed - error: %i, eof: %i",
					ferror(m_Card.GetHandle()), feof(m_Card.GetHandle()));
				ret = RET_FAIL;
			}
		}
		}
		Memory::Write_U32(0x900, _BufferOut);
		break;

	case EVENT_REGISTER: // async
		DEBUG_LOG(WII_IPC_SD, "Register event %x", req.arg);
		m_event.type = (EventType)req.arg;
		ret = RET_EVENT_REGISTER;
		break;

	case EVENT_UNREGISTER: // synchronous
		DEBUG_LOG(WII_IPC_SD, "Unregister event %x", req.arg);
		m_event.type = (EventType)req.arg;
		ret = RET_EVENT_UNREGISTER;
		break;

	default:
		ERROR_LOG(WII_IPC_SD, "Unknown SD command 0x%08x", req.command);
		break;
	}

	return ret;
}
