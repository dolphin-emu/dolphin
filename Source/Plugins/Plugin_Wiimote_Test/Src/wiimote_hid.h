#ifndef WIIMOTE_HID_H
#define WIIMOTE_HID_H

#include <CommonTypes.h>

#ifdef _MSC_VER
#pragma warning(disable:4200)
#pragma pack(push, 1)
#endif

//source: HID_010_SPC_PFL/1.0 (official HID specification)

struct hid_packet {
	u8 param : 4;
	u8 type : 4;
	u8 data[0];
};

#define HID_TYPE_SET_REPORT 5
#define HID_TYPE_DATA 0xA

#define HID_TYPE_HANDSHAKE 0
#define HID_HANDSHAKE_SUCCESS 0
#define HID_HANDSHAKE_WIIMOTE 8	//custom, reserved in HID specs.

#define HID_PARAM_INPUT 1
#define HID_PARAM_OUTPUT 2

//source: http://wiibrew.org/wiki/Wiimote

struct wm_report {
	u8 channel;
	u8 data[0];
};

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
	u8 : 6;
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
	u8 padding1[2];
	u8 unknown : 1;
	u8 extension : 1;
	u8 speaker : 1;
	u8 ir : 1;
	u8 leds : 4;
	u8 padding2[2];
	u8 battery;
};

#define WM_WRITE_DATA 0x16
struct wm_write_data {
	u8 rumble : 1;
	u8 space : 2;	//see WM_SPACE_*
	u8 : 5;
	u8 address[3];
	u8 size;
	u8 data[16];
};

#define WM_WRITE_DATA_REPLY 0x22	//empty, afaik

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

//filled with 0xFF if empty
struct wm_ir_extended {
	u8 x;
	u8 y;
	u8 size : 4;
	u8 xHi : 2;
	u8 yHi : 2;
};

#define WM_REPORT_CORE 0x30

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
#define WM_REPORT_CORE_IR10_EXT9 0x36
#define WM_REPORT_CORE_ACCEL_IR10_EXT6 0x37
#define WM_REPORT_EXT21 0x3d
#define WM_REPORT_INTERLEAVE1 0x3e
#define WM_REPORT_INTERLEAVE2 0x3f

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#endif	//WIIMOTE_HID_H
