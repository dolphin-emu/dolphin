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

#include "StringUtil.h"
#include "../Memmap.h"
//#pragma optimize("",off)
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
			status = RegQueryValueEx(unit_key, component_id_string, NULL,
				&data_type, (LPBYTE)component_id, &len);

			if (!(status != ERROR_SUCCESS || data_type != REG_SZ))
			{
				len = sizeof(net_cfg_instance_id);
				status = RegQueryValueEx(unit_key, net_cfg_instance_id_string, NULL,
					&data_type, (LPBYTE)net_cfg_instance_id, &len);

				if (status == ERROR_SUCCESS && data_type == REG_SZ)
				{
					if (!strcmp(component_id, TAP_COMPONENT_ID) &&
						!strcmp(net_cfg_instance_id, guid))
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

bool GetGUIDs(std::vector<std::string>& guids)
{
	LONG status;
	HKEY control_net_key;
	DWORD len;
	int i = 0;
	bool found_all = false;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ, &control_net_key);

	if (status != ERROR_SUCCESS)
		return false;
	
	while (!found_all)
	{
		char enum_name[256];
		char connection_string[256];
		HKEY connection_key;
		char name_data[256];
		DWORD name_type;
		const char name_string[] = "Name";

		len = sizeof(enum_name);
		status = RegEnumKeyEx(control_net_key, i, enum_name,
			&len, NULL, NULL, NULL, NULL);

		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS)
			return false;

		snprintf(connection_string, sizeof(connection_string),
			"%s\\%s\\Connection", NETWORK_CONNECTIONS_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, connection_string,
			0, KEY_READ, &connection_key);

		if (status == ERROR_SUCCESS)
		{
			len = sizeof(name_data);
			status = RegQueryValueEx(connection_key, name_string, NULL,
				&name_type, (LPBYTE)name_data, &len);

			if (status != ERROR_SUCCESS || name_type != REG_SZ)
			{
				return false;
			}
			else
			{
				if (IsTAPDevice(enum_name))
				{
					guids.push_back(enum_name);
					//found_all = true;
				}
			}

			RegCloseKey(connection_key);
		}
		i++;
	}

	RegCloseKey(control_net_key);

	//if (!found_all)
		//return false;

	return true;
}

bool OpenTAP(HANDLE& adapter, const std::string device_guid)
{
	char device_path[256];

	/* Open Windows TAP-Win32 adapter */
	snprintf(device_path, sizeof(device_path), "%s%s%s",
		USERMODEDEVICEDIR, device_guid.c_str(), TAPSUFFIX);

	adapter = CreateFile(device_path, GENERIC_READ | GENERIC_WRITE, 0, 0,
		OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);

	if (adapter == INVALID_HANDLE_VALUE)
	{
		INFO_LOG(SP1, "Failed to open TAP at %s", device_path);
		return false;
	}
	return true;
}

} // namespace Win32TAPHelper

bool CEXIETHERNET::deactivate()
{
	INFO_LOG(SP1, "Deactivating BBA...");
	if (!isActivated())
		return true;
	CloseHandle(mHRecvEvent);
	mHRecvEvent = INVALID_HANDLE_VALUE;
	CloseHandle(mHAdapter);
	mHAdapter = INVALID_HANDLE_VALUE;
	INFO_LOG(SP1, "Success!");
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

	INFO_LOG(SP1, "Activating BBA...");

	DWORD len;
	std::vector<std::string> device_guids;

	if (!Win32TAPHelper::GetGUIDs(device_guids))
	{
		INFO_LOG(SP1, "Failed to find a TAP GUID");
		return false;
	}

	for (int i = 0; i < device_guids.size(); i++)
	{
		if (Win32TAPHelper::OpenTAP(mHAdapter, device_guids.at(i)))
		{
			ERROR_LOG(SP1, "OPENED %s", device_guids.at(i).c_str());
			i = device_guids.size();
		}
	}
	if (mHAdapter == INVALID_HANDLE_VALUE)
	{
		INFO_LOG(SP1, "Failed to open any TAP");
		return false;
	}

	/* get driver version info */
	ULONG info[3];
	if (DeviceIoControl (mHAdapter, TAP_IOCTL_GET_VERSION,
		&info, sizeof (info), &info, sizeof (info), &len, NULL))
	{
		INFO_LOG(SP1, "TAP-Win32 Driver Version %d.%d %s",
			info[0], info[1], (info[2] ? "(DEBUG)" : ""));
	}
	if ( !(info[0] > TAP_WIN32_MIN_MAJOR
		|| (info[0] == TAP_WIN32_MIN_MAJOR && info[1] >= TAP_WIN32_MIN_MINOR)) )
	{
		PanicAlert("ERROR:  This version of Dolphin requires a TAP-Win32 driver"
			" that is at least version %d.%d -- If you recently upgraded your Dolphin"
			" distribution, a reboot is probably required at this point to get"
			" Windows to see the new driver.",
			TAP_WIN32_MIN_MAJOR, TAP_WIN32_MIN_MINOR);
		return false;
	}

	/* get driver MTU */
	if (!DeviceIoControl(mHAdapter, TAP_IOCTL_GET_MTU,
		&mMtu, sizeof (mMtu), &mMtu, sizeof (mMtu), &len, NULL))
	{
		INFO_LOG(SP1, "Couldn't get device MTU");
		return false;
	}
	INFO_LOG(SP1, "TAP-Win32 MTU=%d (ignored)", mMtu);

	/* set driver media status to 'connected' */
	ULONG status = TRUE;
	if (!DeviceIoControl(mHAdapter, TAP_IOCTL_SET_MEDIA_STATUS,
		&status, sizeof (status), &status, sizeof (status), &len, NULL))
	{
		INFO_LOG(SP1, "WARNING: The TAP-Win32 driver rejected a"
			"TAP_IOCTL_SET_MEDIA_STATUS DeviceIoControl call.");
		return false;
	}

	//set up recv event
	if ((mHRecvEvent = CreateEvent(NULL, false, false, NULL)) == NULL)
	{
		INFO_LOG(SP1, "Failed to create recv event:%x", GetLastError());
		return false;
	}
	ZeroMemory(&mReadOverlapped, sizeof(mReadOverlapped));
	resume();

	INFO_LOG(SP1, "Success!");
	return true;
}

