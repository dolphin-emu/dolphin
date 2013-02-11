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

#include <stdio.h>
#include <stdlib.h>
#include <regex>
#include <unordered_set>

#include <windows.h>
#include <dbt.h>
#include <setupapi.h>

#include "Common.h"
#include "WiimoteReal.h"

// Used for pair up
#undef NTDDI_VERSION
#define NTDDI_VERSION	NTDDI_WINXPSP2
#include <bthdef.h>
#include <BluetoothAPIs.h>

typedef struct _HIDD_ATTRIBUTES
{
	ULONG   Size;
	USHORT  VendorID;
	USHORT  ProductID;
	USHORT  VersionNumber;
} HIDD_ATTRIBUTES, *PHIDD_ATTRIBUTES;

typedef VOID (__stdcall *PHidD_GetHidGuid)(LPGUID);
typedef BOOLEAN (__stdcall *PHidD_GetAttributes)(HANDLE, PHIDD_ATTRIBUTES);
typedef BOOLEAN (__stdcall *PHidD_SetOutputReport)(HANDLE, PVOID, ULONG);

typedef BOOL (__stdcall *PBth_BluetoothFindDeviceClose)(HBLUETOOTH_DEVICE_FIND);
typedef HBLUETOOTH_DEVICE_FIND (__stdcall *PBth_BluetoothFindFirstDevice)(const BLUETOOTH_DEVICE_SEARCH_PARAMS*, BLUETOOTH_DEVICE_INFO*);
typedef HBLUETOOTH_RADIO_FIND (__stdcall *PBth_BluetoothFindFirstRadio)(const BLUETOOTH_FIND_RADIO_PARAMS*,HANDLE*);
typedef BOOL (__stdcall *PBth_BluetoothFindNextDevice)(HBLUETOOTH_DEVICE_FIND, BLUETOOTH_DEVICE_INFO*);
typedef BOOL (__stdcall *PBth_BluetoothFindNextRadio)(HBLUETOOTH_RADIO_FIND, HANDLE*);
typedef BOOL (__stdcall *PBth_BluetoothFindRadioClose)(HBLUETOOTH_RADIO_FIND);
typedef DWORD (__stdcall *PBth_BluetoothGetRadioInfo)(HANDLE, PBLUETOOTH_RADIO_INFO);
typedef DWORD (__stdcall *PBth_BluetoothRemoveDevice)(const BLUETOOTH_ADDRESS*);
typedef DWORD (__stdcall *PBth_BluetoothSetServiceState)(HANDLE, const BLUETOOTH_DEVICE_INFO*, const GUID*, DWORD);

PHidD_GetHidGuid HidD_GetHidGuid = NULL;
PHidD_GetAttributes HidD_GetAttributes = NULL;
PHidD_SetOutputReport HidD_SetOutputReport = NULL;

PBth_BluetoothFindDeviceClose Bth_BluetoothFindDeviceClose = NULL;
PBth_BluetoothFindFirstDevice Bth_BluetoothFindFirstDevice = NULL;
PBth_BluetoothFindFirstRadio Bth_BluetoothFindFirstRadio = NULL;
PBth_BluetoothFindNextDevice Bth_BluetoothFindNextDevice = NULL;
PBth_BluetoothFindNextRadio Bth_BluetoothFindNextRadio = NULL;
PBth_BluetoothFindRadioClose Bth_BluetoothFindRadioClose = NULL;
PBth_BluetoothGetRadioInfo Bth_BluetoothGetRadioInfo = NULL;
PBth_BluetoothRemoveDevice Bth_BluetoothRemoveDevice = NULL;
PBth_BluetoothSetServiceState Bth_BluetoothSetServiceState = NULL;

HINSTANCE hid_lib = NULL;
HINSTANCE bthprops_lib = NULL;

static int initialized = 0;

static std::unordered_set<std::string> g_connected_devices;

