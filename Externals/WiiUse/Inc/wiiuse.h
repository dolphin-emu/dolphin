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
	/* windows */
	#include <windows.h>
#else
	/* nix */
	#include <bluetooth/bluetooth.h>
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
#ifdef WIN32
	#define WIIMOTE_DEFAULT_TIMEOUT		30
	#define WIIMOTE_EXP_TIMEOUT			10
#endif

typedef unsigned char byte;
typedef char sbyte;

struct wiimote_t;



/**
 *      @brief Callback that handles a read event.
 *
 *      @param wm               Pointer to a wiimote_t structure.
 *      @param data             Pointer to the filled data block.
 *      @param len              Length in bytes of the data block.
 *
 *      @see wiiuse_init()
 *
 *      A registered function of this type is called automatically by the wiiuse
 *      library when the wiimote has returned the full data requested by a previous
 *      call to wiiuse_read_data().
 */
typedef void (*wiiuse_read_cb)(struct wiimote_t* wm, byte* data, unsigned short len);


/**
 *	@struct read_req_t
 *	@brief Data read request structure.
 */
struct read_req_t {
	wiiuse_read_cb cb;			/**< read data callback											*/
	byte* buf;					/**< buffer where read data is written							*/
	unsigned int addr;			/**< the offset that the read started at						*/
	unsigned short size;		/**< the length of the data read								*/
	unsigned short wait;		/**< num bytes still needed to finish read						*/
	byte dirty;					/**< set to 1 if not using callback and needs to be cleaned up	*/

	struct read_req_t* next;	/**< next read request in the queue								*/
};


/**
 *	@struct vec2b_t
 *	@brief Unsigned x,y byte vector.
 */
typedef struct vec2b_t {
	byte x, y;
} vec2b_t;


/**
 *	@struct vec3b_t
 *	@brief Unsigned x,y,z byte vector.
 */
typedef struct vec3b_t {
	byte x, y, z;
} vec3b_t;


/**
 *	@struct vec3f_t
 *	@brief Signed x,y,z float struct.
 */
typedef struct vec3f_t {
	float x, y, z;
} vec3f_t;


/**
 *	@struct orient_t
 *	@brief Orientation struct.
 *
 *	Yaw, pitch, and roll range from -180 to 180 degrees.
 */
typedef struct orient_t {
	float roll;						/**< roll, this may be smoothed if enabled	*/
	float pitch;					/**< pitch, this may be smoothed if enabled	*/
	float yaw;

	float a_roll;					/**< absolute roll, unsmoothed				*/
	float a_pitch;					/**< absolute pitch, unsmoothed				*/
} orient_t;


/**
 *	@struct gforce_t
 *	@brief Gravity force struct.
 */
typedef struct gforce_t {
	float x, y, z;
} gforce_t;


/**
 *	@struct accel_t
 *	@brief Accelerometer struct. For any device with an accelerometer.
 */
typedef struct accel_t {
	struct vec3b_t cal_zero;		/**< zero calibration					*/
	struct vec3b_t cal_g;			/**< 1g difference around 0cal			*/

	float st_roll;					/**< last smoothed roll value			*/
	float st_pitch;					/**< last smoothed roll pitch			*/
	float st_alpha;					/**< alpha value for smoothing [0-1]	*/
} accel_t;


/**
 *	@struct ir_dot_t
 *	@brief A single IR source.
 */
typedef struct ir_dot_t {
	byte visible;					/**< if the IR source is visible		*/

	unsigned int x;					/**< interpolated X coordinate			*/
	unsigned int y;					/**< interpolated Y coordinate			*/

	short rx;						/**< raw X coordinate (0-1023)			*/
	short ry;						/**< raw Y coordinate (0-767)			*/

	byte order;						/**< increasing order by x-axis value	*/

	byte size;						/**< size of the IR dot (0-15)			*/
} ir_dot_t;


/**
 *	@enum aspect_t
 *	@brief Screen aspect ratio.
 */
typedef enum aspect_t {
	WIIUSE_ASPECT_4_3,
	WIIUSE_ASPECT_16_9
} aspect_t;


/**
 *	@struct ir_t
 *	@brief IR struct. Hold all data related to the IR tracking.
 */
