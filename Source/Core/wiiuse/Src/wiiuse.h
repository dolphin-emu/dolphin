/*
 *	wiiuse
 *
 *	Written By:
 *		Michael Laforest	< para >
 *		Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 *	Copyright 2006-2007
 *
 *	This file is part of wiiuse.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	$Header$
 *
 */

/**
 *	@file
 *
 *	@brief API header file.
 *
 *	If this file is included from inside the wiiuse source
 *	and not from a third party program, then wiimote_internal.h
 *	is also included which extends this file.
 */

#ifndef WIIUSE_H_INCLUDED
#define WIIUSE_H_INCLUDED

#ifdef _WIN32
	#include <windows.h>
#elif defined(__APPLE__)
	#import <IOBluetooth/IOBluetooth.h>
#elif defined(__linux__)
	#ifdef HAVE_CONFIG_H
		#include "config.h"
	#endif
	#if HAVE_BLUEZ
	#include <bluetooth/bluetooth.h>
	#endif
#endif

#ifdef WIIUSE_INTERNAL_H_INCLUDED
	#define WCONST
#else
	#define WCONST		const
#endif

/* led bit masks */
#define WIIMOTE_LED_NONE				0x00
#define WIIMOTE_LED_1					0x10
#define WIIMOTE_LED_2					0x20
#define WIIMOTE_LED_3					0x40
#define WIIMOTE_LED_4					0x80


/* wiimote option flags */
#define WIIUSE_SMOOTHING				0x01
#define WIIUSE_CONTINUOUS				0x02
#define WIIUSE_ORIENT_THRESH			0x04
#define WIIUSE_INIT_FLAGS				(WIIUSE_SMOOTHING | WIIUSE_ORIENT_THRESH)


/**
 *	@brief Return the IR sensitivity level.
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param lvl		[out] Pointer to an int that will hold the level setting.
 *	If no level is set 'lvl' will be set to 0.
 */
#define WIIUSE_GET_IR_SENSITIVITY(dev, lvl)									\
			do {														\
				if ((wm->state & 0x0200) == 0x0200) 		*lvl = 1;	\
				else if ((wm->state & 0x0400) == 0x0400) 	*lvl = 2;	\
				else if ((wm->state & 0x0800) == 0x0800) 	*lvl = 3;	\
				else if ((wm->state & 0x1000) == 0x1000) 	*lvl = 4;	\
				else if ((wm->state & 0x2000) == 0x2000) 	*lvl = 5;	\
				else									*lvl = 0;		\
			} while (0)



#define WIIUSE_IS_LED_SET(wm, num)		((wm->leds & WIIMOTE_LED_##num) == WIIMOTE_LED_##num)

/*
 *	Largest known payload is 21 bytes.
 *	Add 2 for the prefix and round up to a power of 2.
 */
#define MAX_PAYLOAD			32

/*
 *	This is left over from an old hack, but it may actually
 *	be a useful feature to keep so it wasn't removed.
 */
#define WIIMOTE_DEFAULT_TIMEOUT		30

typedef unsigned char byte;
typedef char sbyte;


/**
 *	@enum win32_bt_stack_t
 *	@brief	Available bluetooth stacks for Windows.
 */
typedef enum win_bt_stack_t {
	WIIUSE_STACK_UNKNOWN,
	WIIUSE_STACK_MS,
	WIIUSE_STACK_BLUESOLEIL
} win_bt_stack_t;


/**
 *	@enum WIIUSE_EVENT_TYPE
 *	@brief Events that wiiuse can generate from a poll.
 */
typedef enum WIIUSE_EVENT_TYPE {
	WIIUSE_NONE = 0,
	WIIUSE_EVENT,
	WIIUSE_STATUS,
	WIIUSE_CONNECT,
	WIIUSE_DISCONNECT,
	WIIUSE_UNEXPECTED_DISCONNECT,
	WIIUSE_READ_DATA,
	WIIUSE_NUNCHUK_INSERTED,
	WIIUSE_NUNCHUK_REMOVED,
	WIIUSE_CLASSIC_CTRL_INSERTED,
	WIIUSE_CLASSIC_CTRL_REMOVED,
	WIIUSE_GUITAR_HERO_3_CTRL_INSERTED,
	WIIUSE_GUITAR_HERO_3_CTRL_REMOVED,
	WIIUSE_WII_BOARD_CTRL_INSERTED,
	WIIUSE_WII_BOARD_CTRL_REMOVED,
	WIIUSE_MOTION_PLUS_INSERTED,
	WIIUSE_MOTION_PLUS_REMOVED
} WIIUSE_EVENT_TYPE;

