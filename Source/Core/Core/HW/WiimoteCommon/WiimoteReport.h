// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "Common/CommonTypes.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif

typedef std::vector<u8> Report;

// Report defines
enum ReportType
{
  RT_RUMBLE = 0x10,
  RT_LEDS = 0x11,
  RT_REPORT_MODE = 0x12,
  RT_IR_PIXEL_CLOCK = 0x13,
  RT_SPEAKER_ENABLE = 0x14,
  RT_REQUEST_STATUS = 0x15,
  RT_WRITE_DATA = 0x16,
  RT_READ_DATA = 0x17,
  RT_WRITE_SPEAKER_DATA = 0x18,
  RT_SPEAKER_MUTE = 0x19,
  RT_IR_LOGIC = 0x1A,
  RT_STATUS_REPORT = 0x20,
  RT_READ_DATA_REPLY = 0x21,
  RT_ACK_DATA = 0x22,
  RT_REPORT_CORE = 0x30,
  RT_REPORT_CORE_ACCEL = 0x31,
  RT_REPORT_CORE_EXT8 = 0x32,
  RT_REPORT_CORE_ACCEL_IR12 = 0x33,
  RT_REPORT_CORE_EXT19 = 0x34,
  RT_REPORT_CORE_ACCEL_EXT16 = 0x35,
  RT_REPORT_CORE_IR10_EXT9 = 0x36,
  RT_REPORT_CORE_ACCEL_IR10_EXT6 = 0x37,
  RT_REPORT_EXT21 = 0x3d,  // never used?
  RT_REPORT_INTERLEAVE1 = 0x3e,
  RT_REPORT_INTERLEAVE2 = 0x3f
};

// Source: http://wiibrew.org/wiki/Wiimote
// Custom structs
#pragma pack(push, 1)
union wm_buttons  // also just called "core data"
{
  u16 hex;

  struct
  {
    u8 left : 1;
    u8 right : 1;
    u8 down : 1;
    u8 up : 1;
    u8 plus : 1;
    u8 acc_x_lsb : 2;  // LSB of accelerometer (10 bits in total)
    u8 unknown : 1;

    u8 two : 1;
    u8 one : 1;
    u8 b : 1;
    u8 a : 1;
    u8 minus : 1;
    u8 acc_y_lsb : 1;  // LSB of accelerometer (9 bits in total)
    u8 acc_z_lsb : 1;  // LSB of accelerometer (9 bits in total)
    u8 home : 1;
  };
};
static_assert(sizeof(wm_buttons) == 2, "Wrong size");

struct wm_accel
{
  u8 x, y, z;
};
static_assert(sizeof(wm_accel) == 3, "Wrong size");

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
static_assert(sizeof(wm_ir_basic) == 5, "Wrong size");

// Three bytes for one object
struct wm_ir_extended
{
  u8 x;
  u8 y;
  u8 size : 4;
  u8 xhi : 2;
  u8 yhi : 2;
};
static_assert(sizeof(wm_ir_extended) == 3, "Wrong size");

// Nunchuk
union wm_nc_core
{
  u8 hex;

  struct
  {
    u8 z : 1;
    u8 c : 1;

    // LSBs of accelerometer
    u8 acc_x_lsb : 2;
    u8 acc_y_lsb : 2;
    u8 acc_z_lsb : 2;
  };
};
static_assert(sizeof(wm_nc_core) == 1, "Wrong size");

union wm_nc
{
  struct
  {
    // joystick x, y
    u8 jx;
    u8 jy;

    // accelerometer
    u8 ax;
    u8 ay;
    u8 az;

    wm_nc_core bt;  // buttons + accelerometer LSBs
  };                // regular data

  struct
  {
    u8 reserved[4];  // jx, jy, ax and ay as in regular case

    u8 extension_connected : 1;
    u8 acc_z : 7;  // MSBs of accelerometer data

    u8 unknown : 1;      // always 0?
    u8 report_type : 1;  // 1: report contains M+ data, 0: report contains extension data

    u8 z : 1;
    u8 c : 1;

    // LSBs of accelerometer - starting from bit 1!
    u8 acc_x_lsb : 1;
    u8 acc_y_lsb : 1;
    u8 acc_z_lsb : 2;
  } passthrough_data;
};
static_assert(sizeof(wm_nc) == 6, "Wrong size");