typedef struct ir_t {
	struct ir_dot_t dot[4];			/**< IR dots							*/
	byte num_dots;					/**< number of dots at this time		*/

	enum aspect_t aspect;			/**< aspect ratio of the screen			*/

	enum ir_position_t pos;			/**< IR sensor bar position				*/

	unsigned int vres[2];			/**< IR virtual screen resolution		*/
	int offset[2];					/**< IR XY correction offset			*/
	int state;						/**< keeps track of the IR state		*/

	int ax;							/**< absolute X coordinate				*/
	int ay;							/**< absolute Y coordinate				*/

	int x;							/**< calculated X coordinate			*/
	int y;							/**< calculated Y coordinate			*/

	float distance;					/**< pixel distance between first 2 dots*/
	float z;						/**< calculated distance				*/
} ir_t;


/**
 *	@struct joystick_t
 *	@brief Joystick calibration structure.
 *
 *	The angle \a ang is relative to the positive y-axis into quadrant I
 *	and ranges from 0 to 360 degrees.  So if the joystick is held straight
 *	upwards then angle is 0 degrees.  If it is held to the right it is 90,
 *	down is 180, and left is 270.
 *
 *	The magnitude \a mag is the distance from the center to where the
 *	joystick is being held.  The magnitude ranges from 0 to 1.
 *	If the joystick is only slightly tilted from the center the magnitude
 *	will be low, but if it is closer to the outter edge the value will
 *	be higher.
 */
typedef struct joystick_t {
	struct vec2b_t max;				/**< maximum joystick values	*/
	struct vec2b_t min;				/**< minimum joystick values	*/
	struct vec2b_t center;			/**< center joystick values		*/

	float ang;						/**< angle the joystick is being held		*/
	float mag;						/**< magnitude of the joystick (range 0-1)	*/
} joystick_t;


/**
 *	@struct nunchuk_t
 *	@brief Nunchuk expansion device.
 */
typedef struct nunchuk_t {
	struct accel_t accel_calib;		/**< nunchuk accelerometer calibration		*/
	struct joystick_t js;			/**< joystick calibration					*/

	int* flags;						/**< options flag (points to wiimote_t.flags) */

	byte btns;						/**< what buttons have just been pressed	*/
	byte btns_held;					/**< what buttons are being held down		*/
	byte btns_released;				/**< what buttons were just released this	*/

	float orient_threshold;			/**< threshold for orient to generate an event */
	int accel_threshold;			/**< threshold for accel to generate an event */

	struct vec3b_t accel;			/**< current raw acceleration data			*/
	struct orient_t orient;			/**< current orientation on each axis		*/
	struct gforce_t gforce;			/**< current gravity forces on each axis	*/
} nunchuk_t;


/**
 *	@struct classic_ctrl_t
 *	@brief Classic controller expansion device.
 */
typedef struct classic_ctrl_t {
	short btns;						/**< what buttons have just been pressed	*/
	short btns_held;				/**< what buttons are being held down		*/
	short btns_released;			/**< what buttons were just released this	*/

	float r_shoulder;				/**< right shoulder button (range 0-1)		*/
	float l_shoulder;				/**< left shoulder button (range 0-1)		*/

	struct joystick_t ljs;			/**< left joystick calibration				*/
	struct joystick_t rjs;			/**< right joystick calibration				*/
} classic_ctrl_t;


/**
 *	@struct guitar_hero_3_t
 *	@brief Guitar Hero 3 expansion device.
 */
typedef struct guitar_hero_3_t {
	short btns;						/**< what buttons have just been pressed	*/
	short btns_held;				/**< what buttons are being held down		*/
	short btns_released;			/**< what buttons were just released this	*/

	float whammy_bar;				/**< whammy bar (range 0-1)					*/

	struct joystick_t js;			/**< joystick calibration					*/
} guitar_hero_3_t;


/**
 *	@struct expansion_t
 *	@brief Generic expansion device plugged into wiimote.
 */
typedef struct expansion_t {
	int type;						/**< type of expansion attached				*/

	union {
		struct nunchuk_t nunchuk;
		struct classic_ctrl_t classic;
		struct guitar_hero_3_t gh3;
	};
} expansion_t;


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
 *	@struct wiimote_state_t
 *	@brief Significant data from the previous event.
 */
typedef struct wiimote_state_t {
	/* expansion_t */
	float exp_ljs_ang;
	float exp_rjs_ang;
	float exp_ljs_mag;
	float exp_rjs_mag;
	unsigned short exp_btns;
	struct orient_t exp_orient;
	struct vec3b_t exp_accel;
	float exp_r_shoulder;
	float exp_l_shoulder;

	/* ir_t */
	int ir_ax;
	int ir_ay;
	float ir_distance;

	struct orient_t orient;
	unsigned short btns;

	struct vec3b_t accel;
} wiimote_state_t;


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
	WIIUSE_GUITAR_HERO_3_CTRL_REMOVED
} WIIUSE_EVENT_TYPE;

