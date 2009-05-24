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
#include "Tap_Win32.h"


bool CEXIETHERNET::deactivate()
{
	DEBUGPRINT("Deactivating BBA...\n");
	if(!isActivated())
		return true;
	CloseHandle(mHRecvEvent);
	mHRecvEvent = INVALID_HANDLE_VALUE;
	CloseHandle(mHAdapter);
	mHAdapter = INVALID_HANDLE_VALUE;
	DEBUGPRINT("Success!\n");
	return true;
}
bool CEXIETHERNET::isActivated()
{ 
	return mHAdapter != INVALID_HANDLE_VALUE;
}

bool CEXIETHERNET::activate() {
if(isActivated())
		return true;

	DEBUGPRINT("\nActivating BBA...\n");

	DWORD len;

#if 0
	device_guid = get_device_guid (dev_node, guid_buffer, sizeof (guid_buffer), tap_reg, panel_reg, &gc);

	if (!device_guid)
		DEGUB("TAP-Win32 adapter '%s' not found\n", dev_node);

	/* Open Windows TAP-Win32 adapter */
	openvpn_snprintf (device_path, sizeof(device_path), "%s%s%s",
		USERMODEDEVICEDIR,
		device_guid,
		TAPSUFFIX);
#endif	//0

	mHAdapter = CreateFile (/*device_path*/
		//{E0277714-28A6-4EB6-8AA2-7DF4870C04F6}
		USERMODEDEVICEDIR "{E0277714-28A6-4EB6-8AA2-7DF4870C04F6}" TAPSUFFIX,
		GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
		FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);
	DEBUGPRINT("TAP-WIN32 device opened: %s\n",
		USERMODEDEVICEDIR "{E0277714-28A6-4EB6-8AA2-7DF4870C04F6}" TAPSUFFIX);
	if(mHAdapter == INVALID_HANDLE_VALUE) {
		DEBUGPRINT("mHAdapter is invalid\n");
		return false;
	}

	/* get driver version info */
	{
		ULONG info[3];
		if (DeviceIoControl (mHAdapter, TAP_IOCTL_GET_VERSION,
			&info, sizeof (info), &info, sizeof (info), &len, NULL))
		{
			DEBUGPRINT("TAP-Win32 Driver Version %d.%d %s\n",
				info[0], info[1], (info[2] ? "(DEBUG)" : ""));
		}
		if ( !(info[0] > TAP_WIN32_MIN_MAJOR
			|| (info[0] == TAP_WIN32_MIN_MAJOR && info[1] >= TAP_WIN32_MIN_MINOR)) )
		{
#define PACKAGE_NAME "Dolphin"
			DEBUGPRINT("ERROR:  This version of " PACKAGE_NAME " requires a TAP-Win32 driver that is at least version %d.%d -- If you recently upgraded your " PACKAGE_NAME " distribution, a reboot is probably required at this point to get Windows to see the new driver.\n",
				TAP_WIN32_MIN_MAJOR,
				TAP_WIN32_MIN_MINOR);
			return false;
		}
	}

	/* get driver MTU */
	{
		if(!DeviceIoControl(mHAdapter, TAP_IOCTL_GET_MTU,
			&mMtu, sizeof (mMtu), &mMtu, sizeof (mMtu), &len, NULL))
		{
			DEBUGPRINT("Couldn't get device MTU");
			return false;
		}
		DEBUGPRINT("TAP-Win32 MTU=%d (ignored)\n", mMtu);
	}

	/* set driver media status to 'connected' */
	{
		ULONG status = TRUE;
		if (!DeviceIoControl (mHAdapter, TAP_IOCTL_SET_MEDIA_STATUS,
			&status, sizeof (status), &status, sizeof (status), &len, NULL))
		{
			DEBUGPRINT("WARNING: The TAP-Win32 driver rejected a TAP_IOCTL_SET_MEDIA_STATUS DeviceIoControl call.\n");
			return false;
		}
		else
		{
			DEBUGPRINT("TAP-WIN32 status as Connected\n");
		}
	}

	//set up recv event
	mHRecvEvent = CreateEvent(NULL, false, false, NULL);
	//ZERO_OBJECT(mReadOverlapped);
	resume();

	DEBUGPRINT("Success!\n\n");
	return true;
	//TODO: Activate Device!
}
bool CEXIETHERNET::sendPacket(u8 *etherpckt, int size) 
{
	if(!isActivated())
		activate();
	DEBUGPRINT( "Packet: 0x");
	for(int a = 0; a < size; ++a)
	{
		DEBUGPRINT( "%02X", etherpckt[a]);
	}
	DEBUGPRINT( " : Size: %d\n", size);
	DWORD numBytesWrit;
	OVERLAPPED overlap;
	memset((void*)&overlap, 0, sizeof(overlap));
	//ZERO_OBJECT(overlap);
	//overlap.hEvent = mHRecvEvent;
	if(!WriteFile(mHAdapter, etherpckt, size, &numBytesWrit, &overlap))
	{ // Fail Boat
		DWORD res = GetLastError();
		DEBUGPRINT("Failed to send packet with error 0x%X\n", res);
	}
	if(numBytesWrit != size) 
	{
		DEBUGPRINT("BBA sendPacket %i only got %i bytes sent!\n", size, numBytesWrit);
		return false;
	}
	recordSendComplete();
	//exit(0);
	return true;
}
bool CEXIETHERNET::handleRecvdPacket() 
{
	DEBUGPRINT(" Handle received Packet!\n");
	exit(0);
}
bool CEXIETHERNET::resume() {
	if(!isActivated())
		return true;
	DEBUGPRINT("BBA resume\n");
	//mStop = false;
	RegisterWaitForSingleObject(&mHReadWait, mHRecvEvent, ReadWaitCallback,
		this, INFINITE, WT_EXECUTEDEFAULT);//WT_EXECUTEINWAITTHREAD
	mReadOverlapped.hEvent = mHRecvEvent;
	if(mBbaMem[BBA_NCRA] & BBA_NCRA_SR) {
		startRecv();
	}
	DEBUGPRINT("BBA resume complete\n");
	return true;
}
bool CEXIETHERNET::startRecv() {
	if(!isActivated())
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
	return true;
}
VOID CALLBACK CEXIETHERNET::ReadWaitCallback(PVOID lpParameter, BOOLEAN TimerFired) {
	static int sNumber = 0;
	int cNumber = sNumber++;
	DEBUGPRINT("WaitCallback %i\n", cNumber);
	if(TimerFired)
		return;
	CEXIETHERNET* self = (CEXIETHERNET*)lpParameter;
	if(self->mHAdapter == INVALID_HANDLE_VALUE)
		return;
	GetOverlappedResult(self->mHAdapter, &self->mReadOverlapped,
		&self->mRecvBufferLength, false);
	self->mWaiting = false;
	self->handleRecvdPacket();
	DEBUGPRINT("WaitCallback %i done\n", cNumber);
}