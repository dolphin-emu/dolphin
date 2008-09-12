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

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#endif	//WIIMOTE_HID_H