bool CEXIETHERNET::CheckRecieved()
{
	if (!isActivated())
		return false;

	return false;
}

// Required for lwip...not sure why
void fixup_ip_checksum(u16 *dataptr, u16 len)
{
	u32 acc = 0;
	u16 *data = dataptr;
	u16 *chksum = &dataptr[5];

	*chksum = 0;

	while (len > 1) {
		u16 s = *data++;
		acc += Common::swap16(s);
		len -= 2;
	}
	acc = (acc >> 16) + (acc & 0xffff);
	acc += (acc >> 16);
	*chksum = Common::swap16(~acc);
}

bool CEXIETHERNET::sendPacket(u8 *etherpckt, int size)
{
	fixup_ip_checksum((u16*)etherpckt +	7, 20);
	INFO_LOG(SP1, "Packet (%zu):\n%s", size, ArrayToString(etherpckt, size, 0, 16).c_str());

	DWORD numBytesWrit;
	OVERLAPPED overlap;
	ZeroMemory(&overlap, sizeof(overlap));
	//overlap.hEvent = mHRecvEvent;
	if (!WriteFile(mHAdapter, etherpckt, size, &numBytesWrit, &overlap))
	{
		// Fail Boat
		DWORD res = GetLastError();
		INFO_LOG(SP1, "Failed to send packet with error 0x%X", res);
	}
	if (numBytesWrit != size)
	{
		INFO_LOG(SP1, "BBA sendPacket %i only got %i bytes sent!", size, numBytesWrit);
		return false;
	}
	recordSendComplete();
	return true;
}

