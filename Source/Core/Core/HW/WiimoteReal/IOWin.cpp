// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Used for pair up
#undef NTDDI_VERSION
#define NTDDI_VERSION  NTDDI_WINXPSP2

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unordered_map>
#include <unordered_set>
#include <windows.h>
// The following Windows headers MUST be included after windows.h.
#include <BluetoothAPIs.h> //NOLINT
#include <dbt.h>           //NOLINT
#include <setupapi.h>      //NOLINT

#include "Common/Common.h"
#include "Common/StringUtil.h"

#include "Core/HW/WiimoteReal/WiimoteReal.h"

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
typedef DWORD (__stdcall *PBth_BluetoothEnumerateInstalledServices)(HANDLE, BLUETOOTH_DEVICE_INFO*, DWORD*, GUID*);

PHidD_GetHidGuid HidD_GetHidGuid = nullptr;
PHidD_GetAttributes HidD_GetAttributes = nullptr;
PHidD_SetOutputReport HidD_SetOutputReport = nullptr;
PHidD_GetProductString HidD_GetProductString = nullptr;

PBth_BluetoothFindDeviceClose Bth_BluetoothFindDeviceClose = nullptr;
PBth_BluetoothFindFirstDevice Bth_BluetoothFindFirstDevice = nullptr;
PBth_BluetoothFindFirstRadio Bth_BluetoothFindFirstRadio = nullptr;
PBth_BluetoothFindNextDevice Bth_BluetoothFindNextDevice = nullptr;
PBth_BluetoothFindNextRadio Bth_BluetoothFindNextRadio = nullptr;
PBth_BluetoothFindRadioClose Bth_BluetoothFindRadioClose = nullptr;
PBth_BluetoothGetRadioInfo Bth_BluetoothGetRadioInfo = nullptr;
PBth_BluetoothRemoveDevice Bth_BluetoothRemoveDevice = nullptr;
PBth_BluetoothSetServiceState Bth_BluetoothSetServiceState = nullptr;
PBth_BluetoothAuthenticateDevice Bth_BluetoothAuthenticateDevice = nullptr;
PBth_BluetoothEnumerateInstalledServices Bth_BluetoothEnumerateInstalledServices = nullptr;

HINSTANCE hid_lib = nullptr;
HINSTANCE bthprops_lib = nullptr;

static bool initialized = false;

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
			PanicAlertT("Failed to load hid.dll! Connecting real Wiimotes won't work and Dolphin might crash unexpectedly!");
			return;
		}

		HidD_GetHidGuid = (PHidD_GetHidGuid)GetProcAddress(hid_lib, "HidD_GetHidGuid");
		HidD_GetAttributes = (PHidD_GetAttributes)GetProcAddress(hid_lib, "HidD_GetAttributes");
		HidD_SetOutputReport = (PHidD_SetOutputReport)GetProcAddress(hid_lib, "HidD_SetOutputReport");
		HidD_GetProductString = (PHidD_GetProductString)GetProcAddress(hid_lib, "HidD_GetProductString");
		if (!HidD_GetHidGuid || !HidD_GetAttributes ||
		    !HidD_SetOutputReport || !HidD_GetProductString)
		{
			PanicAlertT("Failed to load hid.dll! Connecting real Wiimotes won't work and Dolphin might crash unexpectedly!");
			return;
		}

		bthprops_lib = LoadLibrary(_T("bthprops.cpl"));
		if (!bthprops_lib)
		{
			PanicAlertT("Failed to load bthprops.cpl! Connecting real Wiimotes won't work and Dolphin might crash unexpectedly!");
			return;
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
			PanicAlertT("Failed to load bthprops.cpl! Connecting real Wiimotes won't work and Dolphin might crash unexpectedly!");
			return;
		}

		initialized = true;
	}
}

namespace WiimoteReal
{


int _IOWrite(HANDLE &dev_handle, OVERLAPPED &hid_overlap_write, enum win_bt_stack_t &stack, const u8* buf, size_t len);
int _IORead(HANDLE &dev_handle, OVERLAPPED &hid_overlap_read, u8* buf, int index);
void _IOWakeup(HANDLE &dev_handle, OVERLAPPED &hid_overlap_read);

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
void WiimoteScanner::FindWiimotes(std::vector<Wiimote*> & found_wiimotes, Wiimote* & found_board)
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
	HDEVINFO const device_info = SetupDiGetClassDevs(&device_id, nullptr, nullptr, (DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));

