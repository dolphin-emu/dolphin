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
 *	@brief General internal wiiuse stuff.
 *
 *	Since Wiiuse is a library, wiiuse.h is a duplicate
 *	of the API header.
 *
 *	The code that would normally go in that file, but
 *	which is not needed by third party developers,
 *	is put here.
 *
 *	So wiiuse_internal.h is included by other files
 *	internally, wiiuse.h is included only here.
 */

#ifndef WIIUSE_INTERNAL_H_INCLUDED
#define WIIUSE_INTERNAL_H_INCLUDED

/* wiiuse version */
#define WIIUSE_VERSION					"0.12"

/********************
 *
 *	Wiimote internal codes
 *
 ********************/

/* Communication channels */
#define WM_OUTPUT_CHANNEL			0x11
#define WM_INPUT_CHANNEL			0x13

#define WM_SET_REPORT				0x50

/* commands */
#define WM_CMD_LED					0x11
#define WM_CMD_REPORT_TYPE			0x12
#define WM_CMD_RUMBLE				0x13
#define WM_CMD_IR					0x13
#define WM_CMD_CTRL_STATUS			0x15
#define WM_CMD_SPEAKER_ENABLE		0x14// Size 1
#define WM_CMD_WRITE_DATA			0x16
#define WM_CMD_READ_DATA			0x17
#define WM_CMD_SPEAKER_DATA			0x18// Size 21
#define WM_CMD_SPEAKER_MUTE			0x19// Size 1
#define WM_CMD_IR_2					0x1A


#define WM_BT_INPUT					0x01
#define WM_BT_OUTPUT				0x02

/* Identify the wiimote device by its class */
#define WM_DEV_CLASS_0				0x04
#define WM_DEV_CLASS_1				0x25
#define WM_DEV_CLASS_2				0x00
#define WM_VENDOR_ID				0x057E
#define WM_PRODUCT_ID				0x0306



/* offsets in wiimote memory */
#define WM_MEM_OFFSET_CALIBRATION	0x16

#define WM_REG_IR_BLOCK1			0x04B00000
#define WM_REG_IR_BLOCK2			0x04B0001A


/* ir block data */
#define WM_IR_BLOCK1_LEVEL1			"\x02\x00\x00\x71\x01\x00\x64\x00\xfe"
#define WM_IR_BLOCK2_LEVEL1			"\xfd\x05"
#define WM_IR_BLOCK1_LEVEL2			"\x02\x00\x00\x71\x01\x00\x96\x00\xb4"
#define WM_IR_BLOCK2_LEVEL2			"\xb3\x04"
#define WM_IR_BLOCK1_LEVEL3			"\x02\x00\x00\x71\x01\x00\xaa\x00\x64"
#define WM_IR_BLOCK2_LEVEL3			"\x63\x03"
#define WM_IR_BLOCK1_LEVEL4			"\x02\x00\x00\x71\x01\x00\xc8\x00\x36"
#define WM_IR_BLOCK2_LEVEL4			"\x35\x03"
#define WM_IR_BLOCK1_LEVEL5			"\x07\x00\x00\x71\x01\x00\x72\x00\x20"
#define WM_IR_BLOCK2_LEVEL5			"\x1f\x03"

#define WM_IR_TYPE_BASIC			0x01
#define WM_IR_TYPE_EXTENDED			0x03
#define WM_IR_TYPE_FULL				0x05




/********************
 *
 *	End Wiimote internal codes
 *
 ********************/

/* wiimote state flags - (some duplicated in wiiuse.h)*/
#define WIIMOTE_STATE_DEV_FOUND				0x0001
#define WIIMOTE_STATE_HANDSHAKE				0x0002	/* actual connection exists but no handshake yet */
#define WIIMOTE_STATE_HANDSHAKE_COMPLETE	0x0004	/* actual connection exists but no handshake yet */
#define WIIMOTE_STATE_CONNECTED				0x0008
#define WIIMOTE_STATE_RUMBLE				0x0010
#define WIIMOTE_STATE_ACC					0x0020
#define WIIMOTE_STATE_EXP					0x0040
#define WIIMOTE_STATE_IR					0x0080
#define WIIMOTE_STATE_SPEAKER				0x0100
#define WIIMOTE_STATE_IR_SENS_LVL1			0x0200
#define WIIMOTE_STATE_IR_SENS_LVL2			0x0400
#define WIIMOTE_STATE_IR_SENS_LVL3			0x0800
#define WIIMOTE_STATE_IR_SENS_LVL4			0x1000
#define WIIMOTE_STATE_IR_SENS_LVL5			0x2000

#define WIIMOTE_INIT_STATES					(WIIMOTE_STATE_IR_SENS_LVL3)

/* macro to manage states */
#define WIIMOTE_IS_SET(wm, s)			((wm->state & (s)) == (s))
#define WIIMOTE_ENABLE_STATE(wm, s)		(wm->state |= (s))
#define WIIMOTE_DISABLE_STATE(wm, s)	(wm->state &= ~(s))
#define WIIMOTE_TOGGLE_STATE(wm, s)		((wm->state & (s)) ? WIIMOTE_DISABLE_STATE(wm, s) : WIIMOTE_ENABLE_STATE(wm, s))

#define WIIMOTE_IS_FLAG_SET(wm, s)		((wm->flags & (s)) == (s))
#define WIIMOTE_ENABLE_FLAG(wm, s)		(wm->flags |= (s))
#define WIIMOTE_DISABLE_FLAG(wm, s)		(wm->flags &= ~(s))
#define WIIMOTE_TOGGLE_FLAG(wm, s)		((wm->flags & (s)) ? WIIMOTE_DISABLE_FLAG(wm, s) : WIIMOTE_ENABLE_FLAG(wm, s))

#define NUNCHUK_IS_FLAG_SET(wm, s)		((*(wm->flags) & (s)) == (s))

/* misc macros */
#define WIIMOTE_ID(wm)					(wm->unid)
#define WIIMOTE_IS_CONNECTED(wm)		(WIIMOTE_IS_SET(wm, WIIMOTE_STATE_CONNECTED))



#include "wiiuse.h"

#ifdef __cplusplus
extern "C" {
#endif

/* not part of the api */
int wiiuse_set_report_type(struct wiimote_t* wm);
int wiiuse_send(struct wiimote_t* wm, byte report_type, byte* msg, int len);

#ifdef __cplusplus
}
#endif

#endif /* WIIUSE_INTERNAL_H_INCLUDED */
