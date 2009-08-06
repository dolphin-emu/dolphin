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

#include <assert.h>
#include "StringUtil.h"
#include "../Memmap.h"
// GROSS CODE ALERT: headers need to be included in the following order
#include "TAP_Win32.h"
#include "../EXI_Device.h"
#include "../EXI_DeviceEthernet.h"

namespace Win32TAPHelper
{

bool IsTAPDevice(const char *guid)
{
	HKEY netcard_key;
	LONG status;
	DWORD len;
	int i = 0;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ, &netcard_key);

	if (status != ERROR_SUCCESS)
		return false;

	for (;;)
	{
		char enum_name[256];
		char unit_string[256];
		HKEY unit_key;
		char component_id_string[] = "ComponentId";
		char component_id[256];
		char net_cfg_instance_id_string[] = "NetCfgInstanceId";
		char net_cfg_instance_id[256];
		DWORD data_type;

		len = sizeof(enum_name);
		status = RegEnumKeyEx(netcard_key, i, enum_name, &len, NULL, NULL, NULL, NULL);

		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS)
			return false;

		snprintf(unit_string, sizeof(unit_string), "%s\\%s", ADAPTER_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, unit_string, 0, KEY_READ, &unit_key);

		if (status != ERROR_SUCCESS)
		{
			return false;
		}
		else
		{
			len = sizeof(component_id);
			status = RegQueryValueEx(unit_key, component_id_string, NULL, &data_type, (LPBYTE)component_id, &len);

			if (!(status != ERROR_SUCCESS || data_type != REG_SZ))
			{
				len = sizeof(net_cfg_instance_id);
				status = RegQueryValueEx(unit_key, net_cfg_instance_id_string, NULL, &data_type, (LPBYTE)net_cfg_instance_id, &len);

				if (status == ERROR_SUCCESS && data_type == REG_SZ)
				{
					if (!strcmp(component_id, TAP_COMPONENT_ID) && !strcmp(net_cfg_instance_id, guid))
					{
						RegCloseKey(unit_key);
						RegCloseKey(netcard_key);
						return true;
					}
				}
			}
			RegCloseKey(unit_key);
		}
		++i;
	}

	RegCloseKey(netcard_key);
	return false;
}

bool GetGUID(char *name, int name_size)
{
	LONG status;
	HKEY control_net_key;
	DWORD len;
	int i = 0;
	bool stop = false;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ, &control_net_key);

	if (status != ERROR_SUCCESS)
		return false;
	
	while (!stop)
	{
		char enum_name[256];
		char connection_string[256];
		HKEY connection_key;
		char name_data[256];
		DWORD name_type;
		const char name_string[] = "Name";

		len = sizeof(enum_name);
		status = RegEnumKeyEx(control_net_key, i, enum_name, &len, NULL, NULL, NULL, NULL);

		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS)
			return false;

		snprintf(connection_string, sizeof(connection_string), "%s\\%s\\Connection", NETWORK_CONNECTIONS_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, connection_string, 0, KEY_READ, &connection_key);

		if (status == ERROR_SUCCESS)
		{
			len = sizeof(name_data);
			status = RegQueryValueEx(connection_key, name_string, NULL, &name_type, (LPBYTE)name_data, &len);

			if (status != ERROR_SUCCESS || name_type != REG_SZ)
			{
				return false;
			}
			else
			{
				if (IsTAPDevice(enum_name))
				{
					snprintf(name, name_size, "%s", enum_name);
					stop = true;
				}
			}

			RegCloseKey(connection_key);
		}
		i++;
	}

	RegCloseKey(control_net_key);

	if (!stop)
		return false;

	return true;
}

} // namespace Win32TAPHelper

bool CEXIETHERNET::deactivate()
{
	DEBUGPRINT("Deactivating BBA...");
	if (!isActivated())
		return true;
	CloseHandle(mHRecvEvent);
	mHRecvEvent = INVALID_HANDLE_VALUE;
	CloseHandle(mHAdapter);
	mHAdapter = INVALID_HANDLE_VALUE;
	DEBUGPRINT("Success!");
	return true;
}

bool CEXIETHERNET::isActivated()
{ 
	return mHAdapter != INVALID_HANDLE_VALUE;
}

