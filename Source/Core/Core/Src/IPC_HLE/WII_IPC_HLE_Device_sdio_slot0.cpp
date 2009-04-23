// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Common.h"

#include "SDCardUtil.h"

#include "WII_IPC_HLE_Device_sdio_slot0.h"

#include "../HW/CPU.h"
#include "../HW/Memmap.h"
#include "../Core.h"

CWII_IPC_HLE_Device_sdio_slot0::CWII_IPC_HLE_Device_sdio_slot0(u32 _DeviceID, const std::string& _rDeviceName)
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
	m_Card = NULL;
	m_Status = CARD_INSERTED;
	m_BlockLength = 0;
	m_BusWidth = 0;
	// Clear the whole SD Host Control Register
	//Memory::Memset(SDIO_BASE, 0, 0x100);
}

CWII_IPC_HLE_Device_sdio_slot0::~CWII_IPC_HLE_Device_sdio_slot0()
{

}

bool CWII_IPC_HLE_Device_sdio_slot0::Open(u32 _CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_SD, "Open");

	char filename[16] = "sd.raw";
	m_Card = fopen(filename, "r+b");
	if(!m_Card)
	{
		WARN_LOG(WII_IPC_SD, "Failed to open SD Card image, trying to create a new 128MB image...");
		if (SDCardCreate(128, filename))
		{
			WARN_LOG(WII_IPC_SD, "Successfully created %s", filename);
			m_Card = fopen(filename, "r+b");
		}
		if(!m_Card)
		{
			ERROR_LOG(WII_IPC_SD, "Could not open SD Card image or create a new one, are you running from a read-only directory?");
		}
	}

    Memory::Write_U32(GetDeviceID(), _CommandAddress + 0x4);
    return true;
}

bool CWII_IPC_HLE_Device_sdio_slot0::Close(u32 _CommandAddress)
{
	INFO_LOG(WII_IPC_SD, "Close");

	if(m_Card)
		fclose(m_Card);

    Memory::Write_U32(0, _CommandAddress + 0x4);
    return true;
}

