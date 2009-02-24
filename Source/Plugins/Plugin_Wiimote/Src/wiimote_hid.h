// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef WIIMOTE_HID_H
#define WIIMOTE_HID_H

#include <CommonTypes.h>

#ifdef _MSC_VER
#pragma warning(disable:4200)
#endif
#pragma pack(push, 1)

// Source: HID_010_SPC_PFL/1.0 (official HID specification)

struct hid_packet {
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

struct wm_report {
	u8 channel;
	u8 data[0];
};

#define WM_RUMBLE 0x10
#define WM_LEDS 0x11
struct wm_leds {
	u8 rumble : 1;
	u8 : 3;
	u8 leds : 4;
};

#define WM_DATA_REPORTING 0x12
struct wm_data_reporting {
	u8 rumble : 1;
	u8 continuous : 1;
	u8 all_the_time : 1;
	u8 : 5;
	u8 mode;
};

#define WM_IR_PIXEL_CLOCK 0x13
#define WM_IR_LOGIC 0x1A

#define WM_REQUEST_STATUS 0x15
struct wm_request_status {
	u8 rumble : 1;
	u8 : 7;
};

#define WM_STATUS_REPORT 0x20
struct wm_status_report {
	u8 padding1[2]; // two 00
	u8 battery_low : 1;
	u8 extension : 1;
	u8 speaker : 1;
	u8 ir : 1;
	u8 leds : 4;
	u8 padding2[2]; // two 00
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

#define WM_WRITE_DATA_REPLY 0x22	//empty, afaik
struct wm_acknowledge 
{
	u8 Channel;
	u8 unk0; // Core buttons state (wm_core), can be zero
	u8 unk1;
	u8 reportID;
	u8 errorID;
};

#define WM_READ_DATA 0x17
struct wm_read_data {
	u8 rumble : 1;
	u8 space : 2;	//see WM_SPACE_*
	u8 : 5;
	u8 address[3];
	u8 size[2];
};

#define WM_SPACE_EEPROM 0
#define WM_SPACE_REGS1 1
#define WM_SPACE_REGS2 2
#define WM_SPACE_INVALID 3

#define WM_READ_DATA_REPLY 0x21
struct wm_read_data_reply {
	u16 buttons;
	u8 error : 4;	//see WM_RDERR_*
	u8 size : 4;
	u16 address;
	u8 data[16];
};

#define WM_RDERR_WOREG 7
#define WM_RDERR_NOMEM 8

struct wm_core {
	u8 left : 1;
	u8 right : 1;
	u8 down : 1;
	u8 up : 1;
	u8 plus : 1;
	u8 : 3;
	u8 two : 1;
	u8 one : 1;
	u8 b : 1;
	u8 a : 1;
	u8 minus : 1;
	u8 : 2;
	u8 home : 1;
};

struct wm_accel {
	u8 x, y, z;
};

// Four bytes for two objects. Filled with 0xFF if empty
struct wm_ir_basic
{
	u8 x1;
	u8 y1;
	u8 x2Hi : 2;
	u8 y2Hi : 2;
	u8 x1Hi : 2;
	u8 y1Hi : 2;
	u8 x2;
	u8 y2;
};

// Three bytes for one object
struct wm_ir_extended 
{
	u8 x;
	u8 y;
	u8 size : 4;
	u8 xHi : 2;
	u8 yHi : 2;
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

struct wm_cc_4
{
	u8 padding : 1;
	u8 bRT : 1;
	u8 bP : 1;
	u8 bH : 1;
	u8 bM : 1;
	u8 bLT : 1;
	u8 bdD : 1;
	u8 bdR : 1;
};

struct wm_cc_5
{
	u8 bdU : 1;
	u8 bdL : 1;
	u8 bZR : 1;
	u8 bX : 1;
	u8 bA : 1;
	u8 bY : 1;
	u8 bB : 1;
	u8 bZL : 1;
};

struct wm_classic_extension
{
	u8 Lx : 6; // byte 0
	u8 Rx : 2;
	u8 Ly : 6; // byte 1
	u8 Rx2 : 2;
	u8 Ry : 5; // byte 2
	u8 lT : 2;
	u8 Rx3 : 1;
	u8 rT : 5; // byte 3
	u8 lT2 : 3;
	wm_cc_4 b1; // byte 4
	wm_cc_5 b2; // byte 5
};

//******************************************************************************
// Data reports
//******************************************************************************

#define WM_REPORT_CORE 0x30
struct wm_report_core {
	wm_core c;
};

#define WM_REPORT_CORE_ACCEL 0x31
struct wm_report_core_accel {
	wm_core c;
	wm_accel a;
};

#define WM_REPORT_CORE_EXT8 0x32

#define WM_REPORT_CORE_ACCEL_IR12 0x33
struct wm_report_core_accel_ir12 {
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


//******************************************************************************
// Custom structs
//******************************************************************************

/**
 *	@struct accel_t
 *	@brief Accelerometer struct. For any device with an accelerometer.
 */
struct accel_cal
{
	wm_accel cal_zero;		/**< zero calibration					*/
	wm_accel cal_g;			/**< 1g difference around 0cal			*/
};

struct nu_js {
	u8 max, min, center;
};
struct cc_trigger {
	u8 neutral;
};
struct nu_cal
{
	wm_accel cal_zero;		// zero calibratio
	wm_accel cal_g;			// g size
	nu_js jx;				//
	nu_js jy;				//
};
struct cc_cal
{
	nu_js Lx;				//
	nu_js Ly;				//
	nu_js Rx;				//
	nu_js Ry;				//
	cc_trigger Tl;				//
	cc_trigger Tr;				//
};

#pragma pack(pop)

#endif	//WIIMOTE_HID_H
