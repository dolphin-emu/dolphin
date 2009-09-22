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
 *	@brief Guitar Hero 3 expansion device.
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
#include "guitar_hero_3.h"

static void guitar_hero_3_pressed_buttons(struct guitar_hero_3_t* gh3, short now);

/**
 *	@brief Handle the handshake data from the guitar.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 *	@param data		The data read in from the device.
 *	@param len		The length of the data block, in bytes.
 *
 *	@return	Returns 1 if handshake was successful, 0 if not.
 */
int guitar_hero_3_handshake(struct wiimote_t* wm, struct guitar_hero_3_t* gh3, byte* data, unsigned short len) {
	int offset = 0;

	/*
	 *	The good fellows that made the Guitar Hero 3 controller
	 *	failed to factory calibrate the devices.  There is no
	 *	calibration data on the device.
	 */

	gh3->btns = 0;
	gh3->btns_held = 0;
	gh3->btns_released = 0;
	gh3->wb_raw = 0;
	gh3->whammy_bar = 0.0f;
	gh3->tb_raw = 0;
	gh3->touch_bar = -1;

	/* joystick stuff */
	gh3->js.max.x = GUITAR_HERO_3_JS_MAX_X;
	gh3->js.min.x = GUITAR_HERO_3_JS_MIN_X;
	gh3->js.center.x = GUITAR_HERO_3_JS_CENTER_X;
	gh3->js.max.y = GUITAR_HERO_3_JS_MAX_Y;
	gh3->js.min.y = GUITAR_HERO_3_JS_MIN_Y;
	gh3->js.center.y = GUITAR_HERO_3_JS_CENTER_Y;

	/* handshake done */
	wm->event = WIIUSE_GUITAR_HERO_3_CTRL_INSERTED;
	wm->exp.type = EXP_GUITAR_HERO_3;

	#ifdef WIN32
	wm->timeout = WIIMOTE_DEFAULT_TIMEOUT;
	#endif

	return 1;
}


/**
 *	@brief The guitar disconnected.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 */
void guitar_hero_3_disconnected(struct guitar_hero_3_t* gh3) {
	memset(gh3, 0, sizeof(struct guitar_hero_3_t));
}



/**
 *	@brief Handle guitar event.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 *	@param msg		The message specified in the event packet.
 */
void guitar_hero_3_event(struct guitar_hero_3_t* gh3, byte* msg) {
	int i;

	/* decrypt data */
	for (i = 0; i < 6; ++i)
		msg[i] = (msg[i] ^ 0x17) + 0x17;

	guitar_hero_3_pressed_buttons(gh3, BIG_ENDIAN_SHORT(*(short*)(msg + 4)));

	gh3->js.pos.x = (msg[0] & GUITAR_HERO_3_JS_MASK);
	gh3->js.pos.y = (msg[1] & GUITAR_HERO_3_JS_MASK);
	gh3->tb_raw = (msg[2] & GUITAR_HERO_3_TOUCH_MASK);
	gh3->wb_raw = (msg[3] & GUITAR_HERO_3_WHAMMY_MASK);

	/* touch bar */
	gh3->touch_bar = 0;
	if (gh3->tb_raw > 0x1B)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_ORANGE;
	else if (gh3->tb_raw > 0x18)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_ORANGE | GUITAR_HERO_3_TOUCH_BLUE;
	else if (gh3->tb_raw > 0x15)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_BLUE;
	else if (gh3->tb_raw > 0x13)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_BLUE | GUITAR_HERO_3_TOUCH_YELLOW;
	else if (gh3->tb_raw > 0x10)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_YELLOW;
	else if (gh3->tb_raw > 0x0D)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_AVAILABLE;
	else if (gh3->tb_raw > 0x0B)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_YELLOW | GUITAR_HERO_3_TOUCH_RED;
	else if (gh3->tb_raw > 0x08)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_RED;
	else if (gh3->tb_raw > 0x05)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_RED | GUITAR_HERO_3_TOUCH_GREEN;
	else if (gh3->tb_raw > 0x02)
		gh3->touch_bar = GUITAR_HERO_3_TOUCH_GREEN;

	/* whammy bar */
	gh3->whammy_bar = (msg[3] - GUITAR_HERO_3_WHAMMY_BAR_MIN) / (float)(GUITAR_HERO_3_WHAMMY_BAR_MAX - GUITAR_HERO_3_WHAMMY_BAR_MIN);

	/* joy stick */
	calc_joystick_state(&gh3->js, gh3->js.pos.x, gh3->js.pos.y);

}


/**
 *	@brief Find what buttons are pressed.
 *
 *	@param cc		A pointer to a classic_ctrl_t structure.
 *	@param msg		The message byte specified in the event packet.
 */
static void guitar_hero_3_pressed_buttons(struct guitar_hero_3_t* gh3, short now) {

	/* message is inverted (0 is active, 1 is inactive) */
	now = ~now & GUITAR_HERO_3_BUTTON_ALL;

	/* preserve old btns pressed */
	gh3->btns_last = gh3->btns;

	/* pressed now & were pressed, then held */
	gh3->btns_held = (now & gh3->btns);

	/* were pressed or were held & not pressed now, then released */
	gh3->btns_released = ((gh3->btns | gh3->btns_held) & ~now);

	/* buttons pressed now */
	gh3->btns = now;
}
