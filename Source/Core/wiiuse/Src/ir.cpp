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
 *	@brief Handles IR data.
 */

#include <stdio.h>
#include <math.h>

#ifndef _WIN32
	#include <unistd.h>
#endif

#include "Common.h"
#include "wiiuse_internal.h"

static int get_ir_sens(struct wiimote_t* wm, const char** block1, const char** block2);

/**
 *	@brief	Get the IR sensitivity settings.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param block1	[out] Pointer to where block1 will be set.
 *	@param block2	[out] Pointer to where block2 will be set.
 *
 *	@return Returns the sensitivity level.
 */
static int get_ir_sens(struct wiimote_t* wm, const char** block1, const char** block2) {
	if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR_SENS_LVL1)) {
		*block1 = WM_IR_BLOCK1_LEVEL1;
		*block2 = WM_IR_BLOCK2_LEVEL1;
		return 1;
	} else if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR_SENS_LVL2)) {
		*block1 = WM_IR_BLOCK1_LEVEL2;
		*block2 = WM_IR_BLOCK2_LEVEL2;
		return 2;
	} else if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR_SENS_LVL3)) {
		*block1 = WM_IR_BLOCK1_LEVEL3;
		*block2 = WM_IR_BLOCK2_LEVEL3;
		return 3;
	} else if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR_SENS_LVL4)) {
		*block1 = WM_IR_BLOCK1_LEVEL4;
		*block2 = WM_IR_BLOCK2_LEVEL4;
		return 4;
	} else if (WIIMOTE_IS_SET(wm, WIIMOTE_STATE_IR_SENS_LVL5)) {
		*block1 = WM_IR_BLOCK1_LEVEL5;
		*block2 = WM_IR_BLOCK2_LEVEL5;
		return 5;
	}

	*block1 = NULL;
	*block2 = NULL;
	return 0;
}


/**
 *	@brief	Set the IR sensitivity.
 *
 *	@param wm		Pointer to a wiimote_t structure.
 *	@param level	1-5, same as Wii system sensitivity setting.
 *
 *	If the level is < 1, then level will be set to 1.
 *	If the level is > 5, then level will be set to 5.
 */
void wiiuse_set_ir_sensitivity(struct wiimote_t* wm, int level) {
	const char* block1 = NULL;
	const char* block2 = NULL;

	if (!wm)	return;

	if (level > 5)		level = 5;
	if (level < 1)		level = 1;

	WIIMOTE_DISABLE_STATE(wm, (WIIMOTE_STATE_IR_SENS_LVL1 |
								WIIMOTE_STATE_IR_SENS_LVL2 |
								WIIMOTE_STATE_IR_SENS_LVL3 |
								WIIMOTE_STATE_IR_SENS_LVL4 |
								WIIMOTE_STATE_IR_SENS_LVL5));

	switch (level) {
		case 1:
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_IR_SENS_LVL1);
			break;
		case 2:
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_IR_SENS_LVL2);
			break;
		case 3:
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_IR_SENS_LVL3);
			break;
		case 4:
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_IR_SENS_LVL4);
			break;
		case 5:
			WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_IR_SENS_LVL5);
			break;
		default:
			return;
	}

	/* set the new sensitivity */
	get_ir_sens(wm, &block1, &block2);

	wiiuse_write_data(wm, WM_REG_IR_BLOCK1, (byte*)block1, 9);
	wiiuse_write_data(wm, WM_REG_IR_BLOCK2, (byte*)block2, 2);

	DEBUG_LOG(WIIMOTE, "Set IR sensitivity to level %i (unid %i)", level, wm->unid);
}
