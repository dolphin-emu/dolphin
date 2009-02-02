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
#include "SI_DeviceGBAController.h"
#include "../PluginManager.h"

#include "EXI_Device.h"

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
		case CMD_RESET:
			{
				*(u32*)&_pBuffer[0] = SI_GBA;
				iPosition = _iLength; // break the while loop
				LOG(SERIALINTERFACE, "SI-GBA CMD_RESET");
			}
			break;
		case CMD_STATUS: //Same behavior as CMD_RESET: send 0x0004, then lower 8 bits of SIOSTAT
			{
				*(u32*)&_pBuffer[0] = SI_GBA;
				iPosition = _iLength; // break the while loop
				LOG(SERIALINTERFACE, "SI-GBA CMD_STATUS");
			}
			break;
		case CMD_WRITE:
			{
				LOG(SERIALINTERFACE, "SI-GBA CMD_WRITE");
			}
			break;
		case CMD_READ:
			{
				LOG(SERIALINTERFACE, "SI-GBA CMD_READ");
			}
			break;
		default:
			{
				LOG(SERIALINTERFACE, "unknown SI-GBA command     (0x%x)", command);
				//PanicAlert("SI: Unknown command");
				iPosition = _iLength;
			}			
			break;
		}
	}

	return iPosition;
}

// __________________________________________________________________________________________________
// GetData
//
bool 
CSIDevice_GBA::GetData(u32& _Hi, u32& _Low)
{
	LOG(SERIALINTERFACE, "SI-GBA GetData Hi: (0x%x) Low: (0x%x)", _Hi, _Low);

	return true;
}

// __________________________________________________________________________________________________
// SendCommand
//	
void
CSIDevice_GBA::SendCommand(u32 _Cmd)
{
	LOG(SERIALINTERFACE, "SI-GBA SendCommand: (0x%x)", _Cmd);
}
