// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <hidsdi.h>
#include <unordered_map>
#include <unordered_set>
#include <windows.h>
// The following Windows headers must be included AFTER windows.h.
#include <BluetoothAPIs.h> //NOLINT
#include <Cfgmgr32.h>      //NOLINT
// initguid.h must be included before Devpkey.h
#include <initguid.h>      //NOLINT
#include <Devpkey.h>       //NOLINT
#include <dbt.h>           //NOLINT
#include <setupapi.h>      //NOLINT

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"
#include "Common/Logging/Log.h"
#include "Core/HW/WiimoteEmu/WiimoteHid.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

//#define AUTHENTICATE_WIIMOTES
#define SHARE_WRITE_WIIMOTES

// Create func_t function pointer type and declare a nullptr-initialized static variable of that
// type named "pfunc".
#define DYN_FUNC_DECLARE(func) \
	typedef decltype(&func) func ## _t; \
	static func ## _t p ## func = nullptr;

DYN_FUNC_DECLARE(HidD_GetHidGuid);
DYN_FUNC_DECLARE(HidD_GetAttributes);
DYN_FUNC_DECLARE(HidD_SetOutputReport);
DYN_FUNC_DECLARE(HidD_GetProductString);

DYN_FUNC_DECLARE(BluetoothFindDeviceClose);
DYN_FUNC_DECLARE(BluetoothFindFirstDevice);
DYN_FUNC_DECLARE(BluetoothFindFirstRadio);
DYN_FUNC_DECLARE(BluetoothFindNextDevice);
DYN_FUNC_DECLARE(BluetoothFindNextRadio);
DYN_FUNC_DECLARE(BluetoothFindRadioClose);
DYN_FUNC_DECLARE(BluetoothGetRadioInfo);
DYN_FUNC_DECLARE(BluetoothRemoveDevice);
DYN_FUNC_DECLARE(BluetoothSetServiceState);
DYN_FUNC_DECLARE(BluetoothAuthenticateDeviceEx);
DYN_FUNC_DECLARE(BluetoothEnumerateInstalledServices);

#undef DYN_FUNC_DECLARE

static HINSTANCE s_hid_lib = nullptr;
static HINSTANCE s_bthprops_lib = nullptr;

static bool s_loaded_ok = false;

std::unordered_map<BTH_ADDR, std::time_t> g_connect_times;

#ifdef SHARE_WRITE_WIIMOTES
std::unordered_set<std::basic_string<TCHAR>> g_connected_wiimotes;
std::mutex g_connected_wiimotes_lock;
#endif

#define DYN_FUNC_UNLOAD(func) \
	p ## func = nullptr;

