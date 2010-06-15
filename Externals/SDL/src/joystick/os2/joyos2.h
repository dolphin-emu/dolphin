/*****************************************************************************/
/*                                                                           */
/* COPYRIGHT    Copyright (C) 1995 IBM Corporation                           */
/*                                                                           */
/*    The following IBM OS/2 source code is provided to you solely for       */
/*    the purpose of assisting you in your development of OS/2 device        */
/*    drivers. You may use this code in accordance with the IBM License      */
/*    Agreement provided in the IBM Device Driver Source Kit for OS/2. This  */
/*    Copyright statement may not be removed.                                */
/*                                                                           */
/*****************************************************************************/
#ifndef JOYOS2_H
#define JOYOS2_H

/****** GAMEPORT.SYS joystick definitions, start *****************************/
#define GAME_VERSION    0x20           /* 2.0 First IBM version */
#define GAMEPDDNAME     "GAME$   "
#define IOCTL_CAT_USER	0x80
#define GAME_PORT_GET	0x20		/* read GAMEPORT.SYS values */
#define GAME_PORT_RESET 0x60		/* reset joystick mask with given value */

#pragma pack(1)				/* pack structure size is 1 byte */
typedef struct {			/* GAMEPORT.SYS structure */
	USHORT	usJs_AxCnt;		/* Joystick_A X position */
	USHORT	usJs_AyCnt;		/* Joystick_A Y position */
	USHORT	usJs_BxCnt;		/* Joystick_B X position */
	USHORT	usJs_ByCnt;		/* Joystick_B Y position */
	USHORT	usJs_ButtonA1Cnt;	/* button A1 press count */
	USHORT	usJs_ButtonA2Cnt;	/* button A2 press count */
	USHORT	usJs_ButtonB1Cnt;	/* button B1 press count */
	USHORT	usJs_ButtonB2Cnt;	/* button B2 press count */
	UCHAR	ucJs_JoyStickMask;	/* mask of connected joystick pots */
	UCHAR	ucJs_ButtonStatus;	/* bits of switches down */
	ULONG	ulJs_Ticks;		/* joystick clock ticks */
} GAME_PORT_STRUCT;
#pragma pack()				/*reset to normal pack size */
/****** GAMEPORT.SYS joystick definitions, end *******************************/


/****************************************************************************/
#define GAME_GET_VERSION                0x01
#define GAME_GET_PARMS                  0x02
#define GAME_SET_PARMS                  0x03
#define GAME_GET_CALIB                  0x04
#define GAME_SET_CALIB                  0x05
#define GAME_GET_DIGSET                 0x06
#define GAME_SET_DIGSET                 0x07
#define GAME_GET_STATUS                 0x10
#define GAME_GET_STATUS_BUTWAIT         0x11
#define GAME_GET_STATUS_SAMPWAIT        0x12
/****************************************************************************/

/****************************************************************************/
// bit masks for each axis
#define JOY_AX_BIT      0x01
#define JOY_AY_BIT      0x02
#define JOY_A_BITS      (JOY_AX_BIT|JOY_AY_BIT)
#define JOY_BX_BIT      0x04
#define JOY_BY_BIT      0x08
#define JOY_B_BITS      (JOY_BX_BIT|JOY_BY_BIT)
#define JOY_ALLPOS_BITS (JOY_A_BITS|JOY_B_BITS)

// bit masks for each button
#define JOY_BUT1_BIT    0x10
#define JOY_BUT2_BIT    0x20
#define JOY_BUT3_BIT    0x40
#define JOY_BUT4_BIT    0x80
#define JOY_ALL_BUTS    (JOY_BUT1_BIT|JOY_BUT2_BIT|JOY_BUT3_BIT|JOY_BUT4_BIT)
/****************************************************************************/

/****************************************************************************/
// 1-D position struct used for each axis
typedef SHORT   GAME_POS;       /* some data formats require signed values */

// simple 2-D position for each joystick
typedef struct
{
        GAME_POS                x;
        GAME_POS                y;
}
GAME_2DPOS_STRUCT;

// struct defining the instantaneous state of both sticks and all buttons
typedef struct
{
        GAME_2DPOS_STRUCT       A;
        GAME_2DPOS_STRUCT       B;
        USHORT                  butMask;
}
GAME_DATA_STRUCT;

// struct to be used for calibration and digital response on each axis
typedef struct
{
        GAME_POS                lower;
        GAME_POS                centre;
        GAME_POS                upper;
}
GAME_3POS_STRUCT;
/****************************************************************************/

/****************************************************************************/
// status struct returned to OS/2 applications:
// current data for all sticks as well as button counts since last read
typedef struct
{
        GAME_DATA_STRUCT        curdata;
        USHORT                  b1cnt;
        USHORT                  b2cnt;
        USHORT                  b3cnt;
        USHORT                  b4cnt;
}
GAME_STATUS_STRUCT;
/****************************************************************************/

/****************************************************************************/
/* in use bitmasks originating in 0.2b */
#define GAME_USE_BOTH_OLDMASK   0x01    /* for backward compat with bool */
#define GAME_USE_X_NEWMASK      0x02
#define GAME_USE_Y_NEWMASK      0x04
#define GAME_USE_X_EITHERMASK   (GAME_USE_X_NEWMASK|GAME_USE_BOTH_OLDMASK)
#define GAME_USE_Y_EITHERMASK   (GAME_USE_Y_NEWMASK|GAME_USE_BOTH_OLDMASK)
#define GAME_USE_BOTH_NEWMASK   (GAME_USE_X_NEWMASK|GAME_USE_Y_NEWMASK)

/* only timed sampling implemented in version 1.0 */
#define GAME_MODE_TIMED         1       /* timed sampling */
#define GAME_MODE_REQUEST       2       /* request driven sampling */

/* only raw implemented in version 1.0 */
#define GAME_DATA_FORMAT_RAW    1       /* [l,c,r]   */
#define GAME_DATA_FORMAT_SIGNED 2       /* [-l,0,+r] */
#define GAME_DATA_FORMAT_BINARY 3       /* {-1,0,+1} */
#define GAME_DATA_FORMAT_SCALED 4       /* [-10,+10] */

// parameters defining the operation of the driver
typedef struct
{
        USHORT                  useA;           /* new bitmasks: see above */
        USHORT                  useB;
        USHORT                  mode;           /* see consts above */
        USHORT                  format;         /* see consts above */
        USHORT                  sampDiv;        /* samp freq = 32 / n */
        USHORT                  scale;          /* scaling factor */
        USHORT                  res1;           /* must be 0 */
        USHORT                  res2;           /* must be 0 */
}
GAME_PARM_STRUCT;
/****************************************************************************/

/****************************************************************************/
// calibration values for each axis:
//      - upper limit on value to be considered in lower range
//      - centre value
//      - lower limit on value to be considered in upper range
typedef struct
{
        GAME_3POS_STRUCT        Ax;
        GAME_3POS_STRUCT        Ay;
        GAME_3POS_STRUCT        Bx;
        GAME_3POS_STRUCT        By;
}
GAME_CALIB_STRUCT;
/****************************************************************************/

/****************************************************************************/
// struct defining the digital response values for all axes
typedef struct
{
        GAME_3POS_STRUCT        Ax;
        GAME_3POS_STRUCT        Ay;
        GAME_3POS_STRUCT        Bx;
        GAME_3POS_STRUCT        By;
}
GAME_DIGSET_STRUCT;
/****************************************************************************/

#endif
