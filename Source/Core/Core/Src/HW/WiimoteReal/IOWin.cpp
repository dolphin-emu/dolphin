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
#include <algorithm>
#include <unordered_map>
#include <ctime>
#include <unordered_set>

#include <windows.h>
#include <dbt.h>
#include <setupapi.h>

#include "Common.h"
#include "WiimoteReal.h"
#include "StringUtil.h"

// Used for pair up
#undef NTDDI_VERSION
#define NTDDI_VERSION	NTDDI_WINXPSP2
#include <bthdef.h>
#include <BluetoothAPIs.h>

//#define AUTHENTICATE_WIIMOTES
#define SHARE_WRITE_WIIMOTES

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
typedef BOOLEAN (__stdcall *PHidD_GetProductString)(HANDLE, PVOID, ULONG);

typedef BOOL (__stdcall *PBth_BluetoothFindDeviceClose)(HBLUETOOTH_DEVICE_FIND);
typedef HBLUETOOTH_DEVICE_FIND (__stdcall *PBth_BluetoothFindFirstDevice)(const BLUETOOTH_DEVICE_SEARCH_PARAMS*, BLUETOOTH_DEVICE_INFO*);
typedef HBLUETOOTH_RADIO_FIND (__stdcall *PBth_BluetoothFindFirstRadio)(const BLUETOOTH_FIND_RADIO_PARAMS*,HANDLE*);
typedef BOOL (__stdcall *PBth_BluetoothFindNextDevice)(HBLUETOOTH_DEVICE_FIND, BLUETOOTH_DEVICE_INFO*);
typedef BOOL (__stdcall *PBth_BluetoothFindNextRadio)(HBLUETOOTH_RADIO_FIND, HANDLE*);
typedef BOOL (__stdcall *PBth_BluetoothFindRadioClose)(HBLUETOOTH_RADIO_FIND);
typedef DWORD (__stdcall *PBth_BluetoothGetRadioInfo)(HANDLE, PBLUETOOTH_RADIO_INFO);
typedef DWORD (__stdcall *PBth_BluetoothRemoveDevice)(const BLUETOOTH_ADDRESS*);
typedef DWORD (__stdcall *PBth_BluetoothSetServiceState)(HANDLE, const BLUETOOTH_DEVICE_INFO*, const GUID*, DWORD);
typedef DWORD (__stdcall *PBth_BluetoothAuthenticateDevice)(HWND, HANDLE, BLUETOOTH_DEVICE_INFO*, PWCHAR, ULONG);
typedef DWORD (__stdcall *PBth_BluetoothEnumerateInstalledServices)(HANDLE,	BLUETOOTH_DEVICE_INFO*, DWORD*, GUID*);

PHidD_GetHidGuid HidD_GetHidGuid = NULL;
PHidD_GetAttributes HidD_GetAttributes = NULL;
PHidD_SetOutputReport HidD_SetOutputReport = NULL;
PHidD_GetProductString HidD_GetProductString = NULL;

PBth_BluetoothFindDeviceClose Bth_BluetoothFindDeviceClose = NULL;
PBth_BluetoothFindFirstDevice Bth_BluetoothFindFirstDevice = NULL;
PBth_BluetoothFindFirstRadio Bth_BluetoothFindFirstRadio = NULL;
PBth_BluetoothFindNextDevice Bth_BluetoothFindNextDevice = NULL;
PBth_BluetoothFindNextRadio Bth_BluetoothFindNextRadio = NULL;
PBth_BluetoothFindRadioClose Bth_BluetoothFindRadioClose = NULL;
PBth_BluetoothGetRadioInfo Bth_BluetoothGetRadioInfo = NULL;
PBth_BluetoothRemoveDevice Bth_BluetoothRemoveDevice = NULL;
PBth_BluetoothSetServiceState Bth_BluetoothSetServiceState = NULL;
PBth_BluetoothAuthenticateDevice Bth_BluetoothAuthenticateDevice = NULL;
PBth_BluetoothEnumerateInstalledServices Bth_BluetoothEnumerateInstalledServices = NULL;

HINSTANCE hid_lib = NULL;
HINSTANCE bthprops_lib = NULL;

static int initialized = 0;

std::unordered_map<BTH_ADDR, std::time_t> g_connect_times;