// Attempt to load the function from the given module handle.
#define DYN_FUNC_LOAD(module, func) \
	p ## func = ( func ## _t)::GetProcAddress(module, # func ); \
	if (! p ## func ) \
	{ \
		return false; \
	}

bool load_hid()
{
	auto loader = [&]()
	{
		s_hid_lib = ::LoadLibrary(_T("hid.dll"));
		if (!s_hid_lib)
		{
			return false;
		}

		DYN_FUNC_LOAD(s_hid_lib, HidD_GetHidGuid);
		DYN_FUNC_LOAD(s_hid_lib, HidD_GetAttributes);
		DYN_FUNC_LOAD(s_hid_lib, HidD_SetOutputReport);
		DYN_FUNC_LOAD(s_hid_lib, HidD_GetProductString);

		return true;
	};

	bool loaded_ok = loader();

	if (!loaded_ok)
	{
		DYN_FUNC_UNLOAD(HidD_GetHidGuid);
		DYN_FUNC_UNLOAD(HidD_GetAttributes);
		DYN_FUNC_UNLOAD(HidD_SetOutputReport);
		DYN_FUNC_UNLOAD(HidD_GetProductString);

		if (s_hid_lib)
		{
			::FreeLibrary(s_hid_lib);
			s_hid_lib = nullptr;
		}
	}

	return loaded_ok;
}

bool load_bthprops()
{
	auto loader = [&]()
	{
		s_bthprops_lib = ::LoadLibrary(_T("bthprops.cpl"));
		if (!s_bthprops_lib)
		{
			return false;
		}

		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothFindDeviceClose);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothFindFirstDevice);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothFindFirstRadio);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothFindNextDevice);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothFindNextRadio);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothFindRadioClose);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothGetRadioInfo);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothRemoveDevice);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothSetServiceState);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothAuthenticateDeviceEx);
		DYN_FUNC_LOAD(s_bthprops_lib, BluetoothEnumerateInstalledServices);

		return true;
	};

	bool loaded_ok = loader();

	if (!loaded_ok)
	{
		DYN_FUNC_UNLOAD(BluetoothFindDeviceClose);
		DYN_FUNC_UNLOAD(BluetoothFindFirstDevice);
		DYN_FUNC_UNLOAD(BluetoothFindFirstRadio);
		DYN_FUNC_UNLOAD(BluetoothFindNextDevice);
		DYN_FUNC_UNLOAD(BluetoothFindNextRadio);
		DYN_FUNC_UNLOAD(BluetoothFindRadioClose);
		DYN_FUNC_UNLOAD(BluetoothGetRadioInfo);
		DYN_FUNC_UNLOAD(BluetoothRemoveDevice);
		DYN_FUNC_UNLOAD(BluetoothSetServiceState);
		DYN_FUNC_UNLOAD(BluetoothAuthenticateDeviceEx);
		DYN_FUNC_UNLOAD(BluetoothEnumerateInstalledServices);

		if (s_bthprops_lib)
		{
			::FreeLibrary(s_bthprops_lib);
			s_bthprops_lib = nullptr;
		}
	}

	return loaded_ok;
}

#undef DYN_FUNC_LOAD
#undef DYN_FUNC_UNLOAD

inline void init_lib()
{
	static bool initialized = false;

	if (!initialized)
	{
		// Only try once
		initialized = true;

		// After these calls, we know all dynamically loaded APIs will either all be valid or
		// all nullptr.
		if (!load_hid() || !load_bthprops())
		{
			NOTICE_LOG(WIIMOTE,
				"Failed to load Bluetooth support libraries, Wiimotes will not function");
			return;
		}

		s_loaded_ok = true;
	}
}

namespace WiimoteReal
{

class WiimoteWindows final : public Wiimote
{
public:
	WiimoteWindows(const std::basic_string<TCHAR>& path, WinWriteMethod initial_write_method);
	~WiimoteWindows() override;

protected:
	bool ConnectInternal() override;
	void DisconnectInternal() override;
	bool IsConnected() const override;
	void IOWakeup() override;
	int IORead(u8* buf) override;
	int IOWrite(u8 const* buf, size_t len) override;

private:
	std::basic_string<TCHAR> m_devicepath; // Unique Wiimote reference
	HANDLE m_dev_handle;                   // HID handle
	OVERLAPPED m_hid_overlap_read;         // Overlap handles
	OVERLAPPED m_hid_overlap_write;
	WinWriteMethod m_write_method;         // Type of Write Method to use
};

int IOWrite(HANDLE &dev_handle, OVERLAPPED &hid_overlap_write, enum WinWriteMethod &stack, const u8* buf, size_t len, DWORD* written);
int IORead(HANDLE &dev_handle, OVERLAPPED &hid_overlap_read, u8* buf, int index);

template <typename T>
void ProcessWiimotes(bool new_scan, T& callback);

bool AttachWiimote(HANDLE hRadio, const BLUETOOTH_RADIO_INFO&, BLUETOOTH_DEVICE_INFO_STRUCT&);
void RemoveWiimote(BLUETOOTH_DEVICE_INFO_STRUCT&);
bool ForgetWiimote(BLUETOOTH_DEVICE_INFO_STRUCT&);

WiimoteScanner::WiimoteScanner()
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
	if (!s_loaded_ok)
		return;