inline void init_lib()
{
	if (!initialized)
	{
		hid_lib = LoadLibrary(_T("hid.dll"));
		if (!hid_lib)
		{
			PanicAlertT("Failed to load hid.dll");
			exit(EXIT_FAILURE);
		}

		HidD_GetHidGuid = (PHidD_GetHidGuid)GetProcAddress(hid_lib, "HidD_GetHidGuid");
		HidD_GetAttributes = (PHidD_GetAttributes)GetProcAddress(hid_lib, "HidD_GetAttributes");
		HidD_SetOutputReport = (PHidD_SetOutputReport)GetProcAddress(hid_lib, "HidD_SetOutputReport");
		if (!HidD_GetHidGuid || !HidD_GetAttributes || !HidD_SetOutputReport)
		{
			PanicAlertT("Failed to load hid.dll");
			exit(EXIT_FAILURE);
		}

		bthprops_lib = LoadLibrary(_T("bthprops.cpl"));
		if (!bthprops_lib)
		{
			PanicAlertT("Failed to load bthprops.cpl");
			exit(EXIT_FAILURE);
		}

		Bth_BluetoothFindDeviceClose = (PBth_BluetoothFindDeviceClose)GetProcAddress(bthprops_lib, "BluetoothFindDeviceClose");
		Bth_BluetoothFindFirstDevice = (PBth_BluetoothFindFirstDevice)GetProcAddress(bthprops_lib, "BluetoothFindFirstDevice");
		Bth_BluetoothFindFirstRadio = (PBth_BluetoothFindFirstRadio)GetProcAddress(bthprops_lib, "BluetoothFindFirstRadio");
		Bth_BluetoothFindNextDevice = (PBth_BluetoothFindNextDevice)GetProcAddress(bthprops_lib, "BluetoothFindNextDevice");
		Bth_BluetoothFindNextRadio = (PBth_BluetoothFindNextRadio)GetProcAddress(bthprops_lib, "BluetoothFindNextRadio");
		Bth_BluetoothFindRadioClose = (PBth_BluetoothFindRadioClose)GetProcAddress(bthprops_lib, "BluetoothFindRadioClose");
		Bth_BluetoothGetRadioInfo = (PBth_BluetoothGetRadioInfo)GetProcAddress(bthprops_lib, "BluetoothGetRadioInfo");
		Bth_BluetoothRemoveDevice = (PBth_BluetoothRemoveDevice)GetProcAddress(bthprops_lib, "BluetoothRemoveDevice");
		Bth_BluetoothSetServiceState = (PBth_BluetoothSetServiceState)GetProcAddress(bthprops_lib, "BluetoothSetServiceState");

		if (!Bth_BluetoothFindDeviceClose || !Bth_BluetoothFindFirstDevice ||
			!Bth_BluetoothFindFirstRadio || !Bth_BluetoothFindNextDevice ||
			!Bth_BluetoothFindNextRadio || !Bth_BluetoothFindRadioClose ||
			!Bth_BluetoothGetRadioInfo || !Bth_BluetoothRemoveDevice ||
			!Bth_BluetoothSetServiceState)
		{
			PanicAlertT("Failed to load bthprops.cpl");
			exit(EXIT_FAILURE);
		}

		initialized = true;
	}
}

