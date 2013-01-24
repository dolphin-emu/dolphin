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

#include <sys/time.h>
#include <errno.h>

#include <sys/types.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include "Common.h"
#include "WiimoteReal.h"
#include "Host.h"

namespace WiimoteReal
{

// Find wiimotes.
// Does not replace already found wiimotes even if they are disconnected.
// wm is an array of max_wiimotes wiimotes
// Returns the total number of found wiimotes.
int FindWiimotes(Wiimote** wm, int max_wiimotes)
{
	int device_id;
	int device_sock;
	int found_devices;
	int found_wiimotes = 0;
	int i;

	// Count the number of already found wiimotes
	for (i = 0; i < MAX_WIIMOTES; ++i)
	{
		if (wm[i])
			found_wiimotes++;
	}

	// Get the id of the first bluetooth device.
	if ((device_id = hci_get_route(NULL)) < 0)
	{
		NOTICE_LOG(WIIMOTE, "Bluetooth not found.");
		return found_wiimotes;
	}

	// Create a socket to the device
	if ((device_sock = hci_open_dev(device_id)) < 0)
	{
		ERROR_LOG(WIIMOTE, "Unable to open bluetooth.");
		return found_wiimotes;
	}

	int try_num = 0;
	while ((try_num < 5) && (found_wiimotes < max_wiimotes))
	{
		inquiry_info scan_info_arr[128];
		inquiry_info* scan_info = scan_info_arr;
		memset(&scan_info_arr, 0, sizeof(scan_info_arr));

		// Scan for bluetooth devices for approximately one second
		found_devices = hci_inquiry(device_id, 1, 128, NULL, &scan_info, IREQ_CACHE_FLUSH);
		if (found_devices < 0)
		{
			ERROR_LOG(WIIMOTE, "Error searching for bluetooth devices.");
			return found_wiimotes;
		}

		DEBUG_LOG(WIIMOTE, "Found %i bluetooth device(s).", found_devices);

		// Display discovered devices
		for (i = 0; (i < found_devices) && (found_wiimotes < max_wiimotes); ++i)
		{
			char name[1000];
			memset(name, 0, sizeof(name));
			ERROR_LOG(WIIMOTE, "found a device...");
			if (hci_read_remote_name(device_sock, &scan_info[i].bdaddr, sizeof(name), name, 0) < 0) {
				ERROR_LOG(WIIMOTE, "name request failed");
				continue;
			}
			ERROR_LOG(WIIMOTE, "device name %s", name);
			if (IsValidBluetoothName(name))
			{
				bool new_wiimote = true;
				// Determine if this wiimote has already been found.
				for (int j = 0; j < MAX_WIIMOTES && new_wiimote; ++j)
				{
					if (wm[j] && bacmp(&scan_info[i].bdaddr,&wm[j]->bdaddr) == 0)
						new_wiimote = false;
				}

				if (new_wiimote)
				{
					// Find an unused slot
					unsigned int k = 0;
					for (; k < MAX_WIIMOTES && !(WIIMOTE_SRC_REAL & g_wiimote_sources[k] && !wm[k]); ++k);
					wm[k] = new Wiimote(k);

					// Found a new device
					char bdaddr_str[18];
					ba2str(&scan_info[i].bdaddr, bdaddr_str);

					NOTICE_LOG(WIIMOTE, "Found wiimote %i, (%s).",
							wm[k]->index + 1, bdaddr_str);

					wm[k]->bdaddr = scan_info[i].bdaddr;
					++found_wiimotes;
				}
			}
		}
		try_num++;
	}

	close(device_sock);
	return found_wiimotes;
}

// Connect to a wiimote with a known address.
bool Wiimote::Connect()
{
	if (IsConnected())
		return false;

	sockaddr_l2 addr;
	addr.l2_family = AF_BLUETOOTH;
	addr.l2_bdaddr = bdaddr;
	addr.l2_cid = 0;

	// Output channel
	addr.l2_psm = htobs(WM_OUTPUT_CHANNEL);
	if ((cmd_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1 ||
			connect(cmd_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		DEBUG_LOG(WIIMOTE, "Unable to open output socket to wiimote.");
		close(cmd_sock);
		cmd_sock = -1;
		return false;
	}

	// Input channel
	addr.l2_psm = htobs(WM_INPUT_CHANNEL);
	if ((int_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP)) == -1 ||
			connect(int_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		DEBUG_LOG(WIIMOTE, "Unable to open input socket from wiimote.");
		close(int_sock);
		close(cmd_sock);
		int_sock = cmd_sock = -1;
		return false;
	}

	NOTICE_LOG(WIIMOTE, "Connected to wiimote %i.", index + 1);

	m_connected = true;

	// Do the handshake
	Handshake();

	// Set LEDs
	SetLEDs(WIIMOTE_LED_1 << index);

	m_wiimote_thread = std::thread(std::mem_fun(&Wiimote::ThreadFunc), this);

	return true;
}

// Disconnect a wiimote.
void Wiimote::RealDisconnect()
{
	if (!IsConnected())
		return;

	NOTICE_LOG(WIIMOTE, "Disconnecting wiimote %i.", index + 1);

	m_connected = false;

	if (m_wiimote_thread.joinable())
		m_wiimote_thread.join();

	Close();
}

void Wiimote::Close()
{
	if (IsOpen())
	{
		Host_ConnectWiimote(index, false);

		close(cmd_sock);
		close(int_sock);

		cmd_sock = -1;
		int_sock = -1;
	}
}

bool Wiimote::IsOpen() const
{
	return IsConnected() && cmd_sock != -1 && int_sock != -1;
}

int Wiimote::IORead(unsigned char *buf)
{
	// Block select for 1/2000th of a second
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = WIIMOTE_DEFAULT_TIMEOUT * 1000;

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(int_sock, &fds);

	if (select(int_sock + 1, &fds, NULL, NULL, &tv) == -1)
	{
		ERROR_LOG(WIIMOTE, "Unable to select wiimote %i input socket.", index + 1);
		return 0;
	}

	if (!FD_ISSET(int_sock, &fds))
		return 0;

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
			Close();
		}

		return 0;
	}
	else if (!r)
	{
		// Disconnect
		Close();
	}

	return r;
}

int Wiimote::IOWrite(unsigned char* buf, int len)
{
	return write(int_sock, buf, len);
}

}; // WiimoteReal
