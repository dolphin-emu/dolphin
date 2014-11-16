// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include "Common/Common.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"

namespace WiimoteReal
{

WiimoteScanner::WiimoteScanner()
	: m_want_wiimotes()
	, device_id(-1)
	, device_sock(-1)
{
	// Get the id of the first bluetooth device.
	device_id = hci_get_route(nullptr);
	if (device_id < 0)
	{
		NOTICE_LOG(WIIMOTE, "Bluetooth not found.");
		return;
	}

	// Create a socket to the device
	device_sock = hci_open_dev(device_id);
	if (device_sock < 0)
	{
		ERROR_LOG(WIIMOTE, "Unable to open bluetooth.");
		return;
	}
}

bool WiimoteScanner::IsReady() const
{
	return device_sock > 0;
}

WiimoteScanner::~WiimoteScanner()
{
	if (IsReady())
		close(device_sock);
}

void WiimoteScanner::Update()
{}

void WiimoteScanner::FindWiimotes(std::vector<Wiimote*> & found_wiimotes, Wiimote* & found_board)
{
	// supposedly 1.28 seconds
	int const wait_len = 1;

	int const max_infos = 255;
	inquiry_info scan_infos[max_infos] = {};
	auto* scan_infos_ptr = scan_infos;
	found_board = nullptr;

	// Scan for bluetooth devices
	int const found_devices = hci_inquiry(device_id, wait_len, max_infos, nullptr, &scan_infos_ptr, IREQ_CACHE_FLUSH);
	if (found_devices < 0)
	{
		ERROR_LOG(WIIMOTE, "Error searching for bluetooth devices.");
		return;
	}

	DEBUG_LOG(WIIMOTE, "Found %i bluetooth device(s).", found_devices);

	// Display discovered devices
	for (int i = 0; i < found_devices; ++i)
	{
		ERROR_LOG(WIIMOTE, "found a device...");

		// BT names are a maximum of 248 bytes apparently
		char name[255] = {};
		if (hci_read_remote_name(device_sock, &scan_infos[i].bdaddr, sizeof(name), name, 1000) < 0)
		{
			ERROR_LOG(WIIMOTE, "name request failed");
			continue;
		}

		ERROR_LOG(WIIMOTE, "device name %s", name);
		if (IsValidBluetoothName(name))
		{
			bool new_wiimote = true;

			// TODO: do this

			// Determine if this wiimote has already been found.
			//for (int j = 0; j < MAX_WIIMOTES && new_wiimote; ++j)
			//{
			//	if (wm[j] && bacmp(&scan_infos[i].bdaddr,&wm[j]->bdaddr) == 0)
			//		new_wiimote = false;
			//}

			if (new_wiimote)
			{
				// Found a new device
				char bdaddr_str[18] = {};
				ba2str(&scan_infos[i].bdaddr, bdaddr_str);

				auto* const wm = new Wiimote;
				wm->bdaddr = scan_infos[i].bdaddr;
				if (IsBalanceBoardName(name))
				{
					found_board = wm;
					NOTICE_LOG(WIIMOTE, "Found balance board (%s).", bdaddr_str);
				}
				else
				{
					found_wiimotes.push_back(wm);
					NOTICE_LOG(WIIMOTE, "Found wiimote (%s).", bdaddr_str);
				}
			}
		}
	}

}

void Wiimote::InitInternal()
{
	cmd_sock = -1;
	int_sock = -1;

	int fds[2];
	if (pipe(fds))
	{
		ERROR_LOG(WIIMOTE, "pipe failed");
		abort();
	}
	wakeup_pipe_w = fds[1];
	wakeup_pipe_r = fds[0];
	bdaddr = (bdaddr_t){{0, 0, 0, 0, 0, 0}};
}

void Wiimote::TeardownInternal()
{
	close(wakeup_pipe_w);
	close(wakeup_pipe_r);
}

// Connect to a wiimote with a known address.
bool Wiimote::ConnectInternal()
{
	sockaddr_l2 addr;
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_bdaddr = bdaddr;
	addr.l2_cid = 0;

	// Output channel
	addr.l2_psm = htobs(WM_OUTPUT_CHANNEL);
	if ((cmd_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1 ||
			connect(cmd_sock, (sockaddr*)&addr, sizeof(addr)) < 0)
	{
		DEBUG_LOG(WIIMOTE, "Unable to open output socket to wiimote.");
		close(cmd_sock);
		cmd_sock = -1;
		return false;
	}

	// Input channel
	addr.l2_psm = htobs(WM_INPUT_CHANNEL);
	if ((int_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1 ||
			connect(int_sock, (sockaddr*)&addr, sizeof(addr)) < 0)
	{
		DEBUG_LOG(WIIMOTE, "Unable to open input socket from wiimote.");
		close(int_sock);
		close(cmd_sock);
		int_sock = cmd_sock = -1;
		return false;
	}

	return true;
}

void Wiimote::DisconnectInternal()
{
	close(cmd_sock);
	close(int_sock);

	cmd_sock = -1;
	int_sock = -1;
}

bool Wiimote::IsConnected() const
{
	return cmd_sock != -1;// && int_sock != -1;
}

void Wiimote::IOWakeup()
{
	char c = 0;
	if (write(wakeup_pipe_w, &c, 1) != 1)
	{
		ERROR_LOG(WIIMOTE, "Unable to write to wakeup pipe.");
	}
}

// positive = read packet
// negative = didn't read packet
// zero = error
int Wiimote::IORead(u8* buf)
{
	// Block select for 1/2000th of a second

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(int_sock, &fds);
	FD_SET(wakeup_pipe_r, &fds);

	if (select(int_sock + 1, &fds, nullptr, nullptr, nullptr) == -1)
	{
		ERROR_LOG(WIIMOTE, "Unable to select wiimote %i input socket.", index + 1);
		return -1;
	}

	if (FD_ISSET(wakeup_pipe_r, &fds))
	{
		char c;
		if (read(wakeup_pipe_r, &c, 1) != 1)
		{
			ERROR_LOG(WIIMOTE, "Unable to read from wakeup pipe.");
		}
		return -1;
	}

	if (!FD_ISSET(int_sock, &fds))
		return -1;

	// Read the pending message into the buffer
	int r = read(int_sock, buf, MAX_PAYLOAD);
	if (r == -1)
	{
		// Error reading data
		ERROR_LOG(WIIMOTE, "Receiving data from wiimote %i.", index + 1);

		if (errno == ENOTCONN)
		{
			// This can happen if the bluetooth dongle is disconnected
			ERROR_LOG(WIIMOTE, "Bluetooth appears to be disconnected.  "
					"Wiimote %i will be disconnected.", index + 1);
		}

		r = 0;
	}

	return r;
}

int Wiimote::IOWrite(u8 const* buf, size_t len)
{
	return write(int_sock, buf, (int)len);
}

void Wiimote::EnablePowerAssertionInternal()
{}
void Wiimote::DisablePowerAssertionInternal()
{}

}; // WiimoteReal