	bool forgot_some = false;

	ProcessWiimotes(false, [&](HANDLE, BLUETOOTH_RADIO_INFO&, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
	{
		forgot_some |= ForgetWiimote(btdi);
	});

	// Some hacks that allows disconnects to be detected before connections are handled
	// workaround for Wiimote 1 moving to slot 2 on temporary disconnect
	if (forgot_some)
		Common::SleepCurrentThread(100);
}

// Moves up one node in the device tree and returns its device info data along with an info set only including that device for further processing
// See https://msdn.microsoft.com/en-us/library/windows/hardware/ff549417(v=vs.85).aspx
static bool GetParentDevice(const DEVINST &child_device_instance, HDEVINFO *parent_device_info, PSP_DEVINFO_DATA parent_device_data)
{
	ULONG status;
	ULONG problem_number;
	CONFIGRET result;

	// Check if that device instance has device node present
	result = CM_Get_DevNode_Status(&status, &problem_number, child_device_instance, 0);
	if (result != CR_SUCCESS)
	{
		return false;
	}

	DEVINST parent_device;

	// Get the device instance of the parent
	result = CM_Get_Parent(&parent_device, child_device_instance, 0);
	if (result != CR_SUCCESS)
	{
		return false;
	}

	std::vector<WCHAR> parent_device_id(MAX_DEVICE_ID_LEN);;

	// Get the device id of the parent, required to open the device info
	result = CM_Get_Device_ID(parent_device, parent_device_id.data(), (ULONG)parent_device_id.size(), 0);
	if (result != CR_SUCCESS)
	{
		return false;
	}

	// Create a new empty device info set for the device info data
	(*parent_device_info) = SetupDiCreateDeviceInfoList(nullptr, nullptr);

	// Open the device info data of the parent and put it in the emtpy info set
	if (!SetupDiOpenDeviceInfo((*parent_device_info), parent_device_id.data(), nullptr, 0, parent_device_data))
	{
		SetupDiDestroyDeviceInfoList(parent_device_info);
		return false;
	}

	return true;
}

std::wstring GetDeviceProperty(const HDEVINFO &device_info, const PSP_DEVINFO_DATA device_data, const DEVPROPKEY *requested_property)
{
	DWORD required_size = 0;
	DEVPROPTYPE device_property_type;

	SetupDiGetDeviceProperty(device_info, device_data, requested_property, &device_property_type, nullptr, 0, &required_size, 0);

	std::vector<BYTE> unicode_buffer(required_size, 0);

	BOOL result = SetupDiGetDeviceProperty(device_info, device_data, requested_property, &device_property_type, unicode_buffer.data(), required_size, nullptr, 0);
	if (!result)
	{
		return std::wstring();
	}

	return std::wstring((PWCHAR)unicode_buffer.data());
}

// The enumerated device nodes/instances are "empty" PDO's that act as interfaces for the HID Class Driver.
// Since those PDO's normaly don't have a FDO and therefore no driver loaded, we need to move one device node up in the device tree.
// Then check the provider of the device driver, which will be "Microsoft" in case of the default HID Class Driver
// or "TOSHIBA" in case of the Toshiba Bluetooth Stack, because it provides its own Class Driver.
static bool CheckForToshibaStack(const DEVINST &hid_interface_device_instance)
{
	HDEVINFO parent_device_info = nullptr;
	SP_DEVINFO_DATA parent_device_data = {};
	parent_device_data.cbSize = sizeof(SP_DEVINFO_DATA);

	if (GetParentDevice(hid_interface_device_instance, &parent_device_info, &parent_device_data))
	{
		std::wstring class_driver_provider = GetDeviceProperty(parent_device_info, &parent_device_data, &DEVPKEY_Device_DriverProvider);

		SetupDiDestroyDeviceInfoList(parent_device_info);

		return (class_driver_provider == L"TOSHIBA");
	}

	DEBUG_LOG(WIIMOTE, "Unable to detect class driver provider!");

	return false;
}

static WinWriteMethod GetInitialWriteMethod(bool IsUsingToshibaStack)
{
	// Currently Toshiba Bluetooth Stack needs the Output buffer to be the size of the largest output report
	return (IsUsingToshibaStack ? WWM_WRITE_FILE_LARGEST_REPORT_SIZE : WWM_WRITE_FILE_ACTUAL_REPORT_SIZE);
}

// Find and connect Wiimotes.
// Does not replace already found Wiimotes even if they are disconnected.
// wm is an array of max_wiimotes Wiimotes
// Returns the total number of found and connected Wiimotes.
void WiimoteScanner::FindWiimotes(std::vector<Wiimote*> & found_wiimotes, Wiimote* & found_board)
{
	if (!s_loaded_ok)
		return;

	ProcessWiimotes(true, [](HANDLE hRadio, const BLUETOOTH_RADIO_INFO& rinfo, BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
	{
		ForgetWiimote(btdi);
		AttachWiimote(hRadio, rinfo, btdi);
	});

	// Get the device id
	GUID device_id;
	pHidD_GetHidGuid(&device_id);

	// Get all hid devices connected
	HDEVINFO const device_info = SetupDiGetClassDevs(&device_id, nullptr, nullptr, (DIGCF_DEVICEINTERFACE | DIGCF_PRESENT));

	SP_DEVICE_INTERFACE_DATA device_data = {};
	device_data.cbSize = sizeof(device_data);
	PSP_DEVICE_INTERFACE_DETAIL_DATA detail_data = nullptr;

	for (int index = 0; SetupDiEnumDeviceInterfaces(device_info, nullptr, &device_id, index, &device_data); ++index)
	{
		// Get the size of the data block required
		DWORD len;
		SetupDiGetDeviceInterfaceDetail(device_info, &device_data, nullptr, 0, &len, nullptr);
		detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(len);
		detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		SP_DEVINFO_DATA device_info_data = {};
		device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);

		// Query the data for this device
		if (SetupDiGetDeviceInterfaceDetail(device_info, &device_data, detail_data, len, nullptr, &device_info_data))
		{
			std::basic_string<TCHAR> device_path(detail_data->DevicePath);
			bool real_wiimote = false;
			bool is_bb = false;
			bool IsUsingToshibaStack = CheckForToshibaStack(device_info_data.DevInst);

			WinWriteMethod write_method = GetInitialWriteMethod(IsUsingToshibaStack);

			CheckDeviceType(device_path, write_method, real_wiimote, is_bb);

			if (real_wiimote)
			{
				Wiimote* wm = new WiimoteWindows(device_path, write_method);

				if (is_bb)
				{
					found_board = wm;
				}
				else
				{
					found_wiimotes.push_back(wm);
				}
			}
		}

		free(detail_data);
	}

	SetupDiDestroyDeviceInfoList(device_info);
}

int CheckDeviceType_Write(HANDLE &dev_handle, WinWriteMethod &write_method, const u8* buf, size_t size, int attempts)
{
	OVERLAPPED hid_overlap_write = OVERLAPPED();
	hid_overlap_write.hEvent = CreateEvent(nullptr, true, false, nullptr);

	DWORD written = 0;

	for (; attempts>0; --attempts)
	{
		if (IOWrite(dev_handle, hid_overlap_write, write_method, buf, size, &written))
			break;
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
		read = IORead(dev_handle, hid_overlap_read, buf, 1);
		if (read > 0)
			break;
	}

	CloseHandle(hid_overlap_read.hEvent);

	return read;
}

// A convoluted way of checking if a device is a Wii Balance Board and if it is a connectible Wiimote.
// Because nothing on Windows should be easy.
// (We can't seem to easily identify the Bluetooth device an HID device belongs to...)
void WiimoteScanner::CheckDeviceType(std::basic_string<TCHAR> &devicepath, WinWriteMethod &write_method, bool &real_wiimote, bool &is_bb)
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
		(pHidD_GetAttributes(dev_handle, &attrib) &&
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
		                      write_method,
		                      disable_enc_pt1_report,
		                      sizeof(disable_enc_pt1_report),
		                      1);
		CheckDeviceType_Write(dev_handle,
		                      write_method,
		                      disable_enc_pt2_report,
		                      sizeof(disable_enc_pt2_report),
		                      1);

