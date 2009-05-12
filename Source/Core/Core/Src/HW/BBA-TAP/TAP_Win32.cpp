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

#include "../Memmap.h"
#include "../EXI_Device.h"
#include "../EXI_DeviceEthernet.h"

bool CEXIETHERNET::deactivate()
{
	return true;
	// TODO: Actually deactivate
}
bool CEXIETHERNET::isActivated()
{ 
	return false;
	//TODO: Never Activated Yet!
}

bool CEXIETHERNET::activate() {
	if (isActivated())
		return true;
	else
		return false;
	//TODO: Activate Device!
}
bool CEXIETHERNET::sendPacket(u8 *etherpckt, int size) 
{
	DEBUGPRINT( "Packet: 0x");
	for(int a = 0; a < size; ++a)
	{
		DEBUGPRINT( "%02X", etherpckt[a]);
	}
	DEBUGPRINT( " : Size: %d\n", size);
	//fwrite(etherpckt, size, size, raw_socket);
	/*DWORD numBytesWrit;
	OVERLAPPED overlap;
	ZERO_OBJECT(overlap);
	//overlap.hEvent = mHRecvEvent;
	TGLE(WriteFile(mHAdapter, etherpckt, size, &numBytesWrit, &overlap));
	if(numBytesWrit != size) 
	{
		DEGUB("BBA sendPacket %i only got %i bytes sent!\n", size, numBytesWrit);
		FAIL(UE_BBA_ERROR);
	}*/
	recordSendComplete();
	//exit(0);
	return true;
}
bool CEXIETHERNET::handleRecvdPacket() 
{
	DEBUGPRINT(" Handle received Packet!\n");
	exit(0);
}