/**
 *	@struct wiimote_t
 *	@brief Wiimote structure.
 */
typedef struct wiimote_t {
	WCONST int unid;						/**< user specified id						*/

	#ifndef WIN32
		WCONST bdaddr_t bdaddr;				/**< bt address								*/
		WCONST char bdaddr_str[18];			/**< readable bt address					*/
		WCONST int out_sock;				/**< output socket							*/
		WCONST int in_sock;					/**< input socket 							*/
	#else
		WCONST char devicepath[255];		/**< reference for removing/adding new wiimotes to wiimote_t	*/
		WCONST HANDLE dev_handle;			/**< HID handle								*/
		WCONST OVERLAPPED hid_overlap;		/**< overlap handle							*/
		WCONST enum win_bt_stack_t stack;	/**< type of bluetooth stack to use			*/
		WCONST int timeout;					/**< read timeout							*/
	#endif

	WCONST int state;						/**< various state flags					*/
	WCONST byte leds;						/**< currently lit leds						*/
	WCONST int flags;						/**< options flag							*/
	WCONST byte handshake_state;			/**< the state of the connection handshake	*/

	WCONST struct read_req_t* read_req;		/**< list of data read requests				*/
	WCONST WIIUSE_EVENT_TYPE event;			/**< type of event that occured				*/
	WCONST byte event_buf[MAX_PAYLOAD];		/**< event buffer							*/
} wiimote;


/*****************************************
 *
 *	Include API specific stuff
 *
 *****************************************/

#ifdef _WIN32
	#define WIIUSE_EXPORT_DECL __declspec(dllexport)
	#define WIIUSE_IMPORT_DECL __declspec(dllimport)
#else
	#define WIIUSE_EXPORT_DECL
	#define WIIUSE_IMPORT_DECL
#endif

#ifdef WIIUSE_COMPILE_LIB
	#define WIIUSE_EXPORT WIIUSE_EXPORT_DECL
#else
	#define WIIUSE_EXPORT WIIUSE_IMPORT_DECL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* wiiuse.c */
WIIUSE_EXPORT extern const char* wiiuse_version();

WIIUSE_EXPORT extern struct wiimote_t** wiiuse_init(int wiimotes);
WIIUSE_EXPORT extern void wiiuse_disconnected(struct wiimote_t* wm);
WIIUSE_EXPORT extern void wiiuse_cleanup(struct wiimote_t** wm, int wiimotes);
WIIUSE_EXPORT extern void wiiuse_rumble(struct wiimote_t* wm, int status);
WIIUSE_EXPORT extern void wiiuse_toggle_rumble(struct wiimote_t* wm);
WIIUSE_EXPORT extern void wiiuse_set_leds(struct wiimote_t* wm, int leds);
WIIUSE_EXPORT extern int wiiuse_write_data(struct wiimote_t* wm, unsigned int addr, byte* data, byte len);
WIIUSE_EXPORT extern int wiiuse_set_flags(struct wiimote_t* wm, int enable, int disable);
WIIUSE_EXPORT extern void wiiuse_set_timeout(struct wiimote_t** wm, int wiimotes, byte timeout);


/* connect.c */
#ifdef _WIN32
WIIUSE_EXPORT extern int wiiuse_find(struct wiimote_t** wm, int max_wiimotes, int wiimotes);
#else
WIIUSE_EXPORT extern int wiiuse_find(struct wiimote_t** wm, int max_wiimotes, int timeout);
#endif
WIIUSE_EXPORT extern int wiiuse_connect(struct wiimote_t** wm, int wiimotes);
WIIUSE_EXPORT extern void wiiuse_disconnect(struct wiimote_t* wm);
#ifdef WIN32
WIIUSE_EXPORT extern int wiiuse_check_system_notification(unsigned int nMsg, WPARAM wParam, LPARAM lParam);
WIIUSE_EXPORT extern int wiiuse_register_system_notification(HWND hwnd);
#endif

/* ir.c */
WIIUSE_EXPORT extern void wiiuse_set_ir_sensitivity(struct wiimote_t* wm, int level);

/* io.c */
WIIUSE_EXPORT extern int wiiuse_io_read(struct wiimote_t* wm);
WIIUSE_EXPORT extern int wiiuse_io_write(struct wiimote_t* wm, byte* buf, int len);


#ifdef __cplusplus
}
#endif


#endif /* WIIUSE_H_INCLUDED */

