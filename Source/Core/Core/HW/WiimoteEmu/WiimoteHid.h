// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef WIIMOTE_HID_H
#define WIIMOTE_HID_H

#include "CommonTypes.h"

// what is this ?
#ifdef _MSC_VER
#pragma warning(disable:4200)
#endif
#pragma pack(push, 1)

// Source: HID_010_SPC_PFL/1.0 (official HID specification)

struct hid_packet
{
	u8 param : 4;
	u8 type : 4;
	u8 data[0];
};

#define HID_TYPE_HANDSHAKE 0
#define HID_TYPE_SET_REPORT 5
#define HID_TYPE_DATA 0xA

#define HID_HANDSHAKE_SUCCESS 0

#define HID_PARAM_INPUT 1
#define HID_PARAM_OUTPUT 2

//source: http://wiibrew.org/wiki/Wiimote

typedef u16 wm_core;

struct wm_accel
{
	u8 x, y, z;
};

// Four bytes for two objects. Filled with 0xFF if empty
struct wm_ir_basic
{
	u8 x1;
	u8 y1;
	u8 x2hi : 2;
	u8 y2hi : 2;
	u8 x1hi : 2;
	u8 y1hi : 2;
	u8 x2;
	u8 y2;
};

// Three bytes for one object
struct wm_ir_extended
{
	u8 x;
	u8 y;
	u8 size : 4;
	u8 xhi : 2;
	u8 yhi : 2;
};

struct wm_extension
{
	u8 jx; // joystick x, y
	u8 jy;
	u8 ax; // accelerometer
	u8 ay;
	u8 az;
	u8 bt; // buttons
};

struct wm_classic_extension
{
	u8 lx : 6; // byte 0
	u8 rx3 : 2;
	u8 ly : 6; // byte 1
	u8 rx2 : 2;
	u8 ry : 5; // byte 2
	u8 lt2 : 2;
	u8 rx1 : 1;
	u8 rt : 5; // byte 3
	u8 lt1 : 3;
	u16 bt; // byte 4, 5
};

struct wm_guitar_extension
{
	u8 sx   : 6;
	u8 pad1 : 2; // 1 on gh3, 0 on ghwt

	u8 sy   : 6;
	u8 pad2 : 2; // 1 on gh3, 0 on ghwt

	u8 tb   : 5; // not used in gh3
	u8 pad3 : 3; // always 0

	u8 whammy : 5;
	u8 pad4 : 3; // always 0

	u16 bt; // buttons
};

struct wm_drums_extension
{
	u8 sx   : 6;
	u8 pad1 : 2; // always 0

	u8 sy   : 6;
	u8 pad2 : 2; // always 0

	u8 pad3 : 1; // unknown
	u8 which : 5;
	u8 none : 1;
	u8 hhp : 1;

	u8 pad4 : 1; // unknown
	u8 velocity : 4; // unknown
	u8 softness : 3;

	u16 bt; // buttons
};

struct wm_turntable_extension
{
	u8 sx   : 6;
	u8 rtable3 : 2;

	u8 sy   : 6;
	u8 rtable2 : 2;

	u8 rtable4 : 1;
	u8 slider : 4;
	u8 dial2 : 2;
	u8 rtable1 : 1;

	u8 ltable1 : 5;
	u8 dial1 : 3;

	union
	{
		u16 ltable2 : 1;
		u16 bt; // buttons
	};
};

struct wm_motionplus_data
{
	u8 yaw1;

	u8 roll1;

	u8 pitch1;

	u8 yaw2 : 6;
	u8 yaw_slow : 1;
	u8 pitch_slow : 1;

	u8 roll2 : 6;
	u8 roll_slow : 1;
	u8 extension_connected : 1;

	u8 pitch2 : 6;
	u8 is_mp_data : 1;
	u8 zero : 1;
};

struct wm_report
{
	u8 wm;
	union
	{
		u8 data[0];
		struct
		{
			u8 rumble : 1;	// enable/disable rumble
			// only valid for certain reports
			u8 ack : 1;		// respond with an ack
			u8 enable : 1;	// enable/disable certain features
		};
	};
};

#define WM_RUMBLE 0x10

#define WM_LEDS 0x11
struct wm_leds
{
	u8 rumble : 1;
	// real wii also sets bit 0x2 (unknown purpose)
	u8 : 3;
	u8 leds : 4;
};

#define WM_REPORT_MODE 0x12
struct wm_report_mode
{
	u8 rumble : 1;
	// unsure what "all_the_time" actually is, the real wii does set it (bit 0x2)
	u8 all_the_time : 1;
	u8 continuous : 1;
	u8 : 5;
	u8 mode;
};