	SP_DEVICE_INTERFACE_DATA device_data;
	device_data.cbSize = sizeof(device_data);
	PSP_DEVICE_INTERFACE_DETAIL_DATA detail_data = nullptr;

	for (int index = 0; SetupDiEnumDeviceInterfaces(device_info, nullptr, &device_id, index, &device_data); ++index)
	{
		// Get the size of the data block required
		DWORD len;
		SetupDiGetDeviceInterfaceDetail(device_info, &device_data, nullptr, 0, &len, nullptr);
		detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(len);
		detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		// Query the data for this device
		if (SetupDiGetDeviceInterfaceDetail(device_info, &device_data, detail_data, len, nullptr, nullptr))
		{
			auto const wm = new Wiimote;
			wm->devicepath = detail_data->DevicePath;
			bool real_wiimote = false, is_bb = false;

			CheckDeviceType(wm->devicepath, real_wiimote, is_bb);
			if (is_bb)
			{
				found_board = wm;
			}
			else if (real_wiimote)
			{
				found_wiimotes.push_back(wm);
			}
			else
			{
				delete wm;
			}
		}

		free(detail_data);
	}

	SetupDiDestroyDeviceInfoList(device_info);

	// Don't mind me, just a random sleep to fix stuff on Windows
	//if (!wiimotes.empty())
	//    SLEEP(2000);

}
int CheckDeviceType_Write(HANDLE &dev_handle, const u8* buf, size_t size, int attempts)
{
	OVERLAPPED hid_overlap_write = OVERLAPPED();
	hid_overlap_write.hEvent = CreateEvent(nullptr, true, false, nullptr);
	enum win_bt_stack_t stack = MSBT_STACK_UNKNOWN;

	DWORD written = 0;

	for (; attempts>0; --attempts)
	{
		if (_IOWrite(dev_handle, hid_overlap_write, stack, buf, size))
		{
			auto const wait_result = WaitForSingleObject(hid_overlap_write.hEvent, WIIMOTE_DEFAULT_TIMEOUT);
			if (WAIT_TIMEOUT == wait_result)
			{
				WARN_LOG(WIIMOTE, "CheckDeviceType_Write: A timeout occurred on writing to Wiimote.");
				CancelIo(dev_handle);
				continue;
			}
			else if (WAIT_FAILED == wait_result)
			{
				WARN_LOG(WIIMOTE, "CheckDeviceType_Write: A wait error occurred on writing to Wiimote.");
				CancelIo(dev_handle);
				continue;
			}
			if (GetOverlappedResult(dev_handle, &hid_overlap_write, &written, TRUE))
			{
				break;
			}
		}
	}

	CloseHandle(hid_overlap_write.hEvent);

	return written;
}

int CheckDeviceType_Read(HANDLE &dev_handle, u8* buf, int attempts)
{
	OVERLAPPED hid_overlap_read = OVERLAPPED();
	hid_overlap_read.hEvent = CreateEvent(nullptr, true, false, nullptr);
	int read = 0;
	for (; attempts>0; --attempts)
	{
		read = _IORead(dev_handle, hid_overlap_read, buf, 1);
		if (read > 0)
			break;
	}

	CloseHandle(hid_overlap_read.hEvent);

	return read;
}

