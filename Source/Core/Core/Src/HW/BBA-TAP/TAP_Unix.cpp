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
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <stropts.h>
	#include <stdio.h>
	#include <fcntl.h>
	#include <sys/ioctl.h>
	#include <net/if.h>  
	#include <linux/if_tun.h>
	int fd = -1;
bool CEXIETHERNET::deactivate()
{
	close(fd);
	fd = -1;
	return true;
}
bool CEXIETHERNET::isActivated()
{ 
	return fd != -1 ? true : false;
}

bool CEXIETHERNET::activate() {
	if(isActivated())
		return true;
	if( (fd = open("/dev/net/tun", O_RDWR)) < 0)
	{
		DEBUGPRINT("Couldn't Open device\n");
		return false;
	}
	struct ifreq ifr;
	int err;
	
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP;
	
	strncpy(ifr.ifr_name, "Dolphin", IFNAMSIZ);
	
	if( (err = ioctl(fd, TUNSETIFF, (void*) &ifr)) < 0)
	{
		close(fd);
		fd = -1;
		DEBUGPRINT(" Error with IOCTL: 0x%X\n", err);
		return false;
	}
	DEBUGPRINT("Returned Socket name is: %s\n", ifr.ifr_name);
	return true;
	
}
bool CEXIETHERNET::startRecv() {
	DEBUGPRINT("Start Receive!\n");
	exit(0);
	/*if(!isActivated())
		return false;// Should actually be an assert
	DEBUGPRINT("startRecv... ");
	if(mWaiting) {
		DEBUGPRINT("already waiting\n");
		return true;
	}
	DWORD BytesRead = 0;
	DWORD *Buffer = (DWORD *)malloc(2048); // Should be enough
	DWORD res = ReadFile(mHAdapter, Buffer, BytesRead,
		&mRecvBufferLength, &mReadOverlapped);
	mRecvBuffer.write(BytesRead, Buffer);
	free(Buffer);
	if(res) {	//Operation completed immediately
		DEBUGPRINT("completed, res %i\n", res);
		mWaiting = true;
	} else {
		res = GetLastError();
		if (res == ERROR_IO_PENDING) {	//'s ok :)
			DEBUGPRINT("pending\n");
			//WaitCallback will be called
			mWaiting = true;
		}	else {	//error occurred
			return false;
		}
	}
	return true;*/
}
bool CEXIETHERNET::sendPacket(u8 *etherpckt, int size) 
{
	DEBUGPRINT( "Packet: 0x");
	for(int a = 0; a < size; ++a)
	{
		DEBUGPRINT( "%02X", etherpckt[a]);
	}
	DEBUGPRINT( " : Size: %d\n", size);
	int numBytesWrit = write(fd, etherpckt, size);
	if(numBytesWrit != size)
	{
		DEBUGPRINT("BBA sendPacket %i only got %i bytes sent!\n", size, numBytesWrit);
		return false;
	}
	else
		DEBUGPRINT("Sent out the correct number of bytes: %d\n", size);
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