		int rc = CheckDeviceType_Write(dev_handle,
		                               write_method,
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
						rc = CheckDeviceType_Write(dev_handle, write_method, read_ext, 8, 1);
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
	if (!s_loaded_ok)
	{
		return false;
	}

	// TODO: don't search for a radio each time

	BLUETOOTH_FIND_RADIO_PARAMS radioParam;
	radioParam.dwSize = sizeof(radioParam);

	HANDLE hRadio;
	HBLUETOOTH_RADIO_FIND hFindRadio = pBluetoothFindFirstRadio(&radioParam, &hRadio);

	if (nullptr != hFindRadio)
	{
		pBluetoothFindRadioClose(hFindRadio);
		return true;
	}
	else
	{
		return false;
	}
}

// Connect to a Wiimote with a known device path.
bool WiimoteWindows::ConnectInternal()
{
	if (IsConnected())
		return false;

#ifdef SHARE_WRITE_WIIMOTES
	std::lock_guard<std::mutex> lk(g_connected_wiimotes_lock);
	if (g_connected_wiimotes.count(m_devicepath) != 0)
		return false;

	auto const open_flags = FILE_SHARE_READ | FILE_SHARE_WRITE;
#else
	// Having no FILE_SHARE_WRITE disallows us from connecting to the same Wiimote twice.
	// (And disallows using Wiimotes in use by other programs)
	// This is what "WiiYourself" does.
	// Apparently this doesn't work for everyone. It might be their fault.
	auto const open_flags = FILE_SHARE_READ;
#endif

	m_dev_handle = CreateFile(m_devicepath.c_str(),
		GENERIC_READ | GENERIC_WRITE, open_flags,
		nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

	if (m_dev_handle == INVALID_HANDLE_VALUE)
	{
		m_dev_handle = nullptr;
		return false;
	}

#if 0
	TCHAR name[128] = {};
	pHidD_GetProductString(dev_handle, name, 128);

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
	if (!pHidD_GetAttributes(dev_handle, &attr))
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
	g_connected_wiimotes.insert(m_devicepath);
#endif

	return true;
}

void WiimoteWindows::DisconnectInternal()
{
	if (!IsConnected())
		return;

	CloseHandle(m_dev_handle);
	m_dev_handle = nullptr;

#ifdef SHARE_WRITE_WIIMOTES
	std::lock_guard<std::mutex> lk(g_connected_wiimotes_lock);
	g_connected_wiimotes.erase(m_devicepath);
#endif
}

WiimoteWindows::WiimoteWindows(const std::basic_string<TCHAR>& path, WinWriteMethod initial_write_method)
: m_devicepath(path), m_write_method(initial_write_method)
{
	m_dev_handle = nullptr;

	m_hid_overlap_read = OVERLAPPED();
	m_hid_overlap_read.hEvent = CreateEvent(nullptr, true, false, nullptr);

	m_hid_overlap_write = OVERLAPPED();
	m_hid_overlap_write.hEvent = CreateEvent(nullptr, true, false, nullptr);
}

WiimoteWindows::~WiimoteWindows()
{
	Shutdown();
	CloseHandle(m_hid_overlap_read.hEvent);
	CloseHandle(m_hid_overlap_write.hEvent);
}

bool WiimoteWindows::IsConnected() const
{
	return m_dev_handle != nullptr;
}

// positive = read packet
// negative = didn't read packet
// zero = error
int IORead(HANDLE &dev_handle, OVERLAPPED &hid_overlap_read, u8* buf, int index)
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
			if (!GetOverlappedResult(dev_handle, &hid_overlap_read, &bytes, TRUE))
			{
				auto const overlapped_err = GetLastError();

				// In case it was aborted by someone else
				if (ERROR_OPERATION_ABORTED == overlapped_err)
				{
					return -1;
				}

				WARN_LOG(WIIMOTE, "GetOverlappedResult error %d on Wiimote %i.", overlapped_err, index + 1);
				return 0;
			}
			// If IOWakeup sets the event so GetOverlappedResult returns prematurely, but the request is still pending
			else if (hid_overlap_read.Internal == STATUS_PENDING)
			{
				// Don't forget to cancel it.
				CancelIo(dev_handle);
				return -1;
			}
		}
		else
		{
			WARN_LOG(WIIMOTE, "ReadFile error %d on Wiimote %i.", read_err, index + 1);
			return 0;
		}
	}

	return bytes + 1;
}

