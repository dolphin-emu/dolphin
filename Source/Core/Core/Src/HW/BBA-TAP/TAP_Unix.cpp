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
	#include <linux/if_tun.h>
	#include <assert.h>
	int fd = -1;
	bool hasDHCP = false;
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
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	
	strncpy(ifr.ifr_name, "Dolphin", IFNAMSIZ);
	
	if( (err = ioctl(fd, TUNSETIFF, (void*) &ifr)) < 0)
	{
		close(fd);
		fd = -1;
		DEBUGPRINT(" Error with IOCTL: 0x%X\n", err);
		return false;
	}
	ioctl( fd, TUNSETNOCSUM, 1 );
#endif
	DEBUGPRINT("Returned Socket name is: %s\n", ifr.ifr_name);
	system("brctl addif pan0 Dolphin");
	system("ifconfig Dolphin up");
	sleep(5);
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
	if(!CheckRecieved())
		return false;
	DEBUGPRINT("startRecv... ");
	if(mWaiting) {
		DEBUGPRINT("already waiting\n");
		return true;
	}
	u8 B[1514];
	if((mRecvBufferLength = read(fd, B, 1500)) > 0)
	{
		//mRecvBuffer.write(B, BytesRead);
		//strncat(mRecvBuffer.p(), B, BytesRead);
		memcpy(mRecvBuffer, B, mRecvBufferLength);
	}
	DEBUGPRINT("Read %d bytes\n", mRecvBufferLength);
			if(hasDHCP && mRecvBufferLength == 342)
			{
				DEBUGPRINT("DHCP offer packet\n");
				//exit(0);
			}
	handleRecvdPacket();
	return true; 
}
bool CEXIETHERNET::sendPacket(u8 *etherpckt, int size) 
{
	if(!isActivated())
		return false;
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
		if(size == 342)
			hasDHCP = true;
	recordSendComplete();
	//exit(0);
	return true;
}
bool CEXIETHERNET::handleRecvdPacket() 
{

	int rbwpp = mCbw.p_write() + CB_OFFSET;	//read buffer write page pointer
	u32 available_bytes_in_cb;
	if(rbwpp < mRBRPP)
		available_bytes_in_cb = mRBRPP - rbwpp;
	else if(rbwpp == mRBRPP)
		available_bytes_in_cb = mRBEmpty ? CB_SIZE : 0;
	else //rbwpp > mRBRPP
		available_bytes_in_cb = CB_SIZE - rbwpp + (mRBRPP - CB_OFFSET);

	//DUMPWORD(rbwpp);
	//DUMPWORD(mRBRPP);
	//DUMPWORD(available_bytes_in_cb);

	assert(available_bytes_in_cb <= CB_SIZE);
	if(available_bytes_in_cb != CB_SIZE)//< mRecvBufferLength + SIZEOF_RECV_DESCRIPTOR)
	if(available_bytes_in_cb != CB_SIZE)//< mRecvBufferLength + SIZEOF_RECV_DESCRIPTOR)
		return true;
	cbwriteDescriptor(mRecvBufferLength);
	mCbw.write(mRecvBuffer, mRecvBufferLength);
	mCbw.align();
	rbwpp = mCbw.p_write() + CB_OFFSET;
	//DUMPWORD(rbwpp);

	//mPacketsRcvd++;
	mRecvBufferLength = 0;

	if(mBbaMem[0x08] & BBA_INTERRUPT_RECV) 
	{
		if(!(mBbaMem[0x09] & BBA_INTERRUPT_RECV)) 
		{
			mBbaMem[0x09] |= BBA_INTERRUPT_RECV;
			DEBUGPRINT("BBA Recv interrupt raised\n");
			m_bInterruptSet = true;
		}
	}

	if(mBbaMem[BBA_NCRA] & BBA_NCRA_SR) 
	{
		startRecv();
	}

	return true;
}
union bba_descr {
	struct { u32 next_packet_ptr:12, packet_len:12, status:8; };
	u32 word;
};
bool CEXIETHERNET::cbwriteDescriptor(u32 size) {
	//if(size < 0x3C) {//60
#define ETHERNET_HEADER_SIZE 0xE
	if(size < ETHERNET_HEADER_SIZE) 
	{
		DEBUGPRINT("Packet too small: %i bytes\n", size);
		return false;
	}

	size += SIZEOF_RECV_DESCRIPTOR;  //The descriptor supposed to include the size of itself

	//We should probably not implement wraparound here,
	//since neither tmbinc, riptool.dol, or libogc does...
	if(mCbw.p_write() + SIZEOF_RECV_DESCRIPTOR >= CB_SIZE) 
	{
		DEBUGPRINT("The descriptor won't fit\n");
		return false;
	}
	if(size >= CB_SIZE) 
	{
		DEBUGPRINT("Packet too big: %i bytes\n", size);
		return false;
	}

	bba_descr descr;
	descr.word = 0;
	descr.packet_len = size;
	descr.status = 0;
	u32 npp;
	if(mCbw.p_write() + size < CB_SIZE) 
	{
		npp = mCbw.p_write() + size + CB_OFFSET;
	} 
	else 
	{
		npp = mCbw.p_write() + size + CB_OFFSET - CB_SIZE;
	}
	npp = (npp + 0xff) & ~0xff;
	if(npp >= CB_SIZE + CB_OFFSET)
		npp -= CB_SIZE;
	descr.next_packet_ptr = npp >> 8;
	//DWORD swapped = swapw(descr.word);
	//next_packet_ptr:12, packet_len:12, status:8;
	DEBUGPRINT("Writing descriptor 0x%08X @ 0x%04X: next 0x%03X len 0x%03X status 0x%02X\n",
		descr.word, mCbw.p_write() + CB_OFFSET, descr.next_packet_ptr,
		descr.packet_len, descr.status);
	mCbw.write(&descr.word, SIZEOF_RECV_DESCRIPTOR);

	return true;
}
