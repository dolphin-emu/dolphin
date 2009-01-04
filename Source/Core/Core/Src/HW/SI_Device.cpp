// Copyright (C) 2003-2008 Dolphin Project.

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

// =====================================================================================================
// --- base class ---
// =====================================================================================================

int ISIDevice::RunBuffer(u8* _pBuffer, int _iLength)
{
#ifdef _DEBUG
	LOG(SERIALINTERFACE, "Send Data Device(%i) - Length(%i)   ", ISIDevice::m_iDeviceNumber,_iLength);

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
			LOG(SERIALINTERFACE, szTemp);
			szTemp[0] = '\0';
		}		
	}
	LOG(SERIALINTERFACE, szTemp);		
#endif
	return 0;
};

// =====================================================================================================
// --- dummy device ---
// =====================================================================================================

CSIDevice_Dummy::CSIDevice_Dummy(int _iDeviceNumber) :
	ISIDevice(_iDeviceNumber)
{}

int CSIDevice_Dummy::RunBuffer(u8* _pBuffer, int _iLength)
{
	reinterpret_cast<u32*>(_pBuffer)[0] = 0x00000000; // no device
	return 4;
}

bool CSIDevice_Dummy::GetData(u32& _Hi, u32& _Low)
{
	return false;
}

void CSIDevice_Dummy::SendCommand(u32 _Cmd)
{
}
