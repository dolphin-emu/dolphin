// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/StringUtil.h"

#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceEthernet.h"
#include "Core/HW/BBA-TAP/TAP_Win32.h"

namespace Win32TAPHelper
{

bool IsTAPDevice(const TCHAR *guid)
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
		TCHAR enum_name[256];
		TCHAR unit_string[256];
		HKEY unit_key;
		TCHAR component_id_string[] = _T("ComponentId");
		TCHAR component_id[256];
		TCHAR net_cfg_instance_id_string[] = _T("NetCfgInstanceId");
		TCHAR net_cfg_instance_id[256];
		DWORD data_type;

		len = sizeof(enum_name);
		status = RegEnumKeyEx(netcard_key, i, enum_name, &len, nullptr, nullptr, nullptr, nullptr);

		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS)
			return false;

		_sntprintf(unit_string, sizeof(unit_string), _T("%s\\%s"), ADAPTER_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, unit_string, 0, KEY_READ, &unit_key);

		if (status != ERROR_SUCCESS)
		{
			return false;
		}
		else
		{
			len = sizeof(component_id);
			status = RegQueryValueEx(unit_key, component_id_string, nullptr,
				&data_type, (LPBYTE)component_id, &len);

			if (!(status != ERROR_SUCCESS || data_type != REG_SZ))
			{
				len = sizeof(net_cfg_instance_id);
				status = RegQueryValueEx(unit_key, net_cfg_instance_id_string, nullptr,
					&data_type, (LPBYTE)net_cfg_instance_id, &len);

				if (status == ERROR_SUCCESS && data_type == REG_SZ)
				{
					if (!_tcscmp(component_id, TAP_COMPONENT_ID) &&
						!_tcscmp(net_cfg_instance_id, guid))
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

bool GetGUIDs(std::vector<std::basic_string<TCHAR>>& guids)
{
	LONG status;
	HKEY control_net_key;
	DWORD len;
	int i = 0;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ, &control_net_key);

	if (status != ERROR_SUCCESS)
		return false;

	for (;; i++)
	{
		TCHAR enum_name[256];
		TCHAR connection_string[256];
		HKEY connection_key;
		TCHAR name_data[256];
		DWORD name_type;
		const TCHAR name_string[] = _T("Name");

		len = sizeof(enum_name);
		status = RegEnumKeyEx(control_net_key, i, enum_name,
			&len, nullptr, nullptr, nullptr, nullptr);

		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS)
			continue;

		_sntprintf(connection_string, sizeof(connection_string),
			_T("%s\\%s\\Connection"), NETWORK_CONNECTIONS_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, connection_string,
			0, KEY_READ, &connection_key);

		if (status == ERROR_SUCCESS)
		{
			len = sizeof(name_data);
			status = RegQueryValueEx(connection_key, name_string, nullptr,
				&name_type, (LPBYTE)name_data, &len);

			if (status != ERROR_SUCCESS || name_type != REG_SZ)
			{
				continue;
			}
			else
			{
				if (IsTAPDevice(enum_name))
				{
					guids.push_back(enum_name);
				}
			}

			RegCloseKey(connection_key);
		}
	}

	RegCloseKey(control_net_key);

	return !guids.empty();
}

bool OpenTAP(HANDLE& adapter, const std::basic_string<TCHAR>& device_guid)
{
	auto const device_path = USERMODEDEVICEDIR + device_guid + TAPSUFFIX;

	adapter = CreateFile(device_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0,
		OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, 0);

	if (adapter == INVALID_HANDLE_VALUE)
	{
		INFO_LOG(SP1, "Failed to open TAP at %s", device_path);
		return false;
	}
	return true;
}

} // namespace Win32TAPHelper

