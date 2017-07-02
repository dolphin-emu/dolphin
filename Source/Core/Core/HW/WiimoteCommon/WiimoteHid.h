// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif

#pragma pack(push, 1)

// Source: HID_010_SPC_PFL/1.0 (official HID specification)

struct hid_packet
{
  u8 param : 4;
  u8 type : 4;
  u8 data[0];
};

constexpr u8 HID_TYPE_HANDSHAKE = 0;
constexpr u8 HID_TYPE_SET_REPORT = 5;
constexpr u8 HID_TYPE_DATA = 0xA;

constexpr u8 HID_HANDSHAKE_SUCCESS = 0;

constexpr u8 HID_PARAM_INPUT = 1;
constexpr u8 HID_PARAM_OUTPUT = 2;

#pragma pack(pop)

#ifdef _MSC_VER
#pragma warning(pop)
#endif
