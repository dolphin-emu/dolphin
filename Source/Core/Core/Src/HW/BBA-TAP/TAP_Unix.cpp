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
	#include <stdio.h>
	#include <fcntl.h>
	#include <sys/ioctl.h>
	#include <net/if.h>  
#if !defined(__APPLE__)
	#include <stropts.h>
	#include <linux/if_tun.h>
#endif
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
#if !defined(__APPLE__)	
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
#endif
	DEBUGPRINT("Returned Socket name is: %s\n", ifr.ifr_name);
	resume();
	return true;
	
}
bool CEXIETHERNET::CheckRecieved()
{
	if(!isActivated())
		return false;
	int i;
	int maxfd;
	int retval;
	struct timeval tv;
	int timeout = 3; // 3 seconds will kill him
	fd_set mask;

	/* Find the largest file descriptor */
	maxfd = fd;

	/* Check the file descriptors for available data */
	errno = 0;

	/* Set up the mask of file descriptors */
	FD_ZERO(&mask);
	
	FD_SET(fd, &mask);

	/* Set up the timeout */
	tv.tv_sec = timeout/1000;
	tv.tv_usec = (timeout%1000)*1000;

	/* Look! */
	retval = select(maxfd+1, &mask, NULL, NULL, &tv);

	/* Mark all file descriptors ready that have data available */
	if ( retval > 0 ) {
		if ( FD_ISSET(fd, &mask) )
		{
			DEBUGPRINT("\t\t\t\tWe have data!\n");
			return true;
		}
	}
	return false;
}
bool CEXIETHERNET::resume() {
	if(!isActivated())
		return true;
	DEBUGPRINT("BBA resume\n");
	if(mBbaMem[BBA_NCRA] & BBA_NCRA_SR) {
		startRecv();
	}
	DEBUGPRINT("BBA resume complete\n");
	return true;
}
bool CEXIETHERNET::startRecv() {
	DEBUGPRINT("Start Receive!\n");
	//exit(0);
	if(!isActivated())
		return false;// Should actually be an assert
	if(!CheckRecieved()) // Check if we have data
		return false; // Nope
	DEBUGPRINT("startRecv... ");
	if(mWaiting) {
		DEBUGPRINT("already waiting\n");
		return true;
	}
	u32 BytesRead = 0;
	u8 B[2];
	int Num = 0;
	while(read(fd, B, 1))
	{
		DEBUGPRINT("Read 1 Byte!\n");
		mRecvBuffer.write(1, B);
		Num++;
	}
	DEBUGPRINT("Read %d bytes\n", Num);
	return true; 
}
bool CEXIETHERNET::sendPacket(u8 *etherpckt, int size) 
{
	if(!isActivated())
		activate();
	DEBUGPRINT( "Packet: 0x");
	for(int a = 0; a < size; ++a)
	{
		DEBUGPRINT( "%02X ", etherpckt[a]);
	}
	DEBUGPRINT( " : Size: %d\n", size);
	int numBytesWrit = write(fd, etherpckt, size);
	if(numBytesWrit != size)
	{
		DEBUGPRINT("BBA sendPacket %i only got %i bytes sent!errno: %d\n", size, numBytesWrit, errno);
		return false;
	}
	else
		DEBUGPRINT("Sent out the correct number of bytes: %d\n", size);
	recordSendComplete();
	//exit(0);
	return true;
}
bool CEXIETHERNET::handleRecvdPacket() 
{
	DEBUGPRINT(" Handle received Packet!\n");
	exit(0);
}