void WiimoteWindows::IOWakeup()
{
	SetEvent(m_hid_overlap_read.hEvent);
}


// positive = read packet
// negative = didn't read packet
// zero = error
int WiimoteWindows::IORead(u8* buf)
{
	return WiimoteReal::IORead(m_dev_handle, m_hid_overlap_read, buf, m_index);
}

static int IOWritePerSetOutputReport(HANDLE &dev_handle, const u8* buf, size_t len, DWORD* written)
{
	BOOLEAN result = pHidD_SetOutputReport(dev_handle, const_cast<u8*>(buf) + 1, (ULONG)(len - 1));
	if (!result)
	{
		DWORD err = GetLastError();
		if (err == 121)
		{
			// Semaphore timeout
			NOTICE_LOG(WIIMOTE, "IOWrite[WWM_SET_OUTPUT_REPORT]: Unable to send data to the Wiimote");
		}
		else if (err != 0x1F)  // Some third-party adapters (DolphinBar) use this
							   // error code to signal the absence of a Wiimote
							   // linked to the HID device.
		{
			WARN_LOG(WIIMOTE, "IOWrite[WWM_SET_OUTPUT_REPORT]: Error: %08x", err);
		}
	}

	if (written)
	{
		*written = (result ? (DWORD)len : 0);
	}

	return result;
}