bool CEXIETHERNET::activate()
{
	if (isActivated())
		return true;

	DEBUGPRINT("Activating BBA...");

	DWORD len;
	char device_path[256];
	char device_guid[0x100];

	if (!Win32TAPHelper::GetGUID(device_guid, sizeof(device_guid)))
	{
		DEBUGPRINT("Failed to find a TAP GUID");
		return false;
	}

	/* Open Windows TAP-Win32 adapter */
	snprintf(device_path, sizeof(device_path), "%s%s%s", USERMODEDEVICEDIR, device_guid, TAPSUFFIX);

	mHAdapter = CreateFile (
		device_path,
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
		0 );

	if (mHAdapter == INVALID_HANDLE_VALUE)
	{
		DEBUGPRINT("Failed to open TAP at %s", device_path);
		return false;
	}

	/* get driver version info */
	{
		ULONG info[3];
		if (DeviceIoControl (mHAdapter, TAP_IOCTL_GET_VERSION,
			&info, sizeof (info), &info, sizeof (info), &len, NULL))
		{
			DEBUGPRINT("TAP-Win32 Driver Version %d.%d %s",
				info[0], info[1], (info[2] ? "(DEBUG)" : ""));
		}
		if ( !(info[0] > TAP_WIN32_MIN_MAJOR
			|| (info[0] == TAP_WIN32_MIN_MAJOR && info[1] >= TAP_WIN32_MIN_MINOR)) )
		{
#define PACKAGE_NAME "Dolphin"
			DEBUGPRINT("ERROR:  This version of " PACKAGE_NAME " requires a TAP-Win32 driver that is at least version %d.%d -- If you recently upgraded your " PACKAGE_NAME " distribution, a reboot is probably required at this point to get Windows to see the new driver.",
				TAP_WIN32_MIN_MAJOR,
				TAP_WIN32_MIN_MINOR);
			return false;
		}
	}

	/* get driver MTU */
	{
		if (!DeviceIoControl(mHAdapter, TAP_IOCTL_GET_MTU,
			&mMtu, sizeof (mMtu), &mMtu, sizeof (mMtu), &len, NULL))
		{
			DEBUGPRINT("Couldn't get device MTU");
			return false;
		}
		DEBUGPRINT("TAP-Win32 MTU=%d (ignored)", mMtu);
	}

	/* set driver media status to 'connected' */
	{
		ULONG status = TRUE;
		if (!DeviceIoControl (mHAdapter, TAP_IOCTL_SET_MEDIA_STATUS,
			&status, sizeof (status), &status, sizeof (status), &len, NULL))
		{
			DEBUGPRINT("WARNING: The TAP-Win32 driver rejected a TAP_IOCTL_SET_MEDIA_STATUS DeviceIoControl call.");
			return false;
		}
		else
		{
			DEBUGPRINT("TAP-WIN32 status as Connected");
		}
	}

	//set up recv event
	mHRecvEvent = CreateEvent(NULL, false, false, NULL);
	memset((void*)&mReadOverlapped, 0 , sizeof(mReadOverlapped));
	resume();

	DEBUGPRINT("Success!");
	return true;
	//TODO: Activate Device!
}

bool CEXIETHERNET::CheckRecieved()
{
	if (!isActivated())
		return false;

	// I have no idea o_O
	return false;
}

bool CEXIETHERNET::sendPacket(u8 *etherpckt, int size) 
{
	if (!isActivated())
		activate();

	DEBUGPRINT("Packet (%i): %s", size, ArrayToString(etherpckt, size).c_str());

	DWORD numBytesWrit;
	OVERLAPPED overlap;
	memset((void*)&overlap, 0, sizeof(overlap));
	//ZERO_OBJECT(overlap);
	//overlap.hEvent = mHRecvEvent;
	if (!WriteFile(mHAdapter, etherpckt, size, &numBytesWrit, &overlap))
	{
		// Fail Boat
		DWORD res = GetLastError();
		DEBUGPRINT("Failed to send packet with error 0x%X", res);
	}
	if (numBytesWrit != size) 
	{
		DEBUGPRINT("BBA sendPacket %i only got %i bytes sent!", size, numBytesWrit);
		return false;
	}
	recordSendComplete();
	//exit(0);
	return true;
}

bool CEXIETHERNET::handleRecvdPacket() 
{
	int rbwpp = mCbw.p_write() + CB_OFFSET;	//read buffer write page pointer
	u32 available_bytes_in_cb;
	if (rbwpp < mRBRPP)
		available_bytes_in_cb = mRBRPP - rbwpp;
	else if (rbwpp == mRBRPP)
		available_bytes_in_cb = mRBEmpty ? CB_SIZE : 0;
	else //rbwpp > mRBRPP
		available_bytes_in_cb = CB_SIZE - rbwpp + (mRBRPP - CB_OFFSET);

	//DUMPWORD(rbwpp);
	//DUMPWORD(mRBRPP);
	//DUMPWORD(available_bytes_in_cb);

	assert(available_bytes_in_cb <= CB_SIZE);
	if (available_bytes_in_cb != CB_SIZE)//< mRecvBufferLength + SIZEOF_RECV_DESCRIPTOR)
		return true;
	cbwriteDescriptor(mRecvBufferLength);
	mCbw.write(mRecvBuffer, mRecvBufferLength);
	mCbw.align();
	rbwpp = mCbw.p_write() + CB_OFFSET;
	//DUMPWORD(rbwpp);

	//mPacketsRcvd++;
	mRecvBufferLength = 0;

	if (mBbaMem[0x08] & BBA_INTERRUPT_RECV) 
	{
		if (!(mBbaMem[0x09] & BBA_INTERRUPT_RECV)) 
		{
			mBbaMem[0x09] |= BBA_INTERRUPT_RECV;
			DEBUGPRINT("BBA Recv interrupt raised");
			//interrupt.raiseEXI("BBA Recv");
			m_bInterruptSet = true;
		}
	}

	if (mBbaMem[BBA_NCRA] & BBA_NCRA_SR)
		startRecv();

	return true;
}