union wm_classic_extension_buttons
{
  u16 hex;

  struct
  {
    u8 extension_connected : 1;
    u8 rt : 1;  // right trigger
    u8 plus : 1;
    u8 home : 1;
    u8 minus : 1;
    u8 lt : 1;  // left trigger
    u8 dpad_down : 1;
    u8 dpad_right : 1;

    u8 : 2;     // cf. extension_data and passthrough_data
    u8 zr : 1;  // right z button
    u8 x : 1;
    u8 a : 1;
    u8 y : 1;
    u8 b : 1;
    u8 zl : 1;  // left z button
  };            // common data

  // M+ pass-through mode slightly differs from the regular data.
  // Refer to the common data for unnamed fields
  struct
  {
    u8 : 8;

    u8 dpad_up : 1;
    u8 dpad_left : 1;
    u8 : 6;
  } regular_data;

  struct
  {
    u8 : 8;

    u8 unknown : 1;      // always 0?
    u8 report_type : 1;  // 1: report contains M+ data, 0: report contains extension data
    u8 : 6;
  } passthrough_data;
};
static_assert(sizeof(wm_classic_extension_buttons) == 2, "Wrong size");

union wm_classic_extension
{
  // lx/ly/lz; left joystick
  // rx/ry/rz; right joystick
  // lt; left trigger
  // rt; left trigger

  struct
  {
    u8 : 6;
    u8 rx3 : 2;

    u8 : 6;
    u8 rx2 : 2;

    u8 ry : 5;
    u8 lt2 : 2;
    u8 rx1 : 1;

    u8 rt : 5;
    u8 lt1 : 3;

    wm_classic_extension_buttons bt;  // byte 4, 5
  };

  struct
  {
    u8 lx : 6;  // byte 0
    u8 : 2;

    u8 ly : 6;  // byte 1
    u8 : 2;

    unsigned : 32;
  } regular_data;

  struct
  {
    u8 dpad_up : 1;
    u8 lx : 5;  // Bits 1-5
    u8 : 2;

    u8 dpad_left : 1;
    u8 ly : 5;  // Bits 1-5
    u8 : 2;

    unsigned : 32;
  } passthrough_data;
};
static_assert(sizeof(wm_classic_extension) == 6, "Wrong size");

struct wm_guitar_extension
{
  u8 sx : 6;
  u8 pad1 : 2;  // 1 on gh3, 0 on ghwt

  u8 sy : 6;
  u8 pad2 : 2;  // 1 on gh3, 0 on ghwt

  u8 sb : 5;    // not used in gh3
  u8 pad3 : 3;  // always 0

  u8 whammy : 5;
  u8 pad4 : 3;  // always 0

  u16 bt;  // buttons
};
static_assert(sizeof(wm_guitar_extension) == 6, "Wrong size");

struct wm_drums_extension
{
  u8 sx : 6;
  u8 pad1 : 2;  // always 0

  u8 sy : 6;
  u8 pad2 : 2;  // always 0

  u8 pad3 : 1;  // unknown
  u8 which : 5;
  u8 none : 1;
  u8 hhp : 1;

  u8 pad4 : 1;      // unknown
  u8 velocity : 4;  // unknown
  u8 softness : 3;

  u16 bt;  // buttons
};
static_assert(sizeof(wm_drums_extension) == 6, "Wrong size");

struct wm_turntable_extension
{
  u8 sx : 6;
  u8 rtable3 : 2;

  u8 sy : 6;
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
    u16 bt;  // buttons
  };
};
static_assert(sizeof(wm_turntable_extension) == 6, "Wrong size");

struct wm_motionplus_data
{
  // yaw1, roll1, pitch1: Bits 0-7
  // yaw2, roll2, pitch2: Bits 8-13

  u8 yaw1;
  u8 roll1;
  u8 pitch1;

  u8 pitch_slow : 1;
  u8 yaw_slow : 1;
  u8 yaw2 : 6;

  u8 extension_connected : 1;
  u8 roll_slow : 1;
  u8 roll2 : 6;

  u8 zero : 1;
  u8 is_mp_data : 1;
  u8 pitch2 : 6;
};
static_assert(sizeof(wm_motionplus_data) == 6, "Wrong size");