#ifdef SHARE_WRITE_WIIMOTES
std::unordered_set<std::basic_string<TCHAR>> g_connected_wiimotes;
std::mutex g_connected_wiimotes_lock;
#endif

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
		HidD_GetProductString = (PHidD_GetProductString)GetProcAddress(hid_lib, "HidD_GetProductString");
		if (!HidD_GetHidGuid || !HidD_GetAttributes ||
			!HidD_SetOutputReport || !HidD_GetProductString)
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
		Bth_BluetoothAuthenticateDevice = (PBth_BluetoothAuthenticateDevice)GetProcAddress(bthprops_lib, "BluetoothAuthenticateDevice");
		Bth_BluetoothEnumerateInstalledServices = (PBth_BluetoothEnumerateInstalledServices)GetProcAddress(bthprops_lib, "BluetoothEnumerateInstalledServices");

		if (!Bth_BluetoothFindDeviceClose || !Bth_BluetoothFindFirstDevice ||
			!Bth_BluetoothFindFirstRadio || !Bth_BluetoothFindNextDevice ||
			!Bth_BluetoothFindNextRadio || !Bth_BluetoothFindRadioClose ||
			!Bth_BluetoothGetRadioInfo || !Bth_BluetoothRemoveDevice ||
			!Bth_BluetoothSetServiceState || !Bth_BluetoothAuthenticateDevice ||
			!Bth_BluetoothEnumerateInstalledServices)
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

bool AttachWiimote(HANDLE hRadio, const BLUETOOTH_RADIO_INFO&, BLUETOOTH_DEVICE_INFO_STRUCT&);
void RemoveWiimote(BLUETOOTH_DEVICE_INFO_STRUCT&);
bool ForgetWiimote(BLUETOOTH_DEVICE_INFO_STRUCT&);

WiimoteScanner::WiimoteScanner()
	: m_run_thread()
	, m_want_wiimotes()
{
	init_lib();
}

