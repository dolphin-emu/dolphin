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

#include "SI_Device.h"
#include "SI_DeviceGBA.h"

//////////////////////////////////////////////////////////////////////////
// --- GameBoy Advance ---
//////////////////////////////////////////////////////////////////////////

CSIDevice_GBA::CSIDevice_GBA(int _iDeviceNumber) :
	ISIDevice(_iDeviceNumber)
{
}

int CSIDevice_GBA::RunBuffer(u8* _pBuffer, int _iLength)
{
	// for debug logging only
	ISIDevice::RunBuffer(_pBuffer, _iLength);

	int iPosition = 0;
	while(iPosition < _iLength)
	{	
		// read the command
		EBufferCommands command = static_cast<EBufferCommands>(_pBuffer[iPosition ^ 3]);
		iPosition++;

		// handle it
		switch(command)
		{
			// NOTE: All "Send/Recieve" mentioned here is from dolphin's perspective,
			// NOT the GBA's
			// This means "Send"s are seen as CMDs to the GBA, and are therefor IMPORTANT :p
			// Also this means that we can randomly fill recieve bits and try for a fake gba...
			// for example, the ORd bits in RESET and STATUS are just some values to play with
		case CMD_RESET:
			{
				//Device Reset
				//Send 0xFF
				//Recieve 0x00
				//Recieve 0x04
				//Recieve from lower 8bits of SIOSTAT register

				*(u32*)&_pBuffer[0] = SI_GBA|2;
				iPosition = _iLength; // break the while loop
				LOG(SERIALINTERFACE, "GBA CMD_RESET");
			}
			break;
		case CMD_STATUS:
			{
				//Type/Status Data Request
				//Send 0x00
				//Recieve 0x00
				//Recieve 0x04
				//Recieve from lower 8bits of SIOSTAT register

				*(u32*)&_pBuffer[0] = SI_GBA|8;
				iPosition = _iLength; // break the while loop
				LOG(SERIALINTERFACE, "GBA CMD_STATUS");
			}
			break;
		case CMD_WRITE:
			{
				//GBA Data Write (to GBA)
				//Send 0x15
				//Send to Lower 8bits of JOY_RECV_L
				//Send to Upper 8bits of JOY_RECV_L
				//Send to Lower 8bits of JOY_RECV_H
				//Send to Upper 8bits of JOY_RECV_H
				//Receive from lower 8bits of SIOSTAT register

				LOG(SERIALINTERFACE, "GBA CMD_WRITE");
			}
			break;
		case CMD_READ:
			{
				//GBA Data Read (from GBA)
				//Send 0x14
				//Receive from Lower 8bits of JOY_TRANS_L
				//Receive from Upper 8bits of JOY_TRANS_L
				//Receive from Lower 8bits of JOY_TRANS_H
				//Receive from Upper 8bits of JOY_TRANS_H
				//Receive from lower 8bits of SIOSTAT register

				LOG(SERIALINTERFACE, "GBA CMD_READ");
			}
			break;
		default:
			{
				LOG(SERIALINTERFACE, "unknown GBA command     (0x%x)", command);
				iPosition = _iLength;
			}			
			break;
		}
	}

	return iPosition;
}

//////////////////////////////////////////////////////////////////////////
// GetData
//////////////////////////////////////////////////////////////////////////
bool 
CSIDevice_GBA::GetData(u32& _Hi, u32& _Low)
{
	LOG(SERIALINTERFACE, "GBA GetData Hi: 0x%x Low: 0x%x", _Hi, _Low);

	return true;
}

//////////////////////////////////////////////////////////////////////////
// SendCommand
//////////////////////////////////////////////////////////////////////////
void
CSIDevice_GBA::SendCommand(u32 _Cmd)
{
	LOG(SERIALINTERFACE, "GBA SendCommand: (0x%x)", _Cmd);
}
