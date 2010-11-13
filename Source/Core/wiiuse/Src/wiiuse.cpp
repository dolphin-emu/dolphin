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
 *	@brief General wiimote operations.
 *
 *	The file includes functions that handle general
 *	tasks.  Most of these are functions that are part
 *	of the API.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
	#include <unistd.h>
#else
	#include <Winsock2.h>
#endif

#include "Common.h"
#include "wiiuse_internal.h"

static int g_banner = 1;

/**
 *	@breif Returns the version of the library.
 */
const char* wiiuse_version() {
	return WIIUSE_VERSION;
}


/**
 *	@brief Clean up wiimote_t array created by wiiuse_init()
 */
void wiiuse_cleanup(struct wiimote_t** wm, int wiimotes) {
	int i = 0;

	if (!wm)
		return;

	NOTICE_LOG(WIIMOTE, "wiiuse clean up...");

	for (; i < wiimotes; ++i) {
		wiiuse_disconnect(wm[i]);
		free(wm[i]);
	}

	free(wm);

	return;
}


/**
 *	@brief Initialize an array of wiimote structures.
 *
 *	@param wiimotes		Number of wiimote_t structures to create.
 *
 *	@return An array of initialized wiimote_t structures.
 *
 *	@see wiiuse_connect()
 *
 *	The array returned by this function can be passed to various
 *	functions, including wiiuse_connect().
 */
struct wiimote_t** wiiuse_init(int wiimotes) {
	int i = 0;
	struct wiimote_t** wm = NULL;

	/*
	 *	Please do not remove this banner.
	 *	GPL asks that you please leave output credits intact.
	 *	Thank you.
	 *
	 *	This banner is only displayed once so that if you need
	 *	to call this function again it won't be intrusive.
	 */
	if (!g_banner) {
		printf(	"wiiuse v" WIIUSE_VERSION " loaded.\n"
				"  By: Michael Laforest <thepara[at]gmail{dot}com>\n"
				"  http://wiiuse.net  http://wiiuse.sf.net\n");
		g_banner = 1;
	}

	if (!wiimotes)
		return NULL;

	wm = (struct wiimote_t **)malloc(sizeof(struct wiimote_t*) * wiimotes);

	for (i = 0; i < wiimotes; ++i) {
		wm[i] = (struct wiimote_t *)malloc(sizeof(struct wiimote_t));
		memset(wm[i], 0, sizeof(struct wiimote_t));

		wm[i]->unid = i+1;

		#if defined __linux__ && HAVE_BLUEZ
			wm[i]->bdaddr = *BDADDR_ANY;
			wm[i]->out_sock = -1;
			wm[i]->in_sock = -1;
		#elif defined(_WIN32)
			wm[i]->dev_handle = 0;
			wm[i]->stack = WIIUSE_STACK_UNKNOWN;
		#endif

		wm[i]->timeout  = WIIMOTE_DEFAULT_TIMEOUT;
		wm[i]->state = WIIMOTE_INIT_STATES;
		wm[i]->flags = WIIUSE_INIT_FLAGS;
		wm[i]->event = WIIUSE_NONE;
	}

	return wm;
}


/**
 *	@brief	The wiimote disconnected.
 *
 *	@param wm	Pointer to a wiimote_t structure.
 */
void wiiuse_disconnected(struct wiimote_t* wm) {
	if (!wm)	return;

	NOTICE_LOG(WIIMOTE, "Wiimote disconnected [id %i].", wm->unid);

	/* disable the connected flag */
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_CONNECTED);

	/* reset a bunch of stuff */
	wm->leds = 0;
	wm->state = WIIMOTE_INIT_STATES;
	memset(wm->event_buf, 0, sizeof(wm->event_buf));

	#if defined __linux__ && HAVE_BLUEZ
		wm->out_sock = -1;
		wm->in_sock = -1;
	#elif defined(_WIN32)
		CloseHandle(wm->dev_handle);
		wm->dev_handle = 0;
	#endif

	wm->event = WIIUSE_DISCONNECT;
}


/**
 *	@brief	Enable or disable the rumble.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param status	1 to enable, 0 to disable.
 */
void wiiuse_rumble(struct wiimote_t* wm, int status) {
	byte buf;

	if (!wm || !WIIMOTE_IS_CONNECTED(wm))
		return;

	/* make sure to keep the current lit leds */
	buf = wm->leds;

	if (status) {
		DEBUG_LOG(WIIMOTE, "Starting rumble...");
		WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_RUMBLE);
		buf |= 0x01;
	} else {
		DEBUG_LOG(WIIMOTE, "Stopping rumble...");
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_RUMBLE);
	}

	/* preserve IR state */
	if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR))
		buf |= 0x04;

	wiiuse_send(wm, WM_CMD_RUMBLE, &buf, 1);
}

/**
 *	@brief	Set the enabled LEDs.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param leds		What LEDs to enable.
 *
 *	\a leds is a bitwise or of WIIMOTE_LED_1, WIIMOTE_LED_2, WIIMOTE_LED_3, or WIIMOTE_LED_4.
 */
void wiiuse_set_leds(struct wiimote_t* wm, int leds) {
	byte buf;

	if (!wm || !WIIMOTE_IS_CONNECTED(wm))
		return;

	/* remove the lower 4 bits because they control rumble */
	wm->leds = (leds & 0xF0);
	buf = wm->leds;

	/* make sure if the rumble is on that we keep it on */
	if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_RUMBLE))
		buf |= 0x01;

	wiiuse_send(wm, WM_CMD_LED, &buf, 1);
}


