// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <hidapi.h>

#include "Common/Common.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{

class WiimoteHidapi final : public Wiimote
{
public:
	WiimoteHidapi(char* device_path);
	~WiimoteHidapi() override;

protected:
	bool ConnectInternal() override;
	void DisconnectInternal() override;
	bool IsConnected() const override;
	void IOWakeup() override;
	int IORead(u8* buf) override;
	int IOWrite(u8 const* buf, size_t len) override;

private:
	std::string m_device_path;
	hid_device* m_handle;
};

WiimoteScanner::WiimoteScanner()
	: m_want_wiimotes()
{
	if (hid_init() == -1)
	{
		ERROR_LOG(WIIMOTE, "Failed to initialize hidapi.");
	}
}

bool WiimoteScanner::IsReady() const
{
	return true;
}

WiimoteScanner::~WiimoteScanner()
{
	if (hid_exit() == -1)
	{
		ERROR_LOG(WIIMOTE, "Failed to clean up hidapi.");
	}
}

void WiimoteScanner::Update()
{}

void WiimoteScanner::FindWiimotes(std::vector<Wiimote*>& found_wiimotes, Wiimote*& found_board)
{
	// Search for both old and new Wiimotes.
	for (uint16_t product_id : {0x0306, 0x0330})
	{
		hid_device_info* list = hid_enumerate(0x057e, product_id);
		hid_device_info* item = list;
		while (item)
		{
			NOTICE_LOG(WIIMOTE, "Found Wiimote at %s: %ls %ls", item->path, item->manufacturer_string, item->product_string);
			Wiimote* wiimote = new WiimoteHidapi(item->path);
			found_wiimotes.push_back(wiimote);
			item = item->next;
		}
		hid_free_enumeration(list);
	}
}

WiimoteHidapi::WiimoteHidapi(char* device_path) : m_device_path(device_path), m_handle(nullptr)
{}

WiimoteHidapi::~WiimoteHidapi()
{
	Shutdown();
}

// Connect to a wiimote with a known address.
bool WiimoteHidapi::ConnectInternal()
{
	m_handle = hid_open_path(m_device_path.c_str());
	if (!m_handle)
	{
		ERROR_LOG(WIIMOTE, "Could not connect to Wiimote at \"%s\". "
		                   "Do you have permission to access the device?", m_device_path.c_str());
	}
	return !!m_handle;
}

void WiimoteHidapi::DisconnectInternal()
{
	hid_close(m_handle);
	m_handle = nullptr;
}

bool WiimoteHidapi::IsConnected() const
{
	return !!m_handle;
}

void WiimoteHidapi::IOWakeup()
{}

// positive = read packet
// negative = didn't read packet
// zero = error
int WiimoteHidapi::IORead(u8* buf)
{
	int timeout = 200; // ms
	int result = hid_read_timeout(m_handle, buf + 1, MAX_PAYLOAD - 1, timeout);
	// TODO: If and once we use hidapi across plaforms, change our internal API to clean up this mess.
	if (result == -1)
	{
		ERROR_LOG(WIIMOTE, "Failed to read from %s.", m_device_path.c_str());
		result = 0;
	}
	else if (result == 0)
	{
		result = -1;
	}
	else
	{
		buf[0] = WM_SET_REPORT | WM_BT_INPUT;
		result += 1;
	}
	return result;
}

int WiimoteHidapi::IOWrite(u8 const* buf, size_t len)
{
	_dbg_assert_(WIIMOTE, buf[0] == (WM_SET_REPORT | WM_BT_OUTPUT));
	int result = hid_write(m_handle, buf + 1, len - 1);
	if (result == -1)
	{
		ERROR_LOG(WIIMOTE, "Failed to write to %s.", m_device_path.c_str());
		result = 0;
	}
	else
	{
		result += 1;
	}
	return result;
}

}; // WiimoteReal