bool CEXIETHERNET::resume()
{
	if(!isActivated())
		return true;

	DEBUGPRINT("BBA resume");
	//mStop = false;

	RegisterWaitForSingleObject(&mHReadWait, mHRecvEvent, ReadWaitCallback,
		this, INFINITE, WT_EXECUTEDEFAULT);//WT_EXECUTEINWAITTHREAD

	mReadOverlapped.hEvent = mHRecvEvent;

	if (mBbaMem[BBA_NCRA] & BBA_NCRA_SR)
		startRecv();

	DEBUGPRINT("BBA resume complete");
	return true;
}

bool CEXIETHERNET::startRecv()
{
	if (!isActivated())
		return false;// Should actually be an assert

	DEBUGPRINT("startRecv... ");

	if (mWaiting)
	{
		DEBUGPRINT("already waiting");
		return true;
	}

	DWORD res = ReadFile(mHAdapter, mRecvBuffer, mRecvBuffer.size(),
		&mRecvBufferLength, &mReadOverlapped);

	if (res)
	{
		// Operation completed immediately
		DEBUGPRINT("completed, res %i", res);
		mWaiting = true;
	}
	else
	{
		res = GetLastError();
		if (res == ERROR_IO_PENDING)
		{
			//'s ok :)
			DEBUGPRINT("pending");
			// WaitCallback will be called
			mWaiting = true;
		}
		else
		{
			// error occurred
			return false;
		}
	}

	return true;
}

VOID CALLBACK CEXIETHERNET::ReadWaitCallback(PVOID lpParameter, BOOLEAN TimerFired)
{
	static int sNumber = 0;
	int cNumber = sNumber++;
	DEBUGPRINT("WaitCallback %i", cNumber);

	if (TimerFired)
		return;

	CEXIETHERNET* self = (CEXIETHERNET*)lpParameter;
	if(self->mHAdapter == INVALID_HANDLE_VALUE)
		return;
	GetOverlappedResult(self->mHAdapter, &self->mReadOverlapped,
		&self->mRecvBufferLength, false);
	self->mWaiting = false;
	self->handleRecvdPacket();
	DEBUGPRINT("WaitCallback %i done", cNumber);
}

union bba_descr
{
	struct { u32 next_packet_ptr:12, packet_len:12, status:8; };
	u32 word;
};

bool CEXIETHERNET::cbwriteDescriptor(u32 size)
{
	//if(size < 0x3C) {//60
#define ETHERNET_HEADER_SIZE 0xE
	if (size < ETHERNET_HEADER_SIZE) 
	{
		DEBUGPRINT("Packet too small: %i bytes", size);
		return false;
	}

	// The descriptor supposed to include the size of itself
	size += SIZEOF_RECV_DESCRIPTOR;

	//We should probably not implement wraparound here,
	//since neither tmbinc, riptool.dol, or libogc does...
	if (mCbw.p_write() + SIZEOF_RECV_DESCRIPTOR >= CB_SIZE) 
	{
		DEBUGPRINT("The descriptor won't fit");
		return false;
	}
	if (size >= CB_SIZE) 
	{
		DEBUGPRINT("Packet too big: %i bytes", size);
		return false;
	}

	bba_descr descr;
	descr.word = 0;
	descr.packet_len = size;
	descr.status = 0;
	u32 npp;
	if (mCbw.p_write() + size < CB_SIZE) 
	{
		npp = mCbw.p_write() + size + CB_OFFSET;
	} 
	else 
	{
		npp = mCbw.p_write() + size + CB_OFFSET - CB_SIZE;
	}

	npp = (npp + 0xff) & ~0xff;

	if (npp >= CB_SIZE + CB_OFFSET)
		npp -= CB_SIZE;

	descr.next_packet_ptr = npp >> 8;
	//DWORD swapped = swapw(descr.word);
	//next_packet_ptr:12, packet_len:12, status:8;
	DEBUGPRINT("Writing descriptor 0x%08X @ 0x%04X: next 0x%03X len 0x%03X status 0x%02X",
		descr.word, mCbw.p_write() + CB_OFFSET, descr.next_packet_ptr,
		descr.packet_len, descr.status);

	mCbw.write(&descr.word, SIZEOF_RECV_DESCRIPTOR);

	return true;
}