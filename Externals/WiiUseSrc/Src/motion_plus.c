#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef WIN32
	#include <Winsock2.h>
#endif

#include "definitions.h"
#include "wiiuse_internal.h"
#include "dynamics.h"
#include "events.h"
#include "wiiboard.h"
#include "io.h"

void wiiuse_motion_plus_check(struct wiimote_t *wm,ubyte *data,uword len)
{
	u32 val;
	if(data == NULL)
	{
		wiiuse_read_data(wm, wm->motion_plus_id, WM_EXP_ID, 6, wiiuse_motion_plus_check);
	}
	else
	{
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_FAILED);
		WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_HANDSHAKE);
		val = (data[3] << 16) | (data[2] << 24) | (data[4] << 8) | data[5];
		if(val == EXP_ID_CODE_MOTION_PLUS)
		{
			/* handshake done */
			wm->event = WIIUSE_MOTION_PLUS_ACTIVATED;
			wm->exp.type = EXP_MOTION_PLUS;
			
			WIIMOTE_ENABLE_STATE(wm,WIIMOTE_STATE_EXP);
			wiiuse_set_ir_mode(wm);
		}
	}
}

static void wiiuse_set_motion_plus_clear2(struct wiimote_t *wm,ubyte *data,uword len)
{
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP);
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_FAILED);
	WIIMOTE_DISABLE_STATE(wm, WIIMOTE_STATE_EXP_HANDSHAKE);
	wiiuse_set_ir_mode(wm);
	wiiuse_status(wm,NULL);
}

static void wiiuse_set_motion_plus_clear1(struct wiimote_t *wm,ubyte *data,uword len)
{
	ubyte val = 0x00;
	wiiuse_write_data(wm,WM_EXP_MEM_ENABLE1,&val,1,wiiuse_set_motion_plus_clear2);
}


void wiiuse_set_motion_plus(struct wiimote_t *wm, int status)
{
	ubyte val;
	
	if(WIIMOTE_IS_SET(wm,WIIMOTE_STATE_EXP_HANDSHAKE))
		return;
	
	WIIMOTE_ENABLE_STATE(wm, WIIMOTE_STATE_EXP_HANDSHAKE);
	if(status)
	{
		val = 0x04;
		wiiuse_write_data(wm,WM_EXP_MOTION_PLUS_ENABLE,&val,1,wiiuse_motion_plus_check);
	}
	else
	{
		wiiuse_disable_expansion(wm);
		val = 0x55;
		wiiuse_write_data(wm,WM_EXP_MEM_ENABLE1,&val,1,wiiuse_set_motion_plus_clear1);
	}
}

void motion_plus_disconnected(struct motion_plus_t* mp)
{
	WIIUSE_DEBUG("Motion plus disconnected");
	memset(mp, 0, sizeof(struct motion_plus_t));
}

void motion_plus_event(struct motion_plus_t* mp, ubyte* msg)
{
	mp->rx = ((msg[5] & 0xFC) << 6) | msg[2]; // Pitch
	mp->ry = ((msg[4] & 0xFC) << 6) | msg[1]; // Roll
	mp->rz = ((msg[3] & 0xFC) << 6) | msg[0]; // Yaw

	mp->ext = msg[4] & 0x1;
	mp->status = (msg[3] & 0x3) | ((msg[4] & 0x2) << 1); // roll, yaw, pitch
}