/**
 *	@struct wiimote_t
 *	@brief Wiimote structure.
 */
typedef struct wiimote_t {
	WCONST int unid;				/**< user specified id				*/

	#if defined(__APPLE__)
		WCONST IOBluetoothDevice *btd;
		WCONST IOBluetoothL2CAPChannel *ichan;
		WCONST IOBluetoothL2CAPChannel *cchan;
	#define QUEUE_SIZE 64
		WCONST struct buffer {
			char data[MAX_PAYLOAD];
			int len;
		} queue[QUEUE_SIZE];
		WCONST int reader;
		WCONST int writer;
		WCONST int outstanding;
		WCONST int watermark;
	#elif defined(__linux__) && HAVE_BLUEZ
		WCONST bdaddr_t bdaddr;			/**< bt address	(linux)				*/
		WCONST char bdaddr_str[18];		/**< readable bt address			*/
		WCONST int out_sock;			/**< output socket				*/
		WCONST int in_sock;			/**< input socket 				*/
	#elif defined(_WIN32)
		WCONST char devicepath[255];		/**< unique wiimote reference			*/
		//WCONST ULONGLONG btaddr;		/**< bt address	(windows)			*/
		WCONST HANDLE dev_handle;		/**< HID handle					*/
		WCONST OVERLAPPED hid_overlap;		/**< overlap handle				*/
		WCONST enum win_bt_stack_t stack;	/**< type of bluetooth stack to use		*/
	#endif
	WCONST int timeout;				/**< read timeout				*/
	WCONST int state;				/**< various state flags			*/
	WCONST byte leds;				/**< currently lit leds				*/

	WCONST int flags;				/**< options flag				*/

	WCONST WIIUSE_EVENT_TYPE event;			/**< type of event that occured			*/
	WCONST byte event_buf[MAX_PAYLOAD];		/**< event buffer				*/
} wiimote;


/*****************************************
 *
 *	Include API specific stuff
 *
 *****************************************/

/* wiiuse.c */
extern const char* wiiuse_version();

extern struct wiimote_t** wiiuse_init(int wiimotes);
extern void wiiuse_disconnected(struct wiimote_t* wm);
extern void wiiuse_cleanup(struct wiimote_t** wm, int wiimotes);
extern void wiiuse_rumble(struct wiimote_t* wm, int status);
extern void wiiuse_set_leds(struct wiimote_t* wm, int leds);
extern int wiiuse_write_data(struct wiimote_t* wm, unsigned int addr, byte* data, byte len);

/* connect.c / io_win.c */
#ifdef _WIN32
extern int wiiuse_find(struct wiimote_t** wm, int max_wiimotes, int wiimotes);
#else
extern int wiiuse_find(struct wiimote_t** wm, int max_wiimotes, int timeout);
#endif
extern int wiiuse_connect(struct wiimote_t** wm, int wiimotes);
extern void wiiuse_disconnect(struct wiimote_t* wm);
extern void wiiuse_set_timeout(struct wiimote_t** wm, int wiimotes, byte timeout);

#ifdef _WIN32
extern int wiiuse_check_system_notification(unsigned int nMsg, WPARAM wParam, LPARAM lParam);
extern int wiiuse_register_system_notification(HWND hwnd);
#endif

/* ir.c */
extern void wiiuse_set_ir_sensitivity(struct wiimote_t* wm, int level);

/* io.c */
extern int wiiuse_io_read(struct wiimote_t* wm);
extern int wiiuse_io_write(struct wiimote_t* wm, byte* buf, int len);

#endif /* WIIUSE_H_INCLUDED */