// A convoluted way of checking if a device is a Wii Balance Board and if it is a connectable Wiimote.
// Because nothing on Windows should be easy.
// (We can't seem to easily identify the bluetooth device an HID device belongs to...)
void WiimoteScanner::CheckDeviceType(std::basic_string<TCHAR> &devicepath, bool &real_wiimote, bool &is_bb)
{
	real_wiimote = false;
	is_bb = false;

#ifdef SHARE_WRITE_WIIMOTES
	std::lock_guard<std::mutex> lk(g_connected_wiimotes_lock);
	if (g_connected_wiimotes.count(devicepath) != 0)
		return;
#endif

	HANDLE dev_handle = CreateFile(devicepath.c_str(),
	                               GENERIC_READ | GENERIC_WRITE,
	                               FILE_SHARE_READ | FILE_SHARE_WRITE,
	                               nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED,
	                               nullptr);
	if (dev_handle == INVALID_HANDLE_VALUE)
		return;
	// enable to only check for official nintendo wiimotes/bb's
	bool check_vidpid = false;
	HIDD_ATTRIBUTES attrib;
	attrib.Size = sizeof(attrib);
	if (!check_vidpid ||
		(HidD_GetAttributes(dev_handle, &attrib) &&
		(attrib.VendorID == 0x057e) &&
		(attrib.ProductID == 0x0306)))
	{
		// max_cycles insures we are never stuck here due to bad coding...
		int max_cycles = 20;
		u8 buf[MAX_PAYLOAD] = {0};

		u8 const req_status_report[] = {WM_SET_REPORT | WM_BT_OUTPUT, WM_REQUEST_STATUS, 0};
		// The new way to initialize the extension is by writing 0x55 to 0x(4)A400F0, then writing 0x00 to 0x(4)A400FB
		// 52 16 04 A4 00 F0 01 55
		// 52 16 04 A4 00 FB 01 00
		u8 const disable_enc_pt1_report[MAX_PAYLOAD] = {WM_SET_REPORT | WM_BT_OUTPUT, WM_WRITE_DATA, 0x04, 0xa4, 0x00, 0xf0, 0x01, 0x55};
		u8 const disable_enc_pt2_report[MAX_PAYLOAD] = {WM_SET_REPORT | WM_BT_OUTPUT, WM_WRITE_DATA, 0x04, 0xa4, 0x00, 0xfb, 0x01, 0x00};

		CheckDeviceType_Write(dev_handle,
		                      disable_enc_pt1_report,
		                      sizeof(disable_enc_pt1_report),
		                      1);
		CheckDeviceType_Write(dev_handle,
		                      disable_enc_pt2_report,
		                      sizeof(disable_enc_pt2_report),
		                      1);

		int rc = CheckDeviceType_Write(dev_handle,
		                               req_status_report,
		                               sizeof(req_status_report),
		                               1);

		while (rc > 0 && --max_cycles > 0)
		{
			if ((rc = CheckDeviceType_Read(dev_handle, buf, 1)) <= 0)
			{
				// DEBUG_LOG(WIIMOTE, "CheckDeviceType: Read failed...");
				break;
			}

			switch (buf[1])
			{
				case WM_STATUS_REPORT:
				{
					real_wiimote = true;

					// DEBUG_LOG(WIIMOTE, "CheckDeviceType: Got Status Report");
					wm_status_report * wsr = (wm_status_report*)&buf[2];
					if (wsr->extension)
					{
						// Wiimote with extension, we ask it what kind.
						u8 read_ext[MAX_PAYLOAD] = {0};
						read_ext[0] = WM_SET_REPORT | WM_BT_OUTPUT;
						read_ext[1] = WM_READ_DATA;
						// Extension type register.
						*(u32*)&read_ext[2] = Common::swap32(0x4a400fa);
						// Size.
						*(u16*)&read_ext[6] = Common::swap16(6);
						rc = CheckDeviceType_Write(dev_handle, read_ext, 8, 1);
					}
					else
					{
						// Normal Wiimote, exit while and be happy.
						rc = -1;
					}
					break;
				}
				case WM_ACK_DATA:
				{
					real_wiimote = true;
					//wm_acknowledge * wm = (wm_acknowledge*)&buf[2];
					//DEBUG_LOG(WIIMOTE, "CheckDeviceType: Got Ack Error: %X ReportID: %X", wm->errorID, wm->reportID);
					break;
				}
				case WM_READ_DATA_REPLY:
				{
					// DEBUG_LOG(WIIMOTE, "CheckDeviceType: Got Data Reply");
					wm_read_data_reply * wrdr
						= (wm_read_data_reply*)&buf[2];
					// Check if it has returned what we asked.
					if (Common::swap16(wrdr->address) == 0x00fa)
					{
						real_wiimote = true;
						// 0x020420A40000ULL means balance board.
						u64 ext_type = (*(u64*)&wrdr->data[0]);
						// DEBUG_LOG(WIIMOTE,
						//           "CheckDeviceType: GOT EXT TYPE %llX",
						//           ext_type);
						is_bb = (ext_type == 0x020420A40000ULL);
					}
					else
					{
						ERROR_LOG(WIIMOTE,
						          "CheckDeviceType: GOT UNREQUESTED ADDRESS %X",
						          Common::swap16(wrdr->address));
					}
					// force end
					rc = -1;

					break;
				}
				default:
				{
					// We let read try again incase there is another packet waiting.
					// DEBUG_LOG(WIIMOTE, "CheckDeviceType: GOT UNKNOWN REPLY: %X", buf[1]);
					break;
				}
			}
		}
	}
	CloseHandle(dev_handle);
}