namespace WiimoteReal
{

template <typename T>
void ProcessWiimotes(bool new_scan, T& callback);

bool AttachWiimote(HANDLE hRadio, BLUETOOTH_DEVICE_INFO_STRUCT& btdi);
void RemoveWiimote(HANDLE hRadio, BLUETOOTH_DEVICE_INFO_STRUCT& btdi);
bool ForgetWiimote(HANDLE hRadio, BLUETOOTH_DEVICE_INFO_STRUCT& btdi);

WiimoteScanner::WiimoteScanner()
	: m_run_thread()
	, m_want_wiimotes()
{
	init_lib();
}

WiimoteScanner::~WiimoteScanner()
{
	// TODO: what do we want here?
	ProcessWiimotes(false, RemoveWiimote);
}

void WiimoteScanner::Update()
{
	bool forgot_some = false;

	ProcessWiimotes(false, [&](HANDLE hRadio, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
	{
		forgot_some |= ForgetWiimote(hRadio, btdi);
	});

	// Some hacks that allows disconnects to be detected before connections are handled
	// workaround for wiimote 1 moving to slot 2 on temporary disconnect
	if (forgot_some)
		SLEEP(100);
}

// Find and connect wiimotes.
// Does not replace already found wiimotes even if they are disconnected.
// wm is an array of max_wiimotes wiimotes
// Returns the total number of found and connected wiimotes.
std::vector<Wiimote*> WiimoteScanner::FindWiimotes()
{
	bool attached_some;

	ProcessWiimotes(true, [&](HANDLE hRadio, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
	{
		ForgetWiimote(hRadio, btdi);
		attached_some |= AttachWiimote(hRadio, btdi);
	});

	// Hacks...
	if (attached_some)
		SLEEP(1000);

	GUID device_id;
	HDEVINFO device_info;
	DWORD len;
	SP_DEVICE_INTERFACE_DATA device_data;
	PSP_DEVICE_INTERFACE_DETAIL_DATA detail_data = NULL;

	device_data.cbSize = sizeof(device_data);

	// Get the device id
	HidD_GetHidGuid(&device_id);

	// Get all hid devices connected
	device_info = SetupDiGetClassDevs(&device_id, NULL, NULL, (DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));

	std::vector<Wiimote*> wiimotes;
	for (int index = 0; true; ++index)
	{
		free(detail_data);
		detail_data = NULL;

		// Query the next hid device info
		if (!SetupDiEnumDeviceInterfaces(device_info, NULL, &device_id, index, &device_data))
			break;

		// Get the size of the data block required
		SetupDiGetDeviceInterfaceDetail(device_info, &device_data, NULL, 0, &len, NULL);
		detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(len);
		detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		// Query the data for this device
		if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_data, detail_data, len, NULL, NULL))
			continue;

		auto const wm = new Wiimote;
		wm->devicepath = detail_data->DevicePath;
		wiimotes.push_back(wm);
	}

	free(detail_data);

	SetupDiDestroyDeviceInfoList(device_info);

	return wiimotes;
}

bool WiimoteScanner::IsReady() const
{
	// TODO: don't search for a radio each time
	
	BLUETOOTH_FIND_RADIO_PARAMS radioParam;
	radioParam.dwSize = sizeof(radioParam);

	HANDLE hRadio;
	HBLUETOOTH_RADIO_FIND hFindRadio = Bth_BluetoothFindFirstRadio(&radioParam, &hRadio);

	if (NULL != hFindRadio)
	{
		Bth_BluetoothFindRadioClose(hFindRadio);
		return true;
	}
	else
	{
		return false;
	}
}

// Connect to a wiimote with a known device path.
bool Wiimote::Connect()
{
	// This is where we disallow connecting to the same device twice
	if (g_connected_devices.count(devicepath))
		return false;

	dev_handle = CreateFile(devicepath.c_str(),
		(GENERIC_READ | GENERIC_WRITE),
		// TODO: Just do FILE_SHARE_READ and remove "g_connected_devices"?
		// That is what "WiiYourself" does.
		(FILE_SHARE_READ | FILE_SHARE_WRITE),
		NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if (dev_handle == INVALID_HANDLE_VALUE)
	{
		dev_handle = 0;
		return false;
	}

	hid_overlap = OVERLAPPED();
	hid_overlap.hEvent = CreateEvent(NULL, 1, 1, _T(""));
	hid_overlap.Offset = 0;
	hid_overlap.OffsetHigh = 0;

	// TODO: thread isn't started here now, do this elsewhere
	// This isn't as drastic as it sounds, since the process in which the threads
	// reside is normal priority. Needed for keeping audio reports at a decent rate
/*
	if (!SetThreadPriority(m_wiimote_thread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL))
	{
		ERROR_LOG(WIIMOTE, "Failed to set wiimote thread priority");
	}
*/

	g_connected_devices.insert(devicepath);
	return true;
}

void Wiimote::Disconnect()
{
	g_connected_devices.erase(devicepath);

	CloseHandle(dev_handle);
	dev_handle = 0;

	CloseHandle(hid_overlap.hEvent);
}

bool Wiimote::IsConnected() const
{
	return dev_handle != 0;
}

// positive = read packet
// negative = didn't read packet
// zero = error
int Wiimote::IORead(unsigned char* buf)
{
	// used below for a warning
	*buf = 0;
	
	DWORD bytes;
	ResetEvent(hid_overlap.hEvent);
	if (!ReadFile(dev_handle, buf, MAX_PAYLOAD - 1, &bytes, &hid_overlap))
	{
		auto const err = GetLastError();

		if (ERROR_IO_PENDING == err)
		{
			auto const r = WaitForSingleObject(hid_overlap.hEvent, WIIMOTE_DEFAULT_TIMEOUT);
			if (WAIT_TIMEOUT == r)
			{
				// Timeout - cancel and continue
				if (*buf)
					WARN_LOG(WIIMOTE, "Packet ignored.  This may indicate a problem (timeout is %i ms).",
							WIIMOTE_DEFAULT_TIMEOUT);

				CancelIo(dev_handle);
				bytes = -1;
			}
			else if (WAIT_FAILED == r)
			{
				WARN_LOG(WIIMOTE, "A wait error occured on reading from wiimote %i.", index + 1);
				bytes = 0;
			}
			else if (WAIT_OBJECT_0 == r)
			{
				if (!GetOverlappedResult(dev_handle, &hid_overlap, &bytes, TRUE))
				{
					WARN_LOG(WIIMOTE, "GetOverlappedResult failed on wiimote %i.", index + 1);
					bytes = 0;
				}
			}
			else
			{
				bytes =  0;
			}
		}
		else if (ERROR_HANDLE_EOF == err)
		{
			// Remote disconnect
			bytes = 0;
		}
		else if (ERROR_DEVICE_NOT_CONNECTED == err)
		{
			// Remote disconnect
			bytes = 0;
		}
		else
		{
			bytes = 0;
		}
	}

	if (bytes > 0)
	{
		// Move the data over one, so we can add back in data report indicator byte (here, 0xa1)
		memmove(buf + 1, buf, MAX_PAYLOAD - 1);
		buf[0] = 0xa1;

		// TODO: is this really needed?
		bytes = MAX_PAYLOAD;
	}

	return bytes;
}

int Wiimote::IOWrite(const u8* buf, int len)
{
	DWORD bytes = 0;
	switch (stack)
	{
	case MSBT_STACK_UNKNOWN:
	{
		// Try to auto-detect the stack type
		
		auto i = WriteFile(dev_handle, buf + 1, 22, &bytes, &hid_overlap);
		if (i)
		{
			// Bluesoleil will always return 1 here, even if it's not connected
			stack = MSBT_STACK_BLUESOLEIL;
			return i;
		}

		i = HidD_SetOutputReport(dev_handle, (unsigned char*) buf + 1, len - 1);
		if (i)
		{
			stack = MSBT_STACK_MS;
			return i;
		}

		auto const dw = GetLastError();
		// Checking for 121 = timeout on semaphore/device off/disconnected to
		// avoid trouble with other stacks toshiba/widcomm 
		if (dw == 121)
		{
			NOTICE_LOG(WIIMOTE, "IOWrite[MSBT_STACK_UNKNOWN]: Timeout");
			return 0;
		}
		else
		{
			ERROR_LOG(WIIMOTE, "IOWrite[MSBT_STACK_UNKNOWN]: ERROR: %08x", dw);
			// Correct?
			return -1;
		}
		break;
	}
	case MSBT_STACK_MS:
	{
		auto i = HidD_SetOutputReport(dev_handle, (unsigned char*) buf + 1, len - 1);
		auto dw = GetLastError();

		if (dw == 121)
		{
			// Semaphore timeout
			NOTICE_LOG(WIIMOTE, "WiimoteIOWrite[MSBT_STACK_MS]:  Unable to send data to wiimote");
			return 0;
		}

		return i;
		break;
	}
	case MSBT_STACK_BLUESOLEIL:
		return WriteFile(dev_handle, buf + 1, 22, &bytes, &hid_overlap);
		break;
	}

	return 0;
}

// invokes callback for each found wiimote bluetooth device
template <typename T>
void ProcessWiimotes(bool new_scan, T& callback)
{
	// match strings like "Nintendo RVL-WBC-01", "Nintendo RVL-CNT-01", "Nintendo RVL-CNT-01-TR"
	const std::wregex wiimote_device_name(L"Nintendo RVL-.*");

	BLUETOOTH_DEVICE_SEARCH_PARAMS srch;
	srch.dwSize = sizeof(srch);
	srch.fReturnAuthenticated = true;
	srch.fReturnRemembered = true;
	// Does not filter properly somehow, so we need to do an additional check on
	// fConnected BT Devices
	srch.fReturnConnected = true;
	srch.fReturnUnknown = true;
	srch.fIssueInquiry = new_scan;
	// multiple of 1.28 seconds
	srch.cTimeoutMultiplier = 1;

	BLUETOOTH_FIND_RADIO_PARAMS radioParam;
	radioParam.dwSize = sizeof(radioParam);

	HANDLE hRadio;
	
	// TODO: save radio(s) in the WiimoteScanner constructor?

	// Enumerate BT radios
	HBLUETOOTH_RADIO_FIND hFindRadio = Bth_BluetoothFindFirstRadio(&radioParam, &hRadio);
	while (hFindRadio)
	{
		BLUETOOTH_RADIO_INFO radioInfo;
		radioInfo.dwSize = sizeof(radioInfo);

		// TODO: check for SUCCEEDED()
		Bth_BluetoothGetRadioInfo(hRadio, &radioInfo);

		srch.hRadio = hRadio;

		BLUETOOTH_DEVICE_INFO btdi;
		btdi.dwSize = sizeof(btdi);

		// Enumerate BT devices
		HBLUETOOTH_DEVICE_FIND hFindDevice = Bth_BluetoothFindFirstDevice(&srch, &btdi);
		while (hFindDevice)
		{
			// btdi.szName is sometimes missings it's content - it's a bt feature..
			DEBUG_LOG(WIIMOTE, "authed %i connected %i remembered %i ",
					btdi.fAuthenticated, btdi.fConnected, btdi.fRemembered);

			if (std::regex_match(btdi.szName, wiimote_device_name))
			{
				callback(hRadio, btdi);
			}

			if (false == Bth_BluetoothFindNextDevice(hFindDevice, &btdi))
			{
				Bth_BluetoothFindDeviceClose(hFindDevice);
				hFindDevice = NULL;
			}
		}

		if (false == Bth_BluetoothFindNextRadio(hFindRadio, &hRadio))
		{
			Bth_BluetoothFindRadioClose(hFindRadio);
			hFindRadio = NULL;
		}
	}
}

void RemoveWiimote(HANDLE, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
{
	//if (btdi.fConnected)
	{
		if (SUCCEEDED(Bth_BluetoothRemoveDevice(&btdi.Address)))
		{
			NOTICE_LOG(WIIMOTE, "Removed BT Device", GetLastError());
		}
	}
}

bool AttachWiimote(HANDLE hRadio, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
{
	if (!btdi.fConnected && !btdi.fRemembered)
	{
		NOTICE_LOG(WIIMOTE, "Found wiimote. Enabling HID service.");

		// Activate service
		const DWORD hr = Bth_BluetoothSetServiceState(hRadio, &btdi,
			&HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE);

		if (FAILED(hr))
			ERROR_LOG(WIIMOTE, "Pair-Up: BluetoothSetServiceState() returned %08x", hr);
		else
			return true;
	}

	return false;
}

// Removes remembered non-connected devices
bool ForgetWiimote(HANDLE, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
{
	if (!btdi.fConnected && btdi.fRemembered)
	{
		// We don't want "remembered" devices.
		// SetServiceState seems to just fail with them.
		// Make Windows forget about them.
		NOTICE_LOG(WIIMOTE, "Removing remembered wiimote.");
		Bth_BluetoothRemoveDevice(&btdi.Address);

		return true;
	}

	return false;
}

};
