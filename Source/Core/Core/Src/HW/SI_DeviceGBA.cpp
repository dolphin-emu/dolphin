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
#include "GBAPipe.h"

//////////////////////////////////////////////////////////////////////////
// --- GameBoy Advance ---
//////////////////////////////////////////////////////////////////////////

CSIDevice_GBA::CSIDevice_GBA(int _iDeviceNumber) :
	ISIDevice(_iDeviceNumber)
{
	GBAPipe::StartServer();
	js.U16 = 0;
}

CSIDevice_GBA::~CSIDevice_GBA()
{
	GBAPipe::Stop();
}

int CSIDevice_GBA::RunBuffer(u8* _pBuffer, int _iLength)
{
	// for debug logging only
	ISIDevice::RunBuffer(_pBuffer, _iLength);

	int iPosition = 0;

	// read the command
	EBufferCommands command = static_cast<EBufferCommands>(_pBuffer[3]);
	iPosition++;

	// handle it
	switch(command)
	{
		// NOTE: All "Send/Recieve" mentioned here is from dolphin's perspective,
		// NOT the GBA's
		// This means the first "Send"s are seen as CMDs to the GBA, and are therefor IMPORTANT :p
		// Also this means that we can randomly fill recieve bits and try for a fake gba...
		// (try playing with the fake joystat)
		// It seems like JOYSTAT is polled all the time, it's value may or may not matter
	case CMD_RESET:
		{
			//Send 0xFF
			GBAPipe::Write(CMD_RESET);
			//Recieve 0x00
			//Recieve 0x04
			//Recieve from lower 8bits of JOYSTAT register
			for (;iPosition < _iLength; ++iPosition)
				GBAPipe::Read(_pBuffer[iPosition]);

			js.U16 = 0; // FAKE
			js.stat_send = 1; // FAKE
			*(u32*)&_pBuffer[0] |= SI_GBA | js.U16; // hax for now
			WARN_LOG(SERIALINTERFACE, "GBA %i CMD_RESET", this->m_iDeviceNumber);
		}
		break;
	case CMD_STATUS:
		{
			//Send 0x00
			GBAPipe::Write(CMD_STATUS);
			//Recieve 0x00
			//Recieve 0x04
			//Recieve from lower 8bits of JOYSTAT register
			for (;iPosition < _iLength; ++iPosition)
				GBAPipe::Read(_pBuffer[iPosition]);

			js.U16 = 0; // FAKE
			js.stat_rec = 1; // FAKE
			*(u32*)&_pBuffer[0] |= SI_GBA | js.U16; // hax for now
			WARN_LOG(SERIALINTERFACE, "GBA %i CMD_STATUS", this->m_iDeviceNumber);
		}
		break;
	// Probably read and write belong in getdata and sendcommand
	case CMD_WRITE:
		{
			//Send 0x15
			GBAPipe::Write(CMD_WRITE);
			//Send to Lower 8bits of JOY_RECV_L
			//Send to Upper 8bits of JOY_RECV_L
			//Send to Lower 8bits of JOY_RECV_H
			//Send to Upper 8bits of JOY_RECV_H
			for (;iPosition < _iLength-1; ++iPosition)
				GBAPipe::Write(_pBuffer[iPosition]);
			//Receive from lower 8bits of JOYSTAT register
			GBAPipe::Read(_pBuffer[++iPosition]);

			WARN_LOG(SERIALINTERFACE, "GBA %i CMD_WRITE", this->m_iDeviceNumber);
		}
		break;
	case CMD_READ:
		{
			//Send 0x14
			GBAPipe::Write(CMD_READ);
			//Receive from Lower 8bits of JOY_TRANS_L
			//Receive from Upper 8bits of JOY_TRANS_L
			//Receive from Lower 8bits of JOY_TRANS_H
			//Receive from Upper 8bits of JOY_TRANS_H
			//Receive from lower 8bits of JOYSTAT register
			for (;iPosition < _iLength; ++iPosition)
				GBAPipe::Read(_pBuffer[iPosition]);

			WARN_LOG(SERIALINTERFACE, "GBA %i CMD_READ", this->m_iDeviceNumber);;
		}
		break;
	default:
		{
			WARN_LOG(SERIALINTERFACE, "GBA %i CMD_UNKNOWN     (0x%x)", this->m_iDeviceNumber, command);
		}
		break;
	}
	INFO_LOG(SERIALINTERFACE, "GBA buffer %08x",*(u32*)&_pBuffer[0]);

	return iPosition;
}

//////////////////////////////////////////////////////////////////////////
// GetData
//////////////////////////////////////////////////////////////////////////
bool 
CSIDevice_GBA::GetData(u32& _Hi, u32& _Low)
{
	INFO_LOG(SERIALINTERFACE, "GBA %i GetData Hi: 0x%08x Low: 0x%08x", this->m_iDeviceNumber, _Hi, _Low);

	return true;
}

//////////////////////////////////////////////////////////////////////////
// SendCommand
//////////////////////////////////////////////////////////////////////////
void
CSIDevice_GBA::SendCommand(u32 _Cmd, u8 _Poll)
{
	INFO_LOG(SERIALINTERFACE, "GBA %i SendCommand: (0x%08x)", this->m_iDeviceNumber, _Cmd);
}