// The front SD slot
bool CWII_IPC_HLE_Device_sdio_slot0::IOCtl(u32 _CommandAddress) 
{
	u32 Cmd = Memory::Read_U32(_CommandAddress + 0xC);

	u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
    u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
    u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);
	
	// As a safety precaution we fill the out buffer with zeros to avoid
	// returning nonsense values
	Memory::Memset(BufferOut, 0, BufferOutSize);
	
	u32 ReturnValue = 0;
	switch (Cmd) {
	case IOCTL_WRITEHCR:
		{
		u32 reg = Memory::Read_U32(BufferIn);
		u32 val = Memory::Read_U32(BufferIn + 16);

		DEBUG_LOG(WII_IPC_SD, "IOCTL_WRITEHCR 0x%08x - 0x%08x", reg, val);

		if ((reg == HCR_CLOCKCONTROL) && (val & 1))
		{
			// Clock is set to oscilliate, enable bit 1 to say it's stable
			Memory::Write_U32(val | 2, SDIO_BASE + reg);
		}
		else if ((reg == HCR_SOFTWARERESET) && val)
		{
			// When a reset is specified, the register gets cleared
			Memory::Write_U32(0, SDIO_BASE + reg);
		}
		else
		{
			// Default to just storing the new value
			Memory::Write_U32(val, SDIO_BASE + reg);
		}
		}
		break;

	case IOCTL_READHCR:
		{
		u32 reg = Memory::Read_U32(BufferIn);
		u32 val = Memory::Read_U32(SDIO_BASE + reg);

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
		INFO_LOG(WII_IPC_SD, "IOCTL_SENDCMD 0x%08x", Memory::Read_U32(BufferIn));
		ReturnValue = ExecuteCommand(BufferIn, BufferInSize, 0, 0, BufferOut, BufferOutSize);	
		break;

	case IOCTL_GETSTATUS:
		INFO_LOG(WII_IPC_SD, "IOCTL_GETSTATUS. Replying that SD card is %s%s",
			(m_Status & CARD_INSERTED) ? "inserted" : "",
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

// 	INFO_LOG(WII_IPC_SD, "InBuffer");
// 	DumpCommands(BufferIn, BufferInSize / 4, LogTypes::WII_IPC_SD);
// 	INFO_LOG(WII_IPC_SD, "OutBuffer");
// 	DumpCommands(BufferOut, BufferOutSize/4, LogTypes::WII_IPC_SD);

	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}

bool CWII_IPC_HLE_Device_sdio_slot0::IOCtlV(u32 _CommandAddress) 
{
	// PPC sending commands

	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	// Prepare the out buffer(s) with zeros as a safety precaution
	// to avoid returning bad values
	for(u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; i++)
	{
		Memory::Memset(CommandBuffer.PayloadBuffer[i].m_Address, 0,
			CommandBuffer.PayloadBuffer[i].m_Size);
	}

	u32 ReturnValue = 0;
	switch(CommandBuffer.Parameter) {	
	case IOCTLV_SENDCMD:
		INFO_LOG(WII_IPC_SD, "IOCTLV_SENDCMD 0x%08x", Memory::Read_U32(CommandBuffer.InBuffer[0].m_Address));
		ReturnValue = ExecuteCommand(
			CommandBuffer.InBuffer[0].m_Address, CommandBuffer.InBuffer[0].m_Size,
			CommandBuffer.InBuffer[1].m_Address, CommandBuffer.InBuffer[1].m_Size,
			CommandBuffer.PayloadBuffer[0].m_Address, CommandBuffer.PayloadBuffer[0].m_Size);
		break;

	default:
		ERROR_LOG(WII_IPC_SD, "unknown SD IOCtlV command 0x%08x", CommandBuffer.Parameter);
		break;
	}

	//DumpAsync(CommandBuffer.BufferVector, _CommandAddress, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer, LogTypes::WII_IPC_SD);

	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}

u32 CWII_IPC_HLE_Device_sdio_slot0::ExecuteCommand(u32 _BufferIn, u32 _BufferInSize,
												   u32 _rwBuffer, u32 _rwBufferSize,
												   u32 _BufferOut, u32 _BufferOutSize)
{
	// The game will send us a SendCMD with this information. To be able to read and write
	// to a file we need to prepare a 0x10 byte output buffer as response.
	struct Request {
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
	req.isDMA	= Memory::Read_U32(_BufferIn + 28);
	req.pad0	= Memory::Read_U32(_BufferIn + 32);

	// Note: req.addr is the virtual address of _rwBuffer


	u32 rwFail = 0;

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
		//	CSD: 0xff928040 0xc93efbcf 0x325f5a83 0x00002600

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

			if (fseek(m_Card, req.arg, SEEK_SET) != 0)
				ERROR_LOG(WII_IPC_SD, "fseek failed WTF");

			u8* buffer = new u8[size];

			size_t nRead = fread(buffer, req.bsize, req.blocks, m_Card);
			if (nRead == req.blocks)
			{
				u32 i;
				for (i = 0; i < size; ++i)
				{
					Memory::Write_U8((u8)buffer[i], req.addr++);
				}
				DEBUG_LOG(WII_IPC_SD, "outbuffer size %i got %i", _rwBufferSize, i);
			}
			else
			{
				ERROR_LOG(WII_IPC_SD, "Read Failed - read %x, error %i, eof? %i",
					nRead, ferror(m_Card), feof(m_Card));
				rwFail = 1;
			}

			delete [] buffer;
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

		if (m_Card)
		{
			u32 size = req.bsize * req.blocks;

			if (fseek(m_Card, req.arg, SEEK_SET) != 0)
				ERROR_LOG(WII_IPC_SD, "fseek failed WTF");

			u8* buffer = new u8[size];

			for (u32 i = 0; i < size; ++i)
			{
				buffer[i] = Memory::Read_U8(req.addr++);
			}

			size_t nWritten = fwrite(buffer, req.bsize, req.blocks, m_Card);
			if (nWritten != req.blocks)
			{
				ERROR_LOG(WII_IPC_SD, "Write Failed - wrote %x, error %i, eof? %i",
					nWritten, ferror(m_Card), feof(m_Card));
				rwFail = 1;
			}

			delete [] buffer;
		}
		}
		Memory::Write_U32(0x900, _BufferOut);
		break;

	case CRAZY_BIGN:
		DEBUG_LOG(WII_IPC_SD, "CMD64, wtf");
		// <svpe> shuffle2_: try returning -4 for cmd x'40.
		Memory::Write_U32(-0x4, _BufferOut);
		break;

	default:
		ERROR_LOG(WII_IPC_SD, "Unknown SD command 0x%08x", req.command);
		break;
	}

    return rwFail;
}