bool WiimoteScanner::IsReady() const
{
	// TODO: don't search for a radio each time

	BLUETOOTH_FIND_RADIO_PARAMS radioParam;
	radioParam.dwSize = sizeof(radioParam);

	HANDLE hRadio;
	HBLUETOOTH_RADIO_FIND hFindRadio = Bth_BluetoothFindFirstRadio(&radioParam, &hRadio);

	if (nullptr != hFindRadio)
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
bool Wiimote::ConnectInternal()
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
		nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

	if (dev_handle == INVALID_HANDLE_VALUE)
	{
		dev_handle = 0;
		return false;
	}

#if 0
	TCHAR name[128] = {};
	HidD_GetProductString(dev_handle, name, 128);

	//ERROR_LOG(WIIMOTE, "Product string: %s", TStrToUTF8(name).c_str());

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

	// TODO: thread isn't started here now, do this elsewhere
	// This isn't as drastic as it sounds, since the process in which the threads
	// reside is normal priority. Needed for keeping audio reports at a decent rate
/*
	if (!SetThreadPriority(m_wiimote_thread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL))
	{
		ERROR_LOG(WIIMOTE, "Failed to set Wiimote thread priority");
	}
*/
#ifdef SHARE_WRITE_WIIMOTES
	g_connected_wiimotes.insert(devicepath);
#endif

	return true;
}

void Wiimote::DisconnectInternal()
{
	if (!IsConnected())
		return;

	CloseHandle(dev_handle);
	dev_handle = 0;

#ifdef SHARE_WRITE_WIIMOTES
	std::lock_guard<std::mutex> lk(g_connected_wiimotes_lock);
	g_connected_wiimotes.erase(devicepath);
#endif
}

void Wiimote::InitInternal()
{
	dev_handle = 0;
	stack = MSBT_STACK_UNKNOWN;

	hid_overlap_read = OVERLAPPED();
	hid_overlap_read.hEvent = CreateEvent(nullptr, true, false, nullptr);

	hid_overlap_write = OVERLAPPED();
	hid_overlap_write.hEvent = CreateEvent(nullptr, true, false, nullptr);
}

void Wiimote::TeardownInternal()
{
	CloseHandle(hid_overlap_read.hEvent);
	CloseHandle(hid_overlap_write.hEvent);
}

bool Wiimote::IsConnected() const
{
	return dev_handle != 0;
}

void _IOWakeup(HANDLE &dev_handle, OVERLAPPED &hid_overlap_read)
{
	SetEvent(hid_overlap_read.hEvent);
}

// positive = read packet
// negative = didn't read packet
// zero = error
int _IORead(HANDLE &dev_handle, OVERLAPPED &hid_overlap_read, u8* buf, int index)
{
	// Add data report indicator byte (here, 0xa1)
	buf[0] = 0xa1;
	// Used below for a warning
	buf[1] = 0;

	DWORD bytes = 0;
	ResetEvent(hid_overlap_read.hEvent);
	if (!ReadFile(dev_handle, buf + 1, MAX_PAYLOAD - 1, &bytes, &hid_overlap_read))
	{
		auto const read_err = GetLastError();

		if (ERROR_IO_PENDING == read_err)
		{
			auto const wait_result = WaitForSingleObject(hid_overlap_read.hEvent, INFINITE);

			// In case the event was signalled by _IOWakeup before the read completed, cancel it.
			CancelIo(dev_handle);

			if (WAIT_FAILED == wait_result)
			{
				WARN_LOG(WIIMOTE, "A wait error occurred on reading from Wiimote %i.", index + 1);
			}

			if (!GetOverlappedResult(dev_handle, &hid_overlap_read, &bytes, FALSE))
			{
				auto const overlapped_err = GetLastError();

				if (ERROR_OPERATION_ABORTED == overlapped_err)
				{
					// It was.
					return -1;
				}

				WARN_LOG(WIIMOTE, "GetOverlappedResult error %d on Wiimote %i.", overlapped_err, index + 1);
				return 0;
			}
		}
		else
		{
			WARN_LOG(WIIMOTE, "ReadFile error %d on Wiimote %i.", read_err, index + 1);
			return 0;
		}
	}

	WiimoteEmu::Spy(nullptr, buf, bytes + 1);

	return bytes + 1;
}

void Wiimote::IOWakeup()
{
	_IOWakeup(dev_handle, hid_overlap_read);
}


// positive = read packet
// negative = didn't read packet
// zero = error
int Wiimote::IORead(u8* buf)
{
	return _IORead(dev_handle, hid_overlap_read, buf, index);
}


