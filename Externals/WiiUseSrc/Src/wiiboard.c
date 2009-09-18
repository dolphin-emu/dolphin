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
 *	@brief Wii Fit Balance Board device.
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

static short big_to_lil(unsigned short num)
{
	short ret = num;
	char *bret = (char*)&ret;
	char tmp = bret[1];
	bret[1] = bret[0];
	bret[0] = tmp;
	return ret;
}

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
	int i;
	short* handshake_short;
	/* decrypt data */
	printf("DECRYPTED DATA WIIBOARD\n");
	for (i = 0; i < len; i++)
	{
		if(i%16==0)
		{			
			if(i!=0)
				printf("\n");

			printf("%X: ",0x4a40000+32+i);
		}
		printf("%02X ", data[i]);
	}
	printf("\n");


	handshake_short = (short*)data;
	
	wb->ctr[0] = big_to_lil(handshake_short[2]);
	wb->cbr[0] = big_to_lil(handshake_short[3]);
	wb->ctl[0] = big_to_lil(handshake_short[4]);
	wb->cbl[0] = big_to_lil(handshake_short[5]);

	wb->ctr[1] = big_to_lil(handshake_short[6]);
	wb->cbr[1] = big_to_lil(handshake_short[7]);
	wb->ctl[1] = big_to_lil(handshake_short[8]);
	wb->cbl[1] = big_to_lil(handshake_short[9]);

	wb->ctr[2] = big_to_lil(handshake_short[10]);
	wb->cbr[2] = big_to_lil(handshake_short[11]);
	wb->ctl[2] = big_to_lil(handshake_short[12]);
	wb->cbl[2] = big_to_lil(handshake_short[13]);


	/* handshake done */
	wm->exp.type = EXP_WII_BOARD;


	#ifdef WIN32
	wm->timeout = WIIMOTE_DEFAULT_TIMEOUT;
	#endif

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
	short *shmsg = (short*)(msg);
	wb->rtr = big_to_lil(shmsg[0]);
	if(wb->rtr<0) wb->rtr = 0;
	wb->rbr = big_to_lil(shmsg[1]);
	if(wb->rbr<0) wb->rbr = 0;
	wb->rtl = big_to_lil(shmsg[2]);
	if(wb->rtl<0) wb->rtl = 0;
	wb->rbl = big_to_lil(shmsg[3]);		
	if(wb->rbl<0) wb->rbl = 0;

	/* 
		Interpolate values 
		Calculations borrowed from wiili.org - No names to mention sadly :( http://www.wiili.org/index.php/Wii_Balance_Board_PC_Drivers page however!
	*/

	if(wb->rtr<wb->ctr[1])
	{
		wb->tr = 68*(wb->rtr-wb->ctr[0])/(wb->ctr[1]-wb->ctr[0]);
	}
	else if(wb->rtr >= wb->ctr[1])
	{
		wb->tr = 68*(wb->rtr-wb->ctr[1])/(wb->ctr[2]-wb->ctr[1]) + 68;
	}

	if(wb->rtl<wb->ctl[1])
	{
		wb->tl = 68*(wb->rtl-wb->ctl[0])/(wb->ctl[1]-wb->ctl[0]);
	}
	else if(wb->rtl >= wb->ctl[1])
	{
		wb->tl = 68*(wb->rtl-wb->ctl[1])/(wb->ctl[2]-wb->ctl[1]) + 68;
	}

	if(wb->rbr<wb->cbr[1])
	{
		wb->br = 68*(wb->rbr-wb->cbr[0])/(wb->cbr[1]-wb->cbr[0]);
	}
	else if(wb->rbr >= wb->cbr[1])
	{
		wb->br = 68*(wb->rbr-wb->cbr[1])/(wb->cbr[2]-wb->cbr[1]) + 68;
	}

	if(wb->rbl<wb->cbl[1])
	{
		wb->bl = 68*(wb->rbl-wb->cbl[0])/(wb->cbl[1]-wb->cbl[0]);
	}
	else if(wb->rbl >= wb->cbl[1])
	{
		wb->bl = 68*(wb->rbl-wb->cbl[1])/(wb->cbl[2]-wb->cbl[1]) + 68;
	}	
}

void wiiuse_set_wii_board_calib(struct wiimote_t *wm)
{
}
