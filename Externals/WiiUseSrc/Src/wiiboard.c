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
 *	@brief Wiiboard expansion device.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef WIN32
	#include <Winsock2.h>
#endif

#include "definitions.h"
#include "wiiuse_internal.h"
#include "dynamics.h"
#include "events.h"
#include "wiiboard.h"

/**
 *	@brief Handle the handshake data from the guitar.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 *	@param data		The data read in from the device.
 *	@param len		The length of the data block, in bytes.
 *
 *	@return	Returns 1 if handshake was successful, 0 if not.
 */

int wii_board_handshake(struct wiimote_t* wm, struct wii_board_t* wb, byte* data, unsigned short len) {

	int offset = 0;
	if (data[offset]==0xff) {
		if (data[offset+16]==0xff) {
			WIIUSE_DEBUG("Wii Balance Board handshake appears invalid, trying again.");
			wiiuse_read_data(wm, data, WM_EXP_MEM_CALIBR, EXP_HANDSHAKE_LEN);
			return 0;
		}
		offset += 16;
	}

	wb->ctr[0] = (data[offset+4]<<8)|data[offset+5];
	wb->cbr[0] = (data[offset+6]<<8)|data[offset+7];
	wb->ctl[0] = (data[offset+8]<<8)|data[offset+9];
	wb->cbl[0] = (data[offset+10]<<8)|data[offset+11];

	wb->ctr[1] = (data[offset+12]<<8)|data[offset+13];
	wb->cbr[1] = (data[offset+14]<<8)|data[offset+15];
	wb->ctl[1] = (data[offset+16]<<8)|data[offset+17];
	wb->cbl[1] = (data[offset+18]<<8)|data[offset+19];

	wb->ctr[2] = (data[offset+20]<<8)|data[offset+21];
	wb->cbr[2] = (data[offset+22]<<8)|data[offset+23];
	wb->ctl[2] = (data[offset+24]<<8)|data[offset+25];
	wb->cbl[2] = (data[offset+26]<<8)|data[offset+27];

	/* handshake done */
	wm->event = WIIUSE_WII_BOARD_CTRL_INSERTED;
	wm->exp.type = EXP_WII_BOARD;

	return 1; 
}


/**
 *	@brief The wii board disconnected.
 *
 *	@param cc		A pointer to a wii_board_t structure.
 */
void wii_board_disconnected(struct wii_board_t* wb) {
	memset(wb, 0, sizeof(struct wii_board_t));
}

/**
 *	@brief Handle guitar event.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 *	@param msg		The message specified in the event packet.
 */
void wii_board_event(struct wii_board_t* wb, byte* msg) {
	wb->rtr = (msg[0]<<8)|msg[1];
	wb->rbr = (msg[2]<<8)|msg[3];
	wb->rtl = (msg[4]<<8)|msg[5];
	wb->rbl = (msg[6]<<8)|msg[7];	
	calc_balanceboard_state(wb);
}