int _IOWrite(HANDLE &dev_handle, OVERLAPPED &hid_overlap_write, enum win_bt_stack_t &stack, const u8* buf, size_t len)
{
	WiimoteEmu::Spy(nullptr, buf, len);

	switch (stack)
	{
		case MSBT_STACK_UNKNOWN:
		{
			// Try to auto-detect the stack type
			stack = MSBT_STACK_BLUESOLEIL;
			if (_IOWrite(dev_handle, hid_overlap_write, stack, buf, len))
				return 1;

			stack = MSBT_STACK_MS;
			if (_IOWrite(dev_handle, hid_overlap_write, stack, buf, len))
				return 1;

			stack = MSBT_STACK_UNKNOWN;
			break;
		}
		case MSBT_STACK_MS:
		{
			auto result = HidD_SetOutputReport(dev_handle, const_cast<u8*>(buf) + 1, (ULONG)(len - 1));
			//FlushFileBuffers(dev_handle);

			if (!result)
			{
				auto err = GetLastError();
				if (err == 121)
				{
					// Semaphore timeout
					NOTICE_LOG(WIIMOTE, "WiimoteIOWrite[MSBT_STACK_MS]:  Unable to send data to the Wiimote");
				}
				else
				{
					WARN_LOG(WIIMOTE, "IOWrite[MSBT_STACK_MS]: ERROR: %08x", err);
				}
			}

			return result;
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

int Wiimote::IOWrite(const u8* buf, size_t len)
{
	return _IOWrite(dev_handle, hid_overlap_write, stack, buf, len);
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
				// btdi.szName is sometimes missing it's content - it's a bt feature..
				DEBUG_LOG(WIIMOTE, "Authenticated %i connected %i remembered %i ",
				          btdi.fAuthenticated, btdi.fConnected, btdi.fRemembered);

				if (IsValidBluetoothName(UTF16ToUTF8(btdi.szName)))
				{
					callback(hRadio, radioInfo, btdi);
				}

				if (false == Bth_BluetoothFindNextDevice(hFindDevice, &btdi))
				{
					Bth_BluetoothFindDeviceClose(hFindDevice);
					hFindDevice = nullptr;
				}
			}
		}

		if (false == Bth_BluetoothFindNextRadio(hFindRadio, &hRadio))
		{
			Bth_BluetoothFindRadioClose(hFindRadio);
			hFindRadio = nullptr;
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

		NOTICE_LOG(WIIMOTE, "Found Wiimote (%02x:%02x:%02x:%02x:%02x:%02x). Enabling HID service.",
		           wm_addr[0], wm_addr[1], wm_addr[2], wm_addr[3], wm_addr[4], wm_addr[5]);

#if defined(AUTHENTICATE_WIIMOTES)
		// Authenticate
		auto const& radio_addr = radio_info.address.rgBytes;
		const DWORD auth_result = Bth_BluetoothAuthenticateDevice(nullptr, hRadio, &btdi,
		                                                          std::vector<WCHAR>(radio_addr, radio_addr + 6).data(), 6);

		if (ERROR_SUCCESS != auth_result)
		{
			ERROR_LOG(WIIMOTE, "AttachWiimote: BluetoothAuthenticateDevice returned %08x", auth_result);
		}

		DWORD pcServices = 16;
		GUID guids[16];
		// If this is not done, the Wii device will not remember the pairing
		const DWORD srv_result = Bth_BluetoothEnumerateInstalledServices(hRadio, &btdi, &pcServices, guids);

		if (ERROR_SUCCESS != srv_result)
		{
			ERROR_LOG(WIIMOTE, "AttachWiimote: BluetoothEnumerateInstalledServices returned %08x", srv_result);
		}
#endif
		// Activate service
		const DWORD hr = Bth_BluetoothSetServiceState(hRadio, &btdi,
		                                              &HumanInterfaceDeviceServiceClass_UUID, BLUETOOTH_SERVICE_ENABLE);

		g_connect_times[btdi.Address.ullLong] = std::time(nullptr);

		if (FAILED(hr))
		{
			ERROR_LOG(WIIMOTE, "AttachWiimote: BluetoothSetServiceState returned %08x", hr);
		}
		else
		{
			return true;
		}
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
		if (pair_time == g_connect_times.end() ||
		    std::difftime(time(nullptr), pair_time->second) >= avoid_forget_seconds)
		{
			// Make Windows forget about device so it will re-find it if visible.
			// This is also required to detect a disconnect for some reason..
			NOTICE_LOG(WIIMOTE, "Removing remembered Wiimote.");
			Bth_BluetoothRemoveDevice(&btdi.Address);
			return true;
		}
	}

	return false;
}

};