static int IOWritePerWriteFile(HANDLE &dev_handle, OVERLAPPED &hid_overlap_write, WinWriteMethod &write_method, const u8* buf, size_t len, DWORD* written)
{
	DWORD bytes_written;
	LPCVOID write_buffer = buf + 1;
	DWORD bytes_to_write = (DWORD)(len - 1);

	u8 resized_buffer[MAX_PAYLOAD];

	// Resize the buffer, if the underlying HID Class driver needs the buffer to be the size of HidCaps.OuputReportSize
	// In case of Wiimote HidCaps.OuputReportSize is 22 Byte.
	// This is currently needed by the Toshiba Bluetooth Stack.
	if ((write_method == WWM_WRITE_FILE_LARGEST_REPORT_SIZE) && (MAX_PAYLOAD > len))
	{
		std::copy(buf, buf + len, resized_buffer);
		std::fill(resized_buffer + len, resized_buffer + MAX_PAYLOAD, 0);
		write_buffer = resized_buffer + 1;
		bytes_to_write = MAX_PAYLOAD - 1;
	}

	ResetEvent(hid_overlap_write.hEvent);
	BOOLEAN result = WriteFile(dev_handle, write_buffer, bytes_to_write, &bytes_written, &hid_overlap_write);
	if (!result)
	{
		const DWORD error = GetLastError();

		switch (error)
		{
		case ERROR_INVALID_USER_BUFFER:
			INFO_LOG(WIIMOTE, "IOWrite[WWM_WRITE_FILE]: Falling back to SetOutputReport");
			write_method = WWM_SET_OUTPUT_REPORT;
			return IOWritePerSetOutputReport(dev_handle, buf, len, written);
		case ERROR_IO_PENDING:
			// Pending is no error!
			break;
		default:
			WARN_LOG(WIIMOTE, "IOWrite[WWM_WRITE_FILE]: Error on WriteFile: %08x", error);
			CancelIo(dev_handle);
			return 0;
		}
	}

	if (written)
	{
		*written = 0;
	}

	// Wait for completion
	DWORD wait_result = WaitForSingleObject(hid_overlap_write.hEvent, WIIMOTE_DEFAULT_TIMEOUT);

	if (WAIT_TIMEOUT == wait_result)
	{
		WARN_LOG(WIIMOTE, "IOWrite[WWM_WRITE_FILE]: A timeout occurred on writing to Wiimote.");
		CancelIo(dev_handle);
		return 1;
	}
	else if (WAIT_FAILED == wait_result)
	{
		WARN_LOG(WIIMOTE, "IOWrite[WWM_WRITE_FILE]: A wait error occurred on writing to Wiimote.");
		CancelIo(dev_handle);
		return 1;
	}

	if (written)
	{
		if (!GetOverlappedResult(dev_handle, &hid_overlap_write, written, TRUE))
		{
			*written = 0;
		}
	}

	return 1;
}

