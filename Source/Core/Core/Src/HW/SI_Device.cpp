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
#include "SI_DeviceGCController.h"
#include "SI_DeviceGBA.h"

//////////////////////////////////////////////////////////////////////////
// --- interface ISIDevice ---
//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
// --- class CSIDummy ---
//////////////////////////////////////////////////////////////////////////
// Just a dummy that logs reads and writes
// to be used for SI devices we haven't emulated
// and hopefully as an "emtpy" device
class CSIDevice_Dummy : public ISIDevice
{
public:
	CSIDevice_Dummy(int _iDeviceNumber) :
	  ISIDevice(_iDeviceNumber)
	{}

	virtual ~CSIDevice_Dummy(){}

	int RunBuffer(u8* _pBuffer, int _iLength)
	{
		// Debug logging
		ISIDevice::RunBuffer(_pBuffer, _iLength);

		reinterpret_cast<u32*>(_pBuffer)[0] = 0x00000000;
		return 4;
	}

	bool GetData(u32& _Hi, u32& _Low)	{INFO_LOG(SERIALINTERFACE, "SI DUMMY %i GetData", this->m_iDeviceNumber); return false;}
	void SendCommand(u32 _Cmd, u8 _Poll){INFO_LOG(SERIALINTERFACE, "SI DUMMY %i SendCommand: %08x", this->m_iDeviceNumber, _Cmd);}
};

//////////////////////////////////////////////////////////////////////////
// F A C T O R Y /////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

ISIDevice* SIDevice_Create(TSIDevices _SIDevice, int _iDeviceNumber)
{
	switch(_SIDevice)
	{
	case SI_DUMMY:
		return new CSIDevice_Dummy(_iDeviceNumber);
		break;

	case SI_GC_CONTROLLER:
		return new CSIDevice_GCController(_iDeviceNumber);
		break;

	case SI_GBA:
		return new CSIDevice_GBA(_iDeviceNumber);
		break;

	default:
		return new CSIDevice_Dummy(_iDeviceNumber);
		break;
	}

	return NULL;
}