bool CEXIETHERNET::handleRecvdPacket()
{
	static u32 mPacketsRecvd = 0;
	INFO_LOG(SP1, "handleRecvdPacket Packet %i (%i):\n%s", mPacketsRecvd, mRecvBufferLength,
		ArrayToString(mRecvBuffer, mRecvBufferLength, 0, 16).c_str());

	static u8 broadcast[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	if (memcmp(mRecvBuffer, broadcast, 6) && memcmp(mRecvBuffer, mac_address, 6))
	{
		INFO_LOG(SP1, "dropping packet - not for us");
		goto wait_for_next;
	}

	int rbwpp = (int)(mCbw.p_write() + CB_OFFSET);  // read buffer write page pointer
	u32 available_bytes_in_cb;
	if (rbwpp < mRBRPP)
		available_bytes_in_cb = mRBRPP - rbwpp;
	else if (rbwpp == mRBRPP)
		available_bytes_in_cb = mRBEmpty ? CB_SIZE : 0;
	else //rbwpp > mRBRPP
		available_bytes_in_cb = CB_SIZE - rbwpp + (mRBRPP - CB_OFFSET);

	INFO_LOG(SP1, "rbwpp %i mRBRPP %04x available_bytes_in_cb %i",
		rbwpp, mRBRPP, available_bytes_in_cb);

	_dbg_assert_(SP1, available_bytes_in_cb <= CB_SIZE);
	if (available_bytes_in_cb != CB_SIZE)//< mRecvBufferLength + SIZEOF_RECV_DESCRIPTOR)
		return true;
	cbwriteDescriptor(mRecvBufferLength);
	mCbw.write(mRecvBuffer, mRecvBufferLength);
	mCbw.align();
	rbwpp = (int)(mCbw.p_write() + CB_OFFSET);
	
	INFO_LOG(SP1, "rbwpp %i", rbwpp);

	mPacketsRecvd++;
	mRecvBufferLength = 0;

	if ((mBbaMem[BBA_IMR] & INT_R) && !(mBbaMem[BBA_IR] & INT_R))
	{
		if (mBbaMem[2])
		{
			mBbaMem[BBA_IR] |= INT_R;
			INFO_LOG(SP1, "BBA Recv interrupt raised");
			m_bInterruptSet = true;
		}
	}

wait_for_next:
	if (mBbaMem[BBA_NCRA] & NCRA_SR)
		startRecv();

	return true;
}

bool CEXIETHERNET::resume()
{
	if (!isActivated())
		return true;

	INFO_LOG(SP1, "BBA resume");
	//mStop = false;

	RegisterWaitForSingleObject(&mHReadWait, mHRecvEvent, ReadWaitCallback,
		this, INFINITE, WT_EXECUTEDEFAULT);//WT_EXECUTEINWAITTHREAD

	mReadOverlapped.hEvent = mHRecvEvent;

	if (mBbaMem[BBA_NCRA] & NCRA_SR)
		startRecv();

	INFO_LOG(SP1, "BBA resume complete");
	return true;
}

bool CEXIETHERNET::startRecv()
{
	if (!isActivated())
		return false;// Should actually be an assert

	INFO_LOG(SP1, "startRecv... ");

	if (mWaiting)
	{
		INFO_LOG(SP1, "already waiting");
		return true;
	}

	DWORD res = ReadFile(mHAdapter, mRecvBuffer, BBA_RECV_SIZE,
		&mRecvBufferLength, &mReadOverlapped);

	if (res)
	{
		// Operation completed immediately
		INFO_LOG(SP1, "completed, res %i", res);
		mWaiting = true;
	}
	else
	{
		res = GetLastError();
		if (res == ERROR_IO_PENDING)
		{
			//'s ok :)
			INFO_LOG(SP1, "pending");
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
	INFO_LOG(SP1, "WaitCallback %i", cNumber);
	__assume(!TimerFired);
	CEXIETHERNET* self = (CEXIETHERNET*)lpParameter;
	__assume(self->mHAdapter != INVALID_HANDLE_VALUE);
	GetOverlappedResult(self->mHAdapter, &self->mReadOverlapped,
		&self->mRecvBufferLength, false);
	self->mWaiting = false;
	self->handleRecvdPacket();
	INFO_LOG(SP1, "WaitCallback %i done", cNumber);
}

union bba_descr
{
	struct { u32 next_packet_ptr:12, packet_len:12, status:8; };
	u32 word;
};

bool CEXIETHERNET::cbwriteDescriptor(u32 size)
{
	if (size < SIZEOF_ETH_HEADER)
	{
		INFO_LOG(SP1, "Packet too small: %i bytes", size);
		return false;
	}

	// The descriptor supposed to include the size of itself
	size += SIZEOF_RECV_DESCRIPTOR;

	//We should probably not implement wraparound here,
	//since neither tmbinc, riptool.dol, or libogc does...
	if (mCbw.p_write() + SIZEOF_RECV_DESCRIPTOR >= CB_SIZE)
	{
		INFO_LOG(SP1, "The descriptor won't fit");
		return false;
	}
	if (size >= CB_SIZE) 
	{
		INFO_LOG(SP1, "Packet too big: %i bytes", size);
		return false;
	}

	bba_descr descr;
	descr.word = 0;
	descr.packet_len = size;
	descr.status = 0;
	u32 npp;
	if (mCbw.p_write() + size < CB_SIZE)
	{
		npp = (u32)(mCbw.p_write() + size + CB_OFFSET);
	} 
	else
	{
		npp = (u32)(mCbw.p_write() + size + CB_OFFSET - CB_SIZE);
	}

	npp = (npp + 0xff) & ~0xff;

	if (npp >= CB_SIZE + CB_OFFSET)
		npp -= CB_SIZE;

	descr.next_packet_ptr = npp >> 8;
	//DWORD swapped = swapw(descr.word);
	//next_packet_ptr:12, packet_len:12, status:8;
	INFO_LOG(SP1, "Writing descriptor 0x%08X @ 0x%04X: next 0x%03X len 0x%03X status 0x%02X",
		descr.word, mCbw.p_write() + CB_OFFSET, descr.next_packet_ptr,
		descr.packet_len, descr.status);

	mCbw.write(&descr.word, SIZEOF_RECV_DESCRIPTOR);

	return true;
}
//#pragma optimize("",on)