#define WM_IR_PIXEL_CLOCK 0x13
#define WM_IR_LOGIC 0x1A

#define WM_REQUEST_STATUS 0x15
struct wm_request_status
{
	u8 rumble : 1;
	u8 : 7;
};

#define WM_STATUS_REPORT 0x20
struct wm_status_report
{
	wm_core buttons;
	u8 battery_low : 1;
	u8 extension : 1;
	u8 speaker : 1;
	u8 ir : 1;
	u8 leds : 4;
	u8 padding2[2]; // two 00, TODO: this needs more investigation
	u8 battery;
};

#define WM_WRITE_DATA 0x16
struct wm_write_data
{
	u8 rumble : 1;
	u8 space : 2;	//see WM_SPACE_*
	u8 : 5;
	u8 address[3];
	u8 size;
	u8 data[16];
};

#define WM_ACK_DATA 0x22
struct wm_acknowledge
{
	wm_core buttons;
	u8 reportID;
	u8 errorID;
};

#define WM_READ_DATA 0x17
struct wm_read_data
{
	u8 rumble : 1;
	u8 space : 2;	//see WM_SPACE_*
	u8 : 5;
	u8 address[3];
	u16 size;
};

#define WM_SPACE_EEPROM 0
#define WM_SPACE_REGS1 1
#define WM_SPACE_REGS2 2
#define WM_SPACE_INVALID 3

#define WM_READ_DATA_REPLY 0x21
struct wm_read_data_reply
{
	wm_core buttons;
	u8 error : 4;	//see WM_RDERR_*
	u8 size : 4;
	u16 address;
	u8 data[16];
};

#define WM_RDERR_WOREG 7
#define WM_RDERR_NOMEM 8


// Data reports

#define WM_REPORT_CORE 0x30
struct wm_report_core
{
	wm_core c;
};

#define WM_REPORT_CORE_ACCEL 0x31
struct wm_report_core_accel
{
	wm_core c;
	wm_accel a;
};

#define WM_REPORT_CORE_EXT8 0x32

#define WM_REPORT_CORE_ACCEL_IR12 0x33
struct wm_report_core_accel_ir12
{
	wm_core c;
	wm_accel a;
	wm_ir_extended ir[4];
};

#define WM_REPORT_CORE_EXT19 0x34
#define WM_REPORT_CORE_ACCEL_EXT16 0x35
struct wm_report_core_accel_ext16
{
	wm_core c;
	wm_accel a;
	wm_extension ext;
	//wm_ir_basic ir[2];
	u8 pad[10];

};

#define WM_REPORT_CORE_IR10_EXT9 0x36

#define WM_REPORT_CORE_ACCEL_IR10_EXT6 0x37
struct wm_report_core_accel_ir10_ext6
{
	wm_core c;
	wm_accel a;
	wm_ir_basic ir[2];
	//u8 ext[6];
	wm_extension ext;
};

#define WM_REPORT_EXT21 0x3d // never used?
struct wm_report_ext21
{
	u8 ext[21];
};

#define WM_REPORT_INTERLEAVE1 0x3e
#define WM_REPORT_INTERLEAVE2 0x3f

#define WM_SPEAKER_ENABLE 0x14
#define WM_SPEAKER_MUTE 0x19
#define WM_WRITE_SPEAKER_DATA 0x18
struct wm_speaker_data
{
	u8 unknown : 3;
	u8 length : 5;
	u8 data[20];
};

// Custom structs

/**
 *	@struct accel_t
 *	@brief Accelerometer struct. For any device with an accelerometer.
 */
struct accel_cal
{
	struct
	{
		u8 x, y, z;
		u8 xlo : 2;
		u8 ylo : 2;
		u8 zlo : 2;
	} zero_g;

	struct
	{
		u8 x, y, z;
		u8 xlo : 2;
		u8 ylo : 2;
		u8 zlo : 2;
	} one_g;
};

struct nu_js
{
	u8 max, min, center;
};

struct cc_trigger
{
	u8 neutral;
};

struct nu_cal
{
	wm_accel cal_zero;		// zero calibration
	u8 pad1;
	wm_accel cal_g;			// g size
	u8 pad2;
	nu_js jx;				//
	nu_js jy;				//
	u8 sum[2];
};

struct cc_cal
{
	nu_js Lx;				//
	nu_js Ly;				//
	nu_js Rx;				//
	nu_js Ry;				//
	cc_trigger Tl;			//
	cc_trigger Tr;			//
};

struct gh3_cal
{
	nu_js Lx;
	nu_js Ly;
};

#pragma pack(pop)

#endif	//WIIMOTE_HID_H