struct wm_report
{
  u8 wm;
  union
  {
    u8 data[0];
    struct
    {
      u8 rumble : 1;  // enable/disable rumble
      // only valid for certain reports
      u8 ack : 1;     // respond with an ack
      u8 enable : 1;  // enable/disable certain features
    };
  };
};
static_assert(sizeof(wm_report) == 2, "Wrong size");

struct wm_leds
{
  u8 rumble : 1;
  // real Wii also sets bit 0x2 (unknown purpose)
  u8 : 3;
  u8 leds : 4;
};
static_assert(sizeof(wm_leds) == 1, "Wrong size");

struct wm_report_mode
{
  u8 rumble : 1;
  // unsure what "all_the_time" actually is, the real Wii does set it (bit 0x2)
  u8 all_the_time : 1;
  u8 continuous : 1;
  u8 : 5;
  u8 mode;
};
static_assert(sizeof(wm_report_mode) == 2, "Wrong size");

struct wm_request_status
{
  u8 rumble : 1;
  u8 : 7;
};
static_assert(sizeof(wm_request_status) == 1, "Wrong size");

struct wm_status_report
{
  wm_buttons buttons;
  u8 battery_low : 1;
  u8 extension : 1;
  u8 speaker : 1;
  u8 ir : 1;
  u8 leds : 4;
  u8 padding2[2];  // two 00, TODO: this needs more investigation
  u8 battery;
};
static_assert(sizeof(wm_status_report) == 6, "Wrong size");

struct wm_write_data
{
  u8 rumble : 1;
  u8 space : 2;  // see WM_SPACE_*
  u8 : 5;
  u8 address[3];
  u8 size;
  u8 data[16];
};
static_assert(sizeof(wm_write_data) == 21, "Wrong size");

struct wm_acknowledge
{
  wm_buttons buttons;
  u8 reportID;
  u8 errorID;
};
static_assert(sizeof(wm_acknowledge) == 4, "Wrong size");

struct wm_read_data
{
  u8 rumble : 1;
  u8 space : 2;  // see WM_SPACE_*
  u8 : 5;
  u8 address[3];
  u16 size;
};
static_assert(sizeof(wm_read_data) == 6, "Wrong size");

struct wm_read_data_reply
{
  wm_buttons buttons;
  u8 error : 4;  // see WM_RDERR_*
  u8 size : 4;
  u16 address;
  u8 data[16];
};
static_assert(sizeof(wm_read_data_reply) == 21, "Wrong size");

// Data reports

struct wm_report_core
{
  wm_buttons c;
};
static_assert(sizeof(wm_report_core) == 2, "Wrong size");

struct wm_report_core_accel
{
  wm_buttons c;
  wm_accel a;
};
static_assert(sizeof(wm_report_core_accel) == 5, "Wrong size");

struct wm_report_core_ext8
{
  wm_buttons c;
  u8 ext[8];
};
static_assert(sizeof(wm_report_core_ext8) == 10, "Wrong size");

struct wm_report_core_accel_ir12
{
  wm_buttons c;
  wm_accel a;
  wm_ir_extended ir[4];
};
static_assert(sizeof(wm_report_core_accel_ir12) == 17, "Wrong size");

struct wm_report_core_accel_ext16
{
  wm_buttons c;
  wm_accel a;
  wm_nc ext;  // TODO: Does this make any sense? Shouldn't it be just a general "extension" field?
  // wm_ir_basic ir[2];
  u8 pad[10];
};
static_assert(sizeof(wm_report_core_accel_ext16) == 21, "Wrong size");

struct wm_report_core_accel_ir10_ext6
{
  wm_buttons c;
  wm_accel a;
  wm_ir_basic ir[2];
  // u8 ext[6];
  wm_nc ext;  // TODO: Does this make any sense? Shouldn't it be just a general "extension" field?
};
static_assert(sizeof(wm_report_core_accel_ir10_ext6) == 21, "Wrong size");

struct wm_report_ext21
{
  u8 ext[21];
};
static_assert(sizeof(wm_report_ext21) == 21, "Wrong size");

struct wm_speaker_data
{
  u8 unknown : 3;
  u8 length : 5;
  u8 data[20];
};
static_assert(sizeof(wm_speaker_data) == 21, "Wrong size");
#pragma pack(pop)

#ifdef _MSC_VER
#pragma warning(pop)
#endif