WiimoteScanner::~WiimoteScanner()
{
	// TODO: what do we want here?
#if 0
	ProcessWiimotes(false, [](HANDLE, BLUETOOTH_RADIO_INFO&, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
	{
		RemoveWiimote(btdi);
	});
#endif
}

void WiimoteScanner::Update()
{
	bool forgot_some = false;

	ProcessWiimotes(false, [&](HANDLE, BLUETOOTH_RADIO_INFO&, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
	{
		forgot_some |= ForgetWiimote(btdi);
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
	ProcessWiimotes(true, [](HANDLE hRadio, const BLUETOOTH_RADIO_INFO& rinfo, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
	{
		ForgetWiimote(btdi);
		AttachWiimote(hRadio, rinfo, btdi);
	});

	// Get the device id
	GUID device_id;
	HidD_GetHidGuid(&device_id);

	// Get all hid devices connected
	HDEVINFO const device_info = SetupDiGetClassDevs(&device_id, NULL, NULL, (DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));

	std::vector<Wiimote*> wiimotes;

	SP_DEVICE_INTERFACE_DATA device_data;
	device_data.cbSize = sizeof(device_data);
	PSP_DEVICE_INTERFACE_DETAIL_DATA detail_data = NULL;

	for (int index = 0; SetupDiEnumDeviceInterfaces(device_info, NULL, &device_id, index, &device_data); ++index)
	{
		// Get the size of the data block required
		DWORD len;
		SetupDiGetDeviceInterfaceDetail(device_info, &device_data, NULL, 0, &len, NULL);
		detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(len);
		detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		// Query the data for this device
		if (SetupDiGetDeviceInterfaceDetail(device_info, &device_data, detail_data, len, NULL, NULL))
		{
			auto const wm = new Wiimote;
			wm->devicepath = detail_data->DevicePath;
			wiimotes.push_back(wm);
		}

		free(detail_data);
	}

	SetupDiDestroyDeviceInfoList(device_info);

	// Don't mind me, just a random sleep to fix stuff on Windows
	//if (!wiimotes.empty())
	//	SLEEP(2000);

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
	if (IsConnected())
		return false;

#ifdef SHARE_WRITE_WIIMOTES
	std::lock_guard<std::mutex> lk(g_connected_wiimotes_lock);
	if (g_connected_wiimotes.count(devicepath) != 0)
		return false;

	auto const open_flags = FILE_SHARE_READ | FILE_SHARE_WRITE;
#else
	// Having no FILE_SHARE_WRITE disallows us from connecting to the same wiimote twice.
	// (And disallows using wiimotes in use by other programs)
	// This is what "WiiYourself" does.
	// Apparently this doesn't work for everyone. It might be their fault.
	auto const open_flags = FILE_SHARE_READ;
#endif

	dev_handle = CreateFile(devicepath.c_str(),
		GENERIC_READ | GENERIC_WRITE, open_flags,
		NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

	if (dev_handle == INVALID_HANDLE_VALUE)
	{
		dev_handle = 0;
		return false;
	}

#if 0
	TCHAR name[128] = {};
	HidD_GetProductString(dev_handle, name, 128);

	//ERROR_LOG(WIIMOTE, "product string: %s", TStrToUTF8(name).c_str());

	if (!IsValidBluetoothName(TStrToUTF8(name)))
	{
		CloseHandle(dev_handle);
		dev_handle = 0;
		return false;
	}
#endif

#if 0
	HIDD_ATTRIBUTES attr;
	attr.Size = sizeof(attr);
	if (!HidD_GetAttributes(dev_handle, &attr))
	{
		CloseHandle(dev_handle);
		dev_handle = 0;
		return false;
	}
#endif

	hid_overlap_read = OVERLAPPED();
	hid_overlap_read.hEvent = CreateEvent(NULL, true, false, NULL);

	hid_overlap_write = OVERLAPPED();
	hid_overlap_write.hEvent = CreateEvent(NULL, true, false, NULL);

	// TODO: thread isn't started here now, do this elsewhere
	// This isn't as drastic as it sounds, since the process in which the threads
	// reside is normal priority. Needed for keeping audio reports at a decent rate
/*
	if (!SetThreadPriority(m_wiimote_thread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL))
	{
		ERROR_LOG(WIIMOTE, "Failed to set wiimote thread priority");
	}
*/
#ifdef SHARE_WRITE_WIIMOTES
	g_connected_wiimotes.insert(devicepath);
#endif

	return true;
}

void Wiimote::Disconnect()
{
	if (!IsConnected())
		return;

	CloseHandle(dev_handle);
	dev_handle = 0;

	CloseHandle(hid_overlap_read.hEvent);
	CloseHandle(hid_overlap_write.hEvent);

#ifdef SHARE_WRITE_WIIMOTES
	std::lock_guard<std::mutex> lk(g_connected_wiimotes_lock);
	g_connected_wiimotes.erase(devicepath);
#endif
}

bool Wiimote::IsConnected() const
{
	return dev_handle != 0;
}

// positive = read packet
// negative = didn't read packet
// zero = error
int Wiimote::IORead(u8* buf)
{
	// used below for a warning
	*buf = 0;
	
	DWORD bytes;
	ResetEvent(hid_overlap_read.hEvent);
	if (!ReadFile(dev_handle, buf, MAX_PAYLOAD - 1, &bytes, &hid_overlap_read))
	{
		auto const err = GetLastError();

		if (ERROR_IO_PENDING == err)
		{
			auto const r = WaitForSingleObject(hid_overlap_read.hEvent, WIIMOTE_DEFAULT_TIMEOUT);
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
				if (!GetOverlappedResult(dev_handle, &hid_overlap_read, &bytes, TRUE))
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
		std::copy_n(buf, MAX_PAYLOAD - 1, buf + 1);
		buf[0] = 0xa1;

		// TODO: is this really needed?
		bytes = MAX_PAYLOAD;
	}

	return bytes;
}

int Wiimote::IOWrite(const u8* buf, int len)
{
	switch (stack)
	{
	case MSBT_STACK_UNKNOWN:
	{
		// Try to auto-detect the stack type
		stack = MSBT_STACK_BLUESOLEIL;
		if (IOWrite(buf, len))
			return 1;

		stack = MSBT_STACK_MS;
		if (IOWrite(buf, len))
			return 1;

		stack = MSBT_STACK_UNKNOWN;
		break;
	}
	case MSBT_STACK_MS:
	{
		auto result = HidD_SetOutputReport(dev_handle, const_cast<u8*>(buf) + 1, len - 1);
		//FlushFileBuffers(dev_handle);

		if (!result)
		{
			auto err = GetLastError();
			if (err == 121)
			{
				// Semaphore timeout
				NOTICE_LOG(WIIMOTE, "WiimoteIOWrite[MSBT_STACK_MS]:  Unable to send data to wiimote");
			}
			else
			{
				WARN_LOG(WIIMOTE, "IOWrite[MSBT_STACK_MS]: ERROR: %08x", err);
			}
		}

		return result;
		break;
	}
	case MSBT_STACK_BLUESOLEIL:
	{
		u8 big_buf[MAX_PAYLOAD];
		if (len < MAX_PAYLOAD)
		{
			std::copy(buf, buf + len, big_buf);
			std::fill(big_buf + len, big_buf + MAX_PAYLOAD, 0);
			buf = big_buf;
		}

		ResetEvent(hid_overlap_write.hEvent);
		DWORD bytes = 0;
		if (WriteFile(dev_handle, buf + 1, MAX_PAYLOAD - 1, &bytes, &hid_overlap_write))
		{
			// WriteFile always returns true with bluesoleil.
			return 1;
		}
		else
		{
			auto const err = GetLastError();
			if (ERROR_IO_PENDING == err)
			{
				CancelIo(dev_handle);
			}
		}
		break;
	}
	}

	return 0;
}

// invokes callback for each found wiimote bluetooth device
template <typename T>
void ProcessWiimotes(bool new_scan, T& callback)
{
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
	srch.cTimeoutMultiplier = 2;

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

		auto const rinfo_result = Bth_BluetoothGetRadioInfo(hRadio, &radioInfo);
		if (ERROR_SUCCESS == rinfo_result)
		{
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

				if (IsValidBluetoothName(UTF16ToUTF8(btdi.szName)))
				{
					callback(hRadio, radioInfo, btdi);
				}

				if (false == Bth_BluetoothFindNextDevice(hFindDevice, &btdi))
				{
					Bth_BluetoothFindDeviceClose(hFindDevice);
					hFindDevice = NULL;
				}
			}
		}

		if (false == Bth_BluetoothFindNextRadio(hFindRadio, &hRadio))
		{
			Bth_BluetoothFindRadioClose(hFindRadio);
			hFindRadio = NULL;
		}
	}
}

void RemoveWiimote(BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
{
	//if (btdi.fConnected)
	{
		if (SUCCEEDED(Bth_BluetoothRemoveDevice(&btdi.Address)))
		{
			NOTICE_LOG(WIIMOTE, "Removed BT Device", GetLastError());
		}
	}
}

bool AttachWiimote(HANDLE hRadio, const BLUETOOTH_RADIO_INFO& radio_info, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
{
	// We don't want "remembered" devices.
	// SetServiceState will just fail with them..
	if (!btdi.fConnected && !btdi.fRemembered)
	{
		auto const& wm_addr = btdi.Address.rgBytes;

		NOTICE_LOG(WIIMOTE, "Found wiimote (%02x:%02x:%02x:%02x:%02x:%02x). Enabling HID service.",
			wm_addr[0], wm_addr[1], wm_addr[2], wm_addr[3], wm_addr[4], wm_addr[5]);

#if defined(AUTHENTICATE_WIIMOTES)
		// Authenticate
		auto const& radio_addr = radio_info.address.rgBytes;
		const DWORD auth_result = Bth_BluetoothAuthenticateDevice(NULL, hRadio, &btdi,
			std::vector<WCHAR>(radio_addr, radio_addr + 6).data(), 6);

		if (ERROR_SUCCESS != auth_result)
			ERROR_LOG(WIIMOTE, "AttachWiimote: BluetoothAuthenticateDevice returned %08x", auth_result);

		DWORD pcServices = 16;
		GUID guids[16];
		// If this is not done, the Wii device will not remember the pairing
		const DWORD srv_result = Bth_BluetoothEnumerateInstalledServices(hRadio, &btdi, &pcServices, guids);

		if (ERROR_SUCCESS != srv_result)
			ERROR_LOG(WIIMOTE, "AttachWiimote: BluetoothEnumerateInstalledServices returned %08x", srv_result);
#endif
		// Activate service
		const DWORD hr = Bth_BluetoothSetServiceState(hRadio, &btdi,
			&HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE);

		g_connect_times[btdi.Address.ullLong] = std::time(nullptr);

		if (FAILED(hr))
			ERROR_LOG(WIIMOTE, "AttachWiimote: BluetoothSetServiceState returned %08x", hr);
		else
			return true;
	}

	return false;
}

// Removes remembered non-connected devices
bool ForgetWiimote(BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
{
	if (!btdi.fConnected && btdi.fRemembered)
	{
		// Time to avoid RemoveDevice after SetServiceState.
		// Sometimes SetServiceState takes a while..
		auto const avoid_forget_seconds = 5.0;

		auto pair_time = g_connect_times.find(btdi.Address.ullLong);
		if (pair_time == g_connect_times.end()
			|| std::difftime(time(nullptr), pair_time->second) >= avoid_forget_seconds)
		{
			// Make Windows forget about device so it will re-find it if visible.
			// This is also required to detect a disconnect for some reason..
			NOTICE_LOG(WIIMOTE, "Removing remembered wiimote.");
			Bth_BluetoothRemoveDevice(&btdi.Address);
			return true;
		}
	}

	return false;
}

};