bool CEXIETHERNET::Activate()
{
	if (IsActivated())
		return true;

	DWORD len;
	std::vector<std::basic_string<TCHAR>> device_guids;

	if (!Win32TAPHelper::GetGUIDs(device_guids))
	{
		ERROR_LOG(SP1, "Failed to find a TAP GUID");
		return false;
	}

	for (size_t i = 0; i < device_guids.size(); i++)
	{
		if (Win32TAPHelper::OpenTAP(mHAdapter, device_guids.at(i)))
		{
			INFO_LOG(SP1, "OPENED %s", device_guids.at(i).c_str());
			break;
		}
	}
	if (mHAdapter == INVALID_HANDLE_VALUE)
	{
		PanicAlert("Failed to open any TAP");
		return false;
	}

	/* get driver version info */
	ULONG info[3];
	if (DeviceIoControl(mHAdapter, TAP_IOCTL_GET_VERSION,
		&info, sizeof(info), &info, sizeof(info), &len, nullptr))
	{
		INFO_LOG(SP1, "TAP-Win32 Driver Version %d.%d %s",
			info[0], info[1], info[2] ? "(DEBUG)" : "");
	}
	if (!(info[0] > TAP_WIN32_MIN_MAJOR || (info[0] == TAP_WIN32_MIN_MAJOR && info[1] >= TAP_WIN32_MIN_MINOR)))
	{
		PanicAlertT("ERROR: This version of Dolphin requires a TAP-Win32 driver"
			" that is at least version %d.%d -- If you recently upgraded your Dolphin"
			" distribution, a reboot is probably required at this point to get"
			" Windows to see the new driver.",
			TAP_WIN32_MIN_MAJOR, TAP_WIN32_MIN_MINOR);
		return false;
	}

	/* set driver media status to 'connected' */
	ULONG status = TRUE;
	if (!DeviceIoControl(mHAdapter, TAP_IOCTL_SET_MEDIA_STATUS,
		&status, sizeof(status), &status, sizeof(status), &len, nullptr))
	{
		ERROR_LOG(SP1, "WARNING: The TAP-Win32 driver rejected a"
			"TAP_IOCTL_SET_MEDIA_STATUS DeviceIoControl call.");
		return false;
	}

	return true;
}

void CEXIETHERNET::Deactivate()
{
	if (!IsActivated())
		return;

	RecvStop();

	CloseHandle(mHAdapter);
	mHAdapter = INVALID_HANDLE_VALUE;
}

bool CEXIETHERNET::IsActivated()
{
	return mHAdapter != INVALID_HANDLE_VALUE;
}

bool CEXIETHERNET::SendFrame(u8 *frame, u32 size)
{
	DEBUG_LOG(SP1, "SendFrame %x\n%s",
		size, ArrayToString(frame, size, 0x10).c_str());

	OVERLAPPED overlap;
	ZeroMemory(&overlap, sizeof(overlap));

	//WriteFile will always return false because the TAP handle is async
	WriteFile(mHAdapter, frame, size, NULL, &overlap);

	DWORD res = GetLastError();
	if (res != ERROR_IO_PENDING)
	{
		ERROR_LOG(SP1, "Failed to send packet with error 0x%X", res);
	}

	// Always report the packet as being sent successfully, even though it might be a lie
	SendComplete();

	return true;
}

VOID CALLBACK CEXIETHERNET::ReadWaitCallback(PVOID lpParameter, BOOLEAN TimerFired)
{
	CEXIETHERNET* self = (CEXIETHERNET*)lpParameter;

	GetOverlappedResult(self->mHAdapter, &self->mReadOverlapped,
		(LPDWORD)&self->mRecvBufferLength, false);

	self->RecvHandlePacket();
}

bool CEXIETHERNET::RecvInit()
{
	// Set up recv event

	if ((mHRecvEvent = CreateEvent(nullptr, false, false, nullptr)) == nullptr)
	{
		ERROR_LOG(SP1, "Failed to create recv event:%x", GetLastError());
		return false;
	}

	ZeroMemory(&mReadOverlapped, sizeof(mReadOverlapped));

	RegisterWaitForSingleObject(&mHReadWait, mHRecvEvent, ReadWaitCallback,
		this, INFINITE, WT_EXECUTEDEFAULT);

	mReadOverlapped.hEvent = mHRecvEvent;

	return true;
}

bool CEXIETHERNET::RecvStart()
{
	if (!IsActivated())
		return false;

	if (mHRecvEvent == INVALID_HANDLE_VALUE)
		RecvInit();

	DWORD res = ReadFile(mHAdapter, mRecvBuffer, BBA_RECV_SIZE,
		(LPDWORD)&mRecvBufferLength, &mReadOverlapped);

	if (!res && (GetLastError() != ERROR_IO_PENDING))
	{
		// error occurred
		return false;
	}

	if (res)
	{
		// Completed immediately
		RecvHandlePacket();
	}

	return true;
}

void CEXIETHERNET::RecvStop()
{
	if (!IsActivated())
		return;

	UnregisterWaitEx(mHReadWait, INVALID_HANDLE_VALUE);

	CloseHandle(mHRecvEvent);
	mHRecvEvent = INVALID_HANDLE_VALUE;
}
