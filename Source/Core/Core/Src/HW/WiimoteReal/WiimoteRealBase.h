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

#ifndef WIIMOTE_COMM_H
#define WIIMOTE_COMM_H

#ifdef _WIN32
	#include <windows.h>
#elif defined(__APPLE__)
	#import <IOBluetooth/IOBluetooth.h>
#elif defined(__linux__) && HAVE_BLUEZ
	#include <bluetooth/bluetooth.h>
#endif

// Wiimote internal codes

// Communication channels
#define WM_OUTPUT_CHANNEL			0x11
#define WM_INPUT_CHANNEL			0x13

// The 4 most significant bits of the first byte of an outgoing command must be
// 0x50 if sending on the command channel and 0xA0 if sending on the interrupt
// channel. On Mac and Linux we use interrupt channel; on Windows, command.
#ifdef _WIN32
#define WM_SET_REPORT				0x50
#else
#define WM_SET_REPORT				0xA0
#endif

// Commands
#define WM_CMD_RUMBLE				0x10
#define WM_CMD_LED					0x11
#define WM_CMD_REPORT_TYPE			0x12
#define WM_CMD_IR					0x13
#define WM_CMD_SPEAKER_ENABLE		0x14
#define WM_CMD_CTRL_STATUS			0x15
#define WM_CMD_WRITE_DATA			0x16
#define WM_CMD_READ_DATA			0x17
#define WM_CMD_SPEAKER_DATA			0x18
#define WM_CMD_SPEAKER_MUTE			0x19
#define WM_CMD_IR_2					0x1A

#define WM_BT_INPUT					0x01
#define WM_BT_OUTPUT				0x02

// LED bit masks
#define WIIMOTE_LED_NONE			0x00
#define WIIMOTE_LED_1				0x10
#define WIIMOTE_LED_2				0x20
#define WIIMOTE_LED_3				0x40
#define WIIMOTE_LED_4				0x80

// End Wiimote internal codes

#define MAX_PAYLOAD					32
#define WIIMOTE_DEFAULT_TIMEOUT		30

#ifdef _WIN32
// Available bluetooth stacks for Windows.
typedef enum win_bt_stack_t
{
	MSBT_STACK_UNKNOWN,
	MSBT_STACK_MS,
	MSBT_STACK_BLUESOLEIL
} win_bt_stack_t;
#endif

#endif // WIIMOTE_COMM_H
