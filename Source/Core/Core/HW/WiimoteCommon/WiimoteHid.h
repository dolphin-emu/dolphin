// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"

namespace WiimoteCommon
{
// Source: HID_010_SPC_PFL/1.0 (official HID specification)

constexpr u8 HID_TYPE_HANDSHAKE = 0;
constexpr u8 HID_TYPE_SET_REPORT = 5;
constexpr u8 HID_TYPE_DATA = 0xA;

constexpr u8 HID_HANDSHAKE_SUCCESS = 0;

constexpr u8 HID_PARAM_INPUT = 1;
constexpr u8 HID_PARAM_OUTPUT = 2;

#ifdef _MSC_VER
#pragma warning(push)
// Disable warning for zero-sized array:
#pragma warning(disable : 4200)
#endif

#pragma pack(push, 1)

struct HIDPacket
{
  static constexpr int HEADER_SIZE = 1;

  u8 param : 4;
  u8 type : 4;

  u8 data[0];
};

template <typename T>
struct TypedHIDInputData
{
  TypedHIDInputData(InputReportID _rpt_id)
      : param(HID_PARAM_INPUT), type(HID_TYPE_DATA), report_id(_rpt_id)
  {
  }

  u8 param : 4;
  u8 type : 4;

  InputReportID report_id;

  T data;

  static_assert(std::is_pod<T>());

  u8* GetData() { return reinterpret_cast<u8*>(this); }
  const u8* GetData() const { return reinterpret_cast<const u8*>(this); }

  constexpr u32 GetSize() const
  {
    static_assert(sizeof(*this) == sizeof(T) + 2);
    return sizeof(*this);
  }
};

#pragma pack(pop)

#ifdef _MSC_VER
#pragma warning(pop)
#endif

}  // namespace WiimoteCommon