// As of https://msdn.microsoft.com/en-us/library/windows/hardware/ff543402(v=vs.85).aspx, WriteFile is the preferred method
// to send output reports to the HID. WriteFile sends an IRP_MJ_WRITE to the HID Class Driver (https://msdn.microsoft.com/en-us/library/windows/hardware/ff543402(v=vs.85).aspx).
// https://msdn.microsoft.com/en-us/library/windows/hardware/ff541027(v=vs.85).aspx & https://msdn.microsoft.com/en-us/library/windows/hardware/ff543402(v=vs.85).aspx
// state that the used buffer shall be the size of HidCaps.OutputReportSize (the largest output report).
// However as it seems only the Toshiba Bluetooth Stack, which provides its own HID Class Driver, as well as the HID Class Driver
// on Windows 7 enforce this requirement. Whereas on Windows 8/8.1/10 the buffer size can be the actual used report size.
// On Windows 7 when sending a smaller report to the device all bytes of the largest report are sent, which results in
// an error on the Wiimote. Toshiba Bluetooth Stack in contrast only sends the neccessary bytes of the report to the device.
// Therefore it is not possible to use WriteFile on Windows 7 to send data to the Wiimote and the fallback
// to HidP_SetOutputReport is implemented, which in turn does not support "-TR" Wiimotes.
// As to why on the later Windows' WriteFile or the HID Class Driver doesn't follow the documentation, it may be a bug or a feature.
// This leads to the following:
// - Toshiba Bluetooth Stack: Use WriteFile with resized output buffer
// - Windows Default HID Class: Try WriteFile with actual output buffer (will work in Win8/8.1/10)
// - When WriteFile fails, fallback to HidP_SetOutputReport (for Win7)
// Besides the documentation, WriteFile shall be the preferred method to send data, because it seems to use the Bluetooth Interrupt/Data Channel,
// whereas SetOutputReport uses the Control Channel. This leads to the advantage, that "-TR" Wiimotes work with WriteFile
// as they don't accept output reports via the Control Channel.
int IOWrite(HANDLE &dev_handle, OVERLAPPED &hid_overlap_write, WinWriteMethod &write_method, const u8* buf, size_t len, DWORD* written)
{
	switch (write_method)
	{
	case WWM_WRITE_FILE_LARGEST_REPORT_SIZE:
	case WWM_WRITE_FILE_ACTUAL_REPORT_SIZE:
		return IOWritePerWriteFile(dev_handle, hid_overlap_write, write_method, buf, len, written);
	case WWM_SET_OUTPUT_REPORT:
		return IOWritePerSetOutputReport(dev_handle, buf, len, written);
	}

	return 0;
}