/**
 *	@brief	Set the report type based on the current wiimote state.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *
 *	@return The report type sent.
 *
 *	The wiimote reports formatted packets depending on the
 *	report type that was last requested.  This function will
 *	update the type of report that should be sent based on
 *	the current state of the device.
 */
int wiiuse_set_report_type(struct wiimote_t* wm) {
	byte buf[2];
	int motion, expansion, ir;

	if (!wm || !WIIMOTE_IS_CONNECTED(wm))
		return 0;

	buf[0] = (WIIMOTE_IS_FLAG_SET(wm, WIIUSE_CONTINUOUS) ? 0x04 : 0x00);	/* set to 0x04 for continuous reporting */

	/* if rumble is enabled, make sure we keep it */
	if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_RUMBLE))
		buf[0] |= 0x01;

	motion = WIIMOTE_IS_SET(wm, WIIMOTE_STATE_ACC);
	expansion = WIIMOTE_IS_SET(wm, WIIMOTE_STATE_EXP);
	ir = WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR);

	buf[1] = 0x30;


	DEBUG_LOG(WIIMOTE, "Setting report type: 0x%x", buf[1]);

	expansion = wiiuse_send(wm, WM_CMD_REPORT_TYPE, buf, 2);
	if (expansion <= 0)
		return expansion;

	return buf[1];
}



/**
 *	@brief	Write data to the wiimote.
 *
 *	@param wm			Pointer to a wiimote_t structure.
 *	@param addr			The address to write to.
 *	@param data			The data to be written to the memory location.
 *	@param len			The length of the block to be written.
 */
int wiiuse_write_data(struct wiimote_t* wm, unsigned int addr, byte* data, byte len) {
	byte buf[21] = {0};		/* the payload is always 23 */

	if (!wm || !WIIMOTE_IS_CONNECTED(wm))
		return 0;
	if (!data || !len)
		return 0;

	DEBUG_LOG(WIIMOTE, "Writing %i bytes to memory location 0x%x...", len, addr);

	#ifdef WITH_WIIUSE_DEBUG
	{
		int i = 0;
		printf("Write data is: ");
		for (; i < len; ++i)
			printf("%x ", data[i]);
		printf("\n");
	}
	#endif

	/* the offset is in big endian */
	*(int*)(buf) = Common::swap32(addr); /* XXX only if little-endian */

	/* length */
	*(byte*)(buf + 4) = len;

	/* data */
	memcpy(buf + 5, data, len);

	wiiuse_send(wm, WM_CMD_WRITE_DATA, buf, 21);
	return 1;
}


/**
 *	@brief	Send a packet to the wiimote.
 *
 *	@param wm			Pointer to a wiimote_t structure.
 *	@param report_type	The report type to send (WIIMOTE_CMD_LED, WIIMOTE_CMD_RUMBLE, etc). Found in wiiuse.h
 *	@param msg			The payload.
 *	@param len			Length of the payload in bytes.
 *
 *	This function should replace any write()s directly to the wiimote device.
 */
int wiiuse_send(struct wiimote_t* wm, byte report_type, byte* msg, int len) {
	byte buf[32];		/* no payload is better than this */
	int rumble = 0;

	buf[0] = WM_SET_REPORT | WM_BT_OUTPUT;
	buf[1] = report_type;

	switch (report_type) {
		case WM_CMD_LED:
		case WM_CMD_RUMBLE:
		case WM_CMD_CTRL_STATUS:
		{
			/* Rumble flag for: 0x11, 0x13, 0x14, 0x15, 0x19 or 0x1a */
			if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_RUMBLE))
				rumble = 1;
			break;
		}
		default:
			break;
	}

	memcpy(buf+2, msg, len);
	if (rumble)
		buf[2] |= 0x01;

	#ifdef WITH_WIIUSE_DEBUG
	{
		int x = 2;
		printf("[DEBUG] (id %i) SEND: (%x) %.2x ", wm->unid, buf[0], buf[1]);
		#ifndef _WIN32
		for (; x < len+2; ++x)
		#else
		for (; x < len+1; ++x)
		#endif
			printf("%.2x ", buf[x]);
		printf("\n");
	}
	#endif

	return wiiuse_io_write(wm, buf, len+2);
}




/**
 *	@brief	Set the bluetooth stack type to use.
 *
 *	@param wm		Array of wiimote_t structures.
 *	@param wiimotes	Number of objects in the wm array.
 *	@param type		The type of bluetooth stack to use.
 */
void wiiuse_set_bluetooth_stack(struct wiimote_t** wm, int wiimotes, enum win_bt_stack_t type) {
	#ifdef _WIN32
	int i;

	if (!wm)	return;

	for (i = 0; i < wiimotes; ++i)
		wm[i]->stack = type;
	#endif
}

/**
 *	@brief Set the normal and expansion handshake timeouts.
 *
 *	@param wm				Array of wiimote_t structures.
 *	@param wiimotes			Number of objects in the wm array.
 *	@param normal_timeout	The timeout in milliseconds for a normal read.
 *	@param exp_timeout		The timeout in millisecondsd to wait for an expansion handshake.
 */
void wiiuse_set_timeout(struct wiimote_t** wm, int wiimotes, byte timeout) {
	int i;

	if (!wm)	return;

	for (i = 0; i < wiimotes; ++i) {
		wm[i]->timeout = timeout;
	}
}
