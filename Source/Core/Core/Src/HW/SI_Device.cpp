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
#include "SI_DeviceGCController.h"
#include "SI_DeviceGBA.h"
#include "SI_DeviceAMBaseboard.h"


// --- interface ISIDevice ---
int ISIDevice::RunBuffer(u8* _pBuffer, int _iLength)
{
#ifdef _DEBUG
	DEBUG_LOG(SERIALINTERFACE, "Send Data Device(%i) - Length(%i)   ", ISIDevice::m_iDeviceNumber,_iLength);

	char szTemp[256] = "";
	int num = 0;
	while(num < _iLength)
	{
		char szTemp2[128] = "";
		sprintf(szTemp2, "0x%02x ", _pBuffer[num^3]);
		strcat(szTemp, szTemp2);
		num++;

		if ((num % 8) == 0)
		{
			DEBUG_LOG(SERIALINTERFACE, szTemp);
			szTemp[0] = '\0';
		}
	}
	DEBUG_LOG(SERIALINTERFACE, szTemp);
#endif
	return 0;
};


// Stub class for saying nothing is attached, and not having to deal with null pointers :)
class CSIDevice_Null : public ISIDevice
{
public:
	CSIDevice_Null(int _iDeviceNumber) : ISIDevice(_iDeviceNumber) {}
	virtual ~CSIDevice_Null() {}

	int RunBuffer(u8* _pBuffer, int _iLength) {
		reinterpret_cast<u32*>(_pBuffer)[0] = SI_ERROR_NO_RESPONSE;
		return 4;
	}
	bool GetData(u32& _Hi, u32& _Low) {
		_Hi = 0x80000000;
		return true;
	}
	void SendCommand(u32 _Cmd, u8 _Poll) {}
};


// F A C T O R Y 
ISIDevice* SIDevice_Create(TSIDevices _SIDevice, int _iDeviceNumber)
{
	switch(_SIDevice)
	{
	case SI_GC_CONTROLLER:
		return new CSIDevice_GCController(_iDeviceNumber);
		break;

	case SI_GBA:
		return new CSIDevice_GBA(_iDeviceNumber);
		break;

	case SI_AM_BASEBOARD:
		return new CSIDevice_AMBaseboard(_iDeviceNumber);
		break;

	case SI_NONE:
	default:
		return new CSIDevice_Null(_iDeviceNumber);
		break;
	}
}