int WiimoteWindows::IOWrite(const u8* buf, size_t len)
{
	return WiimoteReal::IOWrite(m_dev_handle, m_hid_overlap_write, m_write_method, buf, len, nullptr);
}

// invokes callback for each found Wiimote Bluetooth device
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
	HBLUETOOTH_RADIO_FIND hFindRadio = pBluetoothFindFirstRadio(&radioParam, &hRadio);
	while (hFindRadio)
	{
		BLUETOOTH_RADIO_INFO radioInfo;
		radioInfo.dwSize = sizeof(radioInfo);

		auto const rinfo_result = pBluetoothGetRadioInfo(hRadio, &radioInfo);
		if (ERROR_SUCCESS == rinfo_result)
		{
			srch.hRadio = hRadio;

			BLUETOOTH_DEVICE_INFO btdi;
			btdi.dwSize = sizeof(btdi);

			// Enumerate BT devices
			HBLUETOOTH_DEVICE_FIND hFindDevice = pBluetoothFindFirstDevice(&srch, &btdi);
			while (hFindDevice)
			{
				// btdi.szName is sometimes missing it's content - it's a bt feature..
				DEBUG_LOG(WIIMOTE, "Authenticated %i connected %i remembered %i ",
				          btdi.fAuthenticated, btdi.fConnected, btdi.fRemembered);

				if (IsValidBluetoothName(UTF16ToUTF8(btdi.szName)))
				{
					callback(hRadio, radioInfo, btdi);
				}

				if (false == pBluetoothFindNextDevice(hFindDevice, &btdi))
				{
					pBluetoothFindDeviceClose(hFindDevice);
					hFindDevice = nullptr;
				}
			}
		}

		if (false == pBluetoothFindNextRadio(hFindRadio, &hRadio))
		{
			pBluetoothFindRadioClose(hFindRadio);
			hFindRadio = nullptr;
		}
	}
}

void RemoveWiimote(BLUETOOTH_DEVICE_INFO_STRUCT& btdi)
{
	//if (btdi.fConnected)
	{
		if (SUCCEEDED(pBluetoothRemoveDevice(&btdi.Address)))
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
		// FIXME Not sure this usage of OOB_DATA_INFO is correct...
		BLUETOOTH_OOB_DATA_INFO oob_data_info = { 0 };
		memcpy(&oob_data_info.C[0], &radio_addr[0], sizeof(WCHAR) * 6);
		const DWORD auth_result = pBluetoothAuthenticateDeviceEx(nullptr, hRadio, &btdi,
			&oob_data_info, MITMProtectionNotDefined);

		if (ERROR_SUCCESS != auth_result)
		{
			ERROR_LOG(WIIMOTE, "AttachWiimote: BluetoothAuthenticateDeviceEx returned %08x", auth_result);
		}

		DWORD pcServices = 16;
		GUID guids[16];
		// If this is not done, the Wii device will not remember the pairing
		const DWORD srv_result = pBluetoothEnumerateInstalledServices(hRadio, &btdi, &pcServices, guids);

		if (ERROR_SUCCESS != srv_result)
		{
			ERROR_LOG(WIIMOTE, "AttachWiimote: BluetoothEnumerateInstalledServices returned %08x", srv_result);
		}
#endif
		// Activate service
		const DWORD hr = pBluetoothSetServiceState(hRadio, &btdi,
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
			pBluetoothRemoveDevice(&btdi.Address);
			return true;
		}
	}

	return false;
}

